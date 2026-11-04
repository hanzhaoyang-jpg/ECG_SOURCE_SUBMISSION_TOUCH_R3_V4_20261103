`timescale 1ns / 1ps

module simple_sync_fifo #(
    parameter integer DATA_WIDTH = 16,
    parameter integer DEPTH      = 1024,
    parameter integer ADDR_WIDTH = 10
)(
    input  wire                     clk,
    input  wire                     rst,
    input  wire                     wr_en,
    input  wire [DATA_WIDTH-1:0]    din,
    output wire                     full,
    input  wire                     rd_en,
    output wire [DATA_WIDTH-1:0]    dout,
    output wire                     empty,
    output reg  [ADDR_WIDTH:0]       count
);

    reg [DATA_WIDTH-1:0] mem [0:DEPTH-1];
    reg [ADDR_WIDTH-1:0] wr_ptr;
    reg [ADDR_WIDTH-1:0] rd_ptr;

    assign full  = (count == DEPTH);
    assign empty = (count == 0);

    /*
     * Fall-through read:
     * dout 总是指向当前 rd_ptr 位置的数据。
     * AXI-Lite 读 FIFO_DATA 时，可以先返回 dout，再 rd_en 弹出。
     */
    assign dout = mem[rd_ptr];

    wire do_write = wr_en && !full;
    wire do_read  = rd_en && !empty;

    always @(posedge clk) begin
        if (rst) begin
            wr_ptr <= {ADDR_WIDTH{1'b0}};
            rd_ptr <= {ADDR_WIDTH{1'b0}};
            count  <= {(ADDR_WIDTH+1){1'b0}};
        end else begin
            if (do_write) begin
                mem[wr_ptr] <= din;
                wr_ptr <= wr_ptr + 1'b1;
            end

            if (do_read) begin
                rd_ptr <= rd_ptr + 1'b1;
            end

            case ({do_write, do_read})
                2'b10: count <= count + 1'b1;
                2'b01: count <= count - 1'b1;
                default: count <= count;
            endcase
        end
    end

endmodule
