`timescale 1ns / 1ps

module ad9280_capture_core #(
    parameter integer FIFO_DEPTH = 1024
)(
    input  wire        clk,
    input  wire        rst,

    input  wire        capture_enable,
    input  wire        fifo_clear,
    input  wire        overflow_clear,

    input  wire [31:0] clk_half_period_cfg,

    input  wire [7:0]  ad_data,
    input  wire        ad_otr,
    input  wire        lod_in,

    output wire        ad_clk_out,

    input  wire        fifo_rd_en,
    output wire [15:0] fifo_rd_data,
    output wire        fifo_empty,
    output wire        fifo_full,
    output wire [10:0] fifo_count,

    output reg         overflow_flag,
    output reg [63:0]  sample_counter,

    output wire        lod_sync_out
);

    wire rst_all;
    assign rst_all = rst | fifo_clear;

    wire sample_tick;
    wire adc_data_tick;

    sample_rate_gen #(
        .SYS_CLK_HZ(100000000),
        .OUTPUT_SAMPLE_HZ(360),
        .DEFAULT_HALF_PERIOD(5)
    ) u_sample_rate_gen (
        .clk             (clk),
        .rst             (rst),
        .enable          (capture_enable),
        .half_period_cfg (clk_half_period_cfg),
        .adc_clk         (ad_clk_out),
        .adc_data_tick   (adc_data_tick),
        .sample_tick     (sample_tick)
    );

    /*
     * Capture the ADC bus at the generated AD_CLK falling edge, about half an
     * ADC period after the conversion edge. The independent 360 Hz strobe
     * later selects this coherent, stable byte for the FIFO.
     */
    reg [7:0] ad_data_sampled;
    reg       ad_otr_sampled;

    always @(posedge clk) begin
        if (rst) begin
            ad_data_sampled <= 8'd0;
            ad_otr_sampled  <= 1'b0;
        end else if (adc_data_tick) begin
            ad_data_sampled <= ad_data;
            ad_otr_sampled  <= ad_otr;
        end
    end

    /*
     * CN121 LOD 同步。
     * CN121 数据手册说明 LOD 高电平表示 lead-off。
     */
    (* ASYNC_REG = "TRUE" *) reg lod_meta;
    (* ASYNC_REG = "TRUE" *) reg lod_sync;

    always @(posedge clk) begin
        if (rst) begin
            lod_meta <= 1'b0;
            lod_sync <= 1'b0;
        end else begin
            lod_meta <= lod_in;
            lod_sync <= lod_meta;
        end
    end

    assign lod_sync_out = lod_sync;

    wire [15:0] fifo_wr_data;
    assign fifo_wr_data = {
        ad_otr_sampled,  // bit15: AD9280 OTR
        lod_sync,        // bit14: CN121 LOD
        6'd0,            // bit13:8 reserved
        ad_data_sampled  // bit7:0 ADC sample
    };

    wire fifo_wr_en;
    assign fifo_wr_en = sample_tick & capture_enable & ~fifo_full;

    wire overflow_event;
    assign overflow_event = sample_tick & capture_enable & fifo_full;

    wire [15:0] fifo_dout;
    wire [10:0] fifo_count_int;

    simple_sync_fifo #(
        .DATA_WIDTH(16),
        .DEPTH(FIFO_DEPTH),
        .ADDR_WIDTH(10)
    ) u_sample_fifo (
        .clk    (clk),
        .rst    (rst_all),

        .wr_en  (fifo_wr_en),
        .din    (fifo_wr_data),
        .full   (fifo_full),

        .rd_en  (fifo_rd_en),
        .dout   (fifo_dout),
        .empty  (fifo_empty),

        .count  (fifo_count_int)
    );

    assign fifo_rd_data = fifo_dout;
    assign fifo_count   = fifo_count_int;

    always @(posedge clk) begin
        if (rst) begin
            overflow_flag  <= 1'b0;
            sample_counter <= 64'd0;
        end else begin
            if (overflow_clear) begin
                overflow_flag <= 1'b0;
            end else if (overflow_event) begin
                overflow_flag <= 1'b1;
            end

            if (fifo_clear) begin
                sample_counter <= 64'd0;
            end else if (sample_tick && capture_enable) begin
                sample_counter <= sample_counter + 64'd1;
            end
        end
    end

endmodule
