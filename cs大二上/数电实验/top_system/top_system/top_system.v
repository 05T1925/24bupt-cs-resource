module segment_decoder (
    input  wire [3:0] data_in,  // 8421 BCD码输入
    output reg  [7:0] seg_out 
    );  // 8位输出: [7]是DP, [6:0]是g,f,e,d,c,b,a

    always @(*) begin
        case (data_in)
            //                     DP gfedcba
            4'b0000: seg_out = 8'b0_1111110; // 显示 0
            4'b0001: seg_out = 8'b0_0110000; // 显示 1
            4'b0010: seg_out = 8'b0_1101101; // 显示 2
            4'b0011: seg_out = 8'b0_1111001; // 显示 3
            4'b0100: seg_out = 8'b0_0110011; // 显示 4
            4'b0101: seg_out = 8'b0_1011011; // 显示 5
            4'b0110: seg_out = 8'b0_1011111; // 显示 6
            4'b0111: seg_out = 8'b0_1110000; // 显示 7
            4'b1000: seg_out = 8'b0_1111111; // 显示 8
            4'b1001: seg_out = 8'b0_1111011; // 显示 9
            // 非法输入，全部熄灭
            default: seg_out = 8'b0_1111110;
        endcase
    end

endmodule
module bcd_counter (
    input wire clk,      
    input wire rst_n,    
    output reg [3:0] q   
);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            q <= 4'b0000;      
        end
        else begin
            if (q == 4'b1001)  
                q <= 4'b0000;
            else
                q <= q + 4'b0001;
        end
    end

endmodule
module top_system (
    input wire clk,           // 对应硬件 Pin 60 (QD)
    input wire rst_n,         // 对应硬件 Pin 1 (CLR)
   

    output wire [7:0] hw_seg_out
);


    wire [3:0] bcd_code;


    bcd_counter u_counter (
        .clk   (clk),
        .rst_n (rst_n),
        .q     (bcd_code)
    );


    segment_decoder u_decoder (
        .data_in (bcd_code),
        .seg_out (hw_seg_out)
    );
   
endmodule 