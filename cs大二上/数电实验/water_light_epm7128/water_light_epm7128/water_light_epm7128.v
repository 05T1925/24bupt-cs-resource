module water_light_epm7128 (
    input  wire clk,
    input  wire rst_n,
    output reg  [3:0] led
);

    parameter CNT_LIMIT = 20'd1_000_000;

    reg [19:0] cnt;
    wire tick;
    reg [1:0] state;

    // 计数器：产生 1 秒脉冲
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            cnt <= 0;
        else
            cnt <= (cnt == CNT_LIMIT - 1) ? 0 : cnt + 1'b1;
    end
    assign tick = (cnt == CNT_LIMIT - 1);

    // 状态机：每秒递进
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            state <= 0;
        else if (tick)
            state <= state + 1'b1;
    end

    // LED 输出
    always @(*) begin
        case (state)
            2'd0: led = 4'b0001;
            2'd1: led = 4'b0010;
            2'd2: led = 4'b0100;
            2'd3: led = 4'b1000;
            default: led = 4'b0001;
        endcase
    end

endmodule