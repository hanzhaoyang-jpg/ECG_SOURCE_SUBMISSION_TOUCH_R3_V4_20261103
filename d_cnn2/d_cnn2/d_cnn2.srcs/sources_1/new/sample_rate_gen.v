`timescale 1ns / 1ps

module sample_rate_gen #(
    parameter integer SYS_CLK_HZ          = 100000000,
    parameter integer OUTPUT_SAMPLE_HZ    = 360,
    parameter integer DEFAULT_HALF_PERIOD = 5
)(
    input  wire        clk,
    input  wire        rst,

    input  wire        enable,

    input  wire [31:0] half_period_cfg,

    output reg         adc_clk,
    output reg         adc_data_tick,
    output reg         sample_tick
);

    reg [31:0] adc_clk_cnt;
    reg [31:0] sample_phase;
    wire [31:0] half_period;
    localparam [31:0] SAMPLE_PHASE_STEP =
        OUTPUT_SAMPLE_HZ;
    localparam [31:0] SAMPLE_PHASE_LIMIT =
        SYS_CLK_HZ - OUTPUT_SAMPLE_HZ;

    assign half_period = (half_period_cfg < 32'd2) ?
                         DEFAULT_HALF_PERIOD[31:0] :
                         half_period_cfg;

    always @(posedge clk) begin
        if (rst) begin
            adc_clk_cnt <= 32'd0;
            sample_phase <= 32'd0;
            adc_clk     <= 1'b0;
            adc_data_tick <= 1'b0;
            sample_tick <= 1'b0;
        end else begin
            adc_data_tick <= 1'b0;
            sample_tick <= 1'b0;

            /*
             * Keep AD9280 running continuously at a valid high-speed clock.
             * With a 100 MHz system clock and half_period=5, AD_CLK is 10 MHz.
             */
            if (adc_clk_cnt >= half_period - 1) begin
                adc_clk_cnt <= 32'd0;
                adc_clk <= ~adc_clk;
                if (adc_clk == 1'b1) begin
                    adc_data_tick <= 1'b1;
                end
            end else begin
                adc_clk_cnt <= adc_clk_cnt + 1'b1;
            end

            /*
             * Fractional divider for the ECG stream rate. This strobe selects
             * the latest AD9280 output for the FIFO without slowing the ADC.
             */
            if (!enable) begin
                sample_phase <= 32'd0;
            end else if (sample_phase >= SAMPLE_PHASE_LIMIT) begin
                sample_phase <= sample_phase - SAMPLE_PHASE_LIMIT;
                sample_tick <= 1'b1;
            end else begin
                sample_phase <= sample_phase + SAMPLE_PHASE_STEP;
            end
        end
    end

endmodule
