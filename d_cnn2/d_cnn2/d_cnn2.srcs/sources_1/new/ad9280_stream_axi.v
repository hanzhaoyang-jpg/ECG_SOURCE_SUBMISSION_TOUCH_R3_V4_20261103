//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2026/05/24 17:08:20
// Design Name: 
// Module Name: ad9280_stream_axi
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


`timescale 1ns / 1ps

module ad9280_stream_axi #(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 6,
    parameter integer FIFO_DEPTH         = 1024
)(
    /*
     * AD9280 / CN121 external pins
     */
    input  wire [7:0]  ad_data,
    input  wire        ad_otr,
    output wire        ad_clk_out,
    input  wire        lod_in,

    /*
     * AXI-Lite slave interface
     */
    input  wire                              s_axi_aclk,
    input  wire                              s_axi_aresetn,

    input  wire [C_S_AXI_ADDR_WIDTH-1:0]     s_axi_awaddr,
    input  wire [2:0]                        s_axi_awprot,
    input  wire                              s_axi_awvalid,
    output reg                               s_axi_awready,

    input  wire [C_S_AXI_DATA_WIDTH-1:0]     s_axi_wdata,
    input  wire [(C_S_AXI_DATA_WIDTH/8)-1:0] s_axi_wstrb,
    input  wire                              s_axi_wvalid,
    output reg                               s_axi_wready,

    output reg  [1:0]                        s_axi_bresp,
    output reg                               s_axi_bvalid,
    input  wire                              s_axi_bready,

    input  wire [C_S_AXI_ADDR_WIDTH-1:0]     s_axi_araddr,
    input  wire [2:0]                        s_axi_arprot,
    input  wire                              s_axi_arvalid,
    output reg                               s_axi_arready,

    output reg  [C_S_AXI_DATA_WIDTH-1:0]     s_axi_rdata,
    output reg  [1:0]                        s_axi_rresp,
    output reg                               s_axi_rvalid,
    input  wire                              s_axi_rready
);

    localparam integer ADDR_LSB = 2;

    /*
     * Register map:
     *
     * 0x00 CONTROL
     *      bit0  capture_enable
     *      bit1  fifo_clear pulse, write 1 to clear
     *      bit2  overflow_clear pulse, write 1 to clear
     *
     * 0x04 STATUS
     *      bit0  running/capture_enable
     *      bit1  fifo_empty
     *      bit2  fifo_full
     *      bit3  overflow_flag
     *      bit4  lod
     *
     * 0x08 FIFO_COUNT
     *      bit[15:0] FIFO sample count
     *
     * 0x0C FIFO_DATA
     *      read pops one sample if FIFO not empty
     *      bit31 valid
     *      bit15 AD9280 OTR
     *      bit14 CN121 LOD
     *      bit7:0 ADC data
     *
     * 0x10 SAMPLE_COUNTER_L
     * 0x14 SAMPLE_COUNTER_H
     *
     * 0x18 CLK_HALF_PERIOD
     *      AD_CLK half period in s_axi_aclk cycles.
     *      If s_axi_aclk = 100 MHz and half_period = 5,
     *      AD_CLK = 10 MHz. FIFO output remains fixed at 360 Hz.
     *
     * 0x1C DEBUG
     */

    localparam [5:0] REG_CONTROL          = 6'h00;
    localparam [5:0] REG_STATUS           = 6'h04;
    localparam [5:0] REG_FIFO_COUNT       = 6'h08;
    localparam [5:0] REG_FIFO_DATA        = 6'h0C;
    localparam [5:0] REG_SAMPLE_COUNTER_L = 6'h10;
    localparam [5:0] REG_SAMPLE_COUNTER_H = 6'h14;
    localparam [5:0] REG_CLK_HALF_PERIOD  = 6'h18;
    localparam [5:0] REG_DEBUG            = 6'h1C;

    wire axi_reset;
    assign axi_reset = ~s_axi_aresetn;

    reg        capture_enable;
    reg [31:0] clk_half_period_cfg;
    reg [31:0] debug_reg;

    reg fifo_clear_pulse;
    reg overflow_clear_pulse;

    wire [15:0] fifo_rd_data;
    wire        fifo_empty;
    wire        fifo_full;
    wire [10:0] fifo_count;
    wire        overflow_flag;
    wire [63:0] sample_counter;
    wire        lod_sync_out;

    reg fifo_rd_en;

    ad9280_capture_core #(
        .FIFO_DEPTH(FIFO_DEPTH)
    ) u_capture_core (
        .clk                 (s_axi_aclk),
        .rst                 (axi_reset),

        .capture_enable      (capture_enable),
        .fifo_clear          (fifo_clear_pulse),
        .overflow_clear      (overflow_clear_pulse),

        .clk_half_period_cfg (clk_half_period_cfg),

        .ad_data             (ad_data),
        .ad_otr              (ad_otr),
        .lod_in              (lod_in),

        .ad_clk_out          (ad_clk_out),

        .fifo_rd_en          (fifo_rd_en),
        .fifo_rd_data        (fifo_rd_data),
        .fifo_empty          (fifo_empty),
        .fifo_full           (fifo_full),
        .fifo_count          (fifo_count),

        .overflow_flag       (overflow_flag),
        .sample_counter      (sample_counter),

        .lod_sync_out        (lod_sync_out)
    );

    /*
     * AXI write channel
     */
    wire aw_hs;
    wire w_hs;

    assign aw_hs = s_axi_awvalid && s_axi_awready;
    assign w_hs  = s_axi_wvalid  && s_axi_wready;

    reg [C_S_AXI_ADDR_WIDTH-1:0] awaddr_latched;

    always @(posedge s_axi_aclk) begin
        if (axi_reset) begin
            s_axi_awready <= 1'b0;
            s_axi_wready  <= 1'b0;
            s_axi_bvalid  <= 1'b0;
            s_axi_bresp   <= 2'b00;
            awaddr_latched <= {C_S_AXI_ADDR_WIDTH{1'b0}};

            capture_enable      <= 1'b0;
            clk_half_period_cfg <= 32'd5;
            debug_reg           <= 32'd0;

            fifo_clear_pulse     <= 1'b0;
            overflow_clear_pulse <= 1'b0;
        end else begin
            fifo_clear_pulse     <= 1'b0;
            overflow_clear_pulse <= 1'b0;

            if (!s_axi_awready && s_axi_awvalid && !s_axi_bvalid) begin
                s_axi_awready <= 1'b1;
                awaddr_latched <= s_axi_awaddr;
            end else begin
                s_axi_awready <= 1'b0;
            end

            if (!s_axi_wready && s_axi_wvalid && !s_axi_bvalid) begin
                s_axi_wready <= 1'b1;
            end else begin
                s_axi_wready <= 1'b0;
            end

            if (aw_hs && w_hs) begin
                case (awaddr_latched[5:0])
                    REG_CONTROL: begin
                        if (s_axi_wstrb[0]) begin
                            capture_enable <= s_axi_wdata[0];

                            if (s_axi_wdata[1])
                                fifo_clear_pulse <= 1'b1;

                            if (s_axi_wdata[2])
                                overflow_clear_pulse <= 1'b1;
                        end
                    end

                    REG_CLK_HALF_PERIOD: begin
                        clk_half_period_cfg <= s_axi_wdata;
                    end

                    REG_DEBUG: begin
                        debug_reg <= s_axi_wdata;
                    end

                    default: begin
                    end
                endcase

                s_axi_bvalid <= 1'b1;
                s_axi_bresp  <= 2'b00;
            end else if (s_axi_bvalid && s_axi_bready) begin
                s_axi_bvalid <= 1'b0;
            end
        end
    end

    /*
     * AXI read channel
     */
    wire ar_hs;
    assign ar_hs = s_axi_arvalid && s_axi_arready;

    reg [C_S_AXI_ADDR_WIDTH-1:0] araddr_latched;

    reg [31:0] read_data_next;

    always @(*) begin
        read_data_next = 32'd0;

        case (araddr_latched[5:0])
            REG_CONTROL: begin
                read_data_next = {
                    29'd0,
                    1'b0,
                    1'b0,
                    capture_enable
                };
            end

            REG_STATUS: begin
                read_data_next = {
                    27'd0,
                    lod_sync_out,
                    overflow_flag,
                    fifo_full,
                    fifo_empty,
                    capture_enable
                };
            end

            REG_FIFO_COUNT: begin
                read_data_next = {21'd0, fifo_count};
            end

            REG_FIFO_DATA: begin
                /*
                 * bit31: valid
                 * bit15: AD9280 OTR
                 * bit14: CN121 LOD
                 * bit7:0 ADC data
                 */
                read_data_next = fifo_empty ?
                                 32'd0 :
                                 {1'b1, 15'd0, fifo_rd_data};
            end

            REG_SAMPLE_COUNTER_L: begin
                read_data_next = sample_counter[31:0];
            end

            REG_SAMPLE_COUNTER_H: begin
                read_data_next = sample_counter[63:32];
            end

            REG_CLK_HALF_PERIOD: begin
                read_data_next = clk_half_period_cfg;
            end

            REG_DEBUG: begin
                read_data_next = debug_reg;
            end

            default: begin
                read_data_next = 32'd0;
            end
        endcase
    end

    always @(posedge s_axi_aclk) begin
        if (axi_reset) begin
            s_axi_arready <= 1'b0;
            s_axi_rvalid  <= 1'b0;
            s_axi_rresp   <= 2'b00;
            s_axi_rdata   <= 32'd0;
            araddr_latched <= {C_S_AXI_ADDR_WIDTH{1'b0}};
            fifo_rd_en    <= 1'b0;
        end else begin
            fifo_rd_en <= 1'b0;

            if (!s_axi_arready && s_axi_arvalid && !s_axi_rvalid) begin
                s_axi_arready <= 1'b1;
                araddr_latched <= s_axi_araddr;
            end else begin
                s_axi_arready <= 1'b0;
            end

            if (ar_hs) begin
                /*
                 * FIFO uses fall-through read: return the current head word,
                 * then advance the read pointer with fifo_rd_en.
                 */
                if (s_axi_araddr[5:0] == REG_FIFO_DATA && !fifo_empty) begin
                    fifo_rd_en <= 1'b1;
                end

                s_axi_rdata  <= read_data_next;
                s_axi_rresp  <= 2'b00;
                s_axi_rvalid <= 1'b1;
            end else if (s_axi_rvalid && s_axi_rready) begin
                s_axi_rvalid <= 1'b0;
            end
        end
    end

endmodule
