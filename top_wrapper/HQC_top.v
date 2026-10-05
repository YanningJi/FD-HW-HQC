`timescale 1ns / 1ps
module HQC_top(
    input clk,
    input load_i,
    input [63:0] key_i,  // key
    //input [511:0] data0_i, data1_i,  // textin, cipherin (sk_seed, pk_seed / m_in, _ / d,_ / u_0, u_1 / v_0, v_1 /)
    input [511:0] data0_i, data1_i,  //can only send !less than! 48=3*16 bytes due to chipwhisperer limitation // textin, cipherin (sk_seed, pk_seed / m_in, _ / d,_ / u_0, u_1 / v_0, v_1 /)
    output reg [511:0] data_o,  // cipherout
    output reg busy_o
);

parameter WEIGHT = 66;
parameter N = 17_669;
parameter MEM_WIDTH = 128;
parameter N_MEM = N + (MEM_WIDTH - N%MEM_WIDTH)%MEM_WIDTH;


localparam state_stall = 5'h0,
        state_reset = 5'h01,

        state_wrseed_start = 5'h02,
        state_wrseed = 5'h03,
        state_keygen_start = 5'h04,
        state_keygen_wait = 5'h05,
        state_read_y = 5'h06,
        state_read_h = 5'h07,
        state_read_s = 5'h08,
        
        state_wrmsg_start = 5'h09,
        state_wrmsg = 5'h0a,
        state_encap_start = 5'h0b,
        state_encap_stall = 5'h1a,
        state_encap_wait = 5'h0c,
        state_read_ss = 5'h0d,
        state_read_d = 5'h0e,
        state_read_u = 5'h0f,
        state_read_v = 5'h10,

        state_wr_d_start = 5'h11,
        state_wr_d = 5'h12,
        state_decap_start = 5'h13,
        state_decap_wait = 5'h14,
        state_read_ss_decap = 5'h15,
        
        state_wr_u = 5'h16,
        state_wr_u_done = 5'h17,
        
        state_wr_v = 5'h18,
        state_wr_v_done = 5'h19,
        
        state_reset_wait = 5'h1b;
  
        
reg rst;
reg [4:0] state;
reg start, sk_seed_wen, pk_seed_wen, keygen_out_en, m_wen, encap_out_en, decap_in_wen, decap_out_en;
reg [1:0] op;
wire [1:0] keygen_out_type, encap_out_type;
wire done;
reg [9:0] counter, read_counter, u_counter, v_counter, d_counter, counter_delay;

wire [3:0] sk_seed_addr, pk_seed_addr;
wire [2:0] m_addr;
reg [31:0] sk_seed, pk_seed, m_in;
wire [8:0] keygen_out_addr, encap_out_addr, decap_out_addr;
reg [8:0] decap_in_addr;
wire [127:0] keygen_out, encap_out, decap_out;
wire [127:0] h_0, h_1, s_0, s_1, u_0, u_1, v_0, v_1, decap_in;
wire [9:0] h_addr_0, h_addr_1, s_addr_0, s_addr_1, u_addr_0, u_addr_1, v_addr_0, v_addr_1;
wire [15:0] y;
reg [7:0] addr_delay;
wire [7:0] y_addr;

reg [1:0] read_state;
reg read_done, read_start;
wire flag_read_done, flag_d_in;
localparam read_state_stall = 0,
        read_state_run = 2'd1,
        read_state_wait = 2'd2;

wire [127:0] hmem_din0;
wire [7:0] hmem_addr0, hmem_addr1;
wire  hmem_wen0;
mem_dual #(.WIDTH(128), .DEPTH(256)) H_MEM (
    .clock(clk),
    .data_0(hmem_din0),
    .data_1(128'b0),
    .address_0(hmem_addr0),
    .address_1(hmem_addr1),
    .wren_0(hmem_wen0),
    .wren_1(1'b0),
    .q_0(h_0),
    .q_1(h_1)
  );

wire [127:0] smem_din0;
wire [7:0] smem_addr0, smem_addr1;
wire  smem_wen0;
mem_dual #(.WIDTH(128), .DEPTH(256)) S_MEM (
    .clock(clk),
    .data_0(smem_din0),
    .data_1(128'b0),
    .address_0(smem_addr0),
    .address_1(smem_addr1),
    .wren_0(smem_wen0),
    .wren_1(1'b0),
    .q_0(s_0),
    .q_1(s_1)
  );

reg [127:0] umem_din0_reg, umem_din1_reg;
wire [127:0] umem_din0, umem_din1;
wire [7:0] umem_addr0, umem_addr1;
wire  umem_wen0, umem_wen1;
mem_dual #(.WIDTH(128), .DEPTH(256)) U_MEM (
    .clock(clk),
    .data_0(umem_din0),
    .data_1(umem_din1),
    .address_0(umem_addr0),
    .address_1(umem_addr1),
    .wren_0(umem_wen0),
    .wren_1(umem_wen1),
    .q_0(u_0),
    .q_1(u_1)
  );

reg [127:0] vmem_din0_reg, vmem_din1_reg;
wire [127:0] vmem_din0, vmem_din1;
wire [7:0] vmem_addr0, vmem_addr1;
wire  vmem_wen0, vmem_wen1;
mem_dual #(.WIDTH(128), .DEPTH(256)) V_MEM (
    .clock(clk),
    .data_0(vmem_din0),
    .data_1(vmem_din1),
    .address_0(vmem_addr0),
    .address_1(vmem_addr1),
    .wren_0(vmem_wen0),
    .wren_1(vmem_wen1),
    .q_0(v_0),
    .q_1(v_1)
  );

wire [31:0] ssmem_din;
wire [4:0] ssmem_addr;
wire  ssmem_wen;
wire [31:0] ss;
mem_single #(.WIDTH(32), .DEPTH(32)) SS_MEM (
    .clock(clk),
    .data(ssmem_din),
    .address(ssmem_addr),
    .wr_en(ssmem_wen),
    .q(ss)
  );

reg dmem_din_reg;
wire [31:0] dmem_din;
wire [3:0] dmem_addr;
wire  dmem_wen;
mem_single #(.WIDTH(32), .DEPTH(16)) D_MEM (
    .clock(clk),
    .data(dmem_din),
    .address(dmem_addr),
    .wr_en(dmem_wen),
    .q(decap_in)
  );

wire [15:0] ymem_din;
wire [7:0] ymem_addr;
wire  ymem_wen;
mem_single #(.WIDTH(16), .DEPTH(WEIGHT)) Y_MEM (
    .clock(clk),
    .data(ymem_din),
    .address(ymem_addr),
    .wr_en(ymem_wen),
    .q(y)
  );

wire [31:0] d_min;
wire [1:0] d_maddr;
wire  d_mwen;
  
hqc_kem_joint_design #(.parameter_set("hqc128"), .CT_DESIGN(2'b01), .PARALLEL_ENCRYPT(1'b1)) UUT
(
    .clk(clk),
    .rst(rst),
	.operation(op), // KEYGEN operation = 2'b00, ENCAP  operation = 2'b01, DECAP operation  = 2'b10; load things op = 2'b11
	.start(start),
	.done(done),
	// keygen ports
	.sk_seed_addr(sk_seed_addr),
    .sk_seed(sk_seed),
	.sk_seed_wen(sk_seed_wen),
	.pk_seed_addr(pk_seed_addr),
    .pk_seed(pk_seed),
	.pk_seed_wen(pk_seed_wen),	
	.keygen_out_type(keygen_out_type), // 00 - X, 01 - Y, 10 - vect_set_Random(h), 11 - S
	.keygen_out_en(keygen_out_en),	
	.keygen_out_addr(keygen_out_addr),	
	.keygen_out(keygen_out),
	// encap ports	
    .m_in(m_in),
	.m_addr(m_addr),
	.m_wen(m_wen),
    .encap_out_type(encap_out_type),    // 00 - session key, 01 - salt(d), 10 - u, 11 - v
    .encap_out_en(encap_out_en),
    .encap_out_addr(encap_out_addr),
    .encap_out(encap_out),
    // decap and encap ports
    .h_0(h_0),
	.h_1(h_1),
	//.h_0({128{1'b1}}),
	//.h_1({128{1'b1}}),	
	.h_addr_0(h_addr_0),
	.h_addr_1(h_addr_1),
    .s_0(s_0),
	.s_1(s_1),
	//.s_0({128{1'b1}}),
	//.s_1({128{1'b1}}),	
	.s_addr_0(s_addr_0),
	.s_addr_1(s_addr_1),
    // decap ports
    .decap_in_type(2'b01),  // 01 - d
    .decap_in(decap_in),
	.decap_in_addr(decap_in_addr),
	.decap_in_wen(decap_in_wen),
	.y_addr(y_addr),
	.y(y),
	//.y({128{1'b1}}),
	.u_0(u_0),
	.u_1(u_1),
	.u_addr_0(u_addr_0),
	.u_addr_1(u_addr_1),
	.v_0(v_0),
	.v_1(v_1),	
	.v_addr_0(v_addr_0),
	.v_addr_1(v_addr_1),
    .decap_out_en(decap_out_en),
    .decap_out_addr(decap_out_addr),
    .decap_out(decap_out),
    
    .e_m_in(d_min),
    .e_m_addr(d_maddr),
    .e_m_wen(d_mwen)
);

assign hmem_din0 = keygen_out;
assign smem_din0 = keygen_out;
assign hmem_addr0 = (op == 2'b0)? addr_delay : h_addr_0;
assign hmem_addr1 = h_addr_1;
assign smem_addr0 = (op == 2'b0)? addr_delay : s_addr_0;
assign smem_addr1 = s_addr_1;
assign ymem_din = keygen_out;
assign ymem_addr = (op == 2'b10)? y_addr : addr_delay;
assign ymem_wen = (read_state != read_state_stall & state == state_read_y)? 1 : 0;
assign hmem_wen0 = (read_state != read_state_stall & state == state_read_h)? 1 : 0;
assign smem_wen0 = (read_state != read_state_stall & state == state_read_s)? 1 : 0;

assign umem_din0 = (op == 2'b11)? umem_din0_reg : encap_out;
assign umem_din1 = umem_din1_reg;
assign vmem_din0 = (op == 2'b11)? vmem_din0_reg : encap_out;
assign vmem_din1 = vmem_din1_reg;
assign umem_addr0 = (op == 2'b1)? addr_delay : 
                (op == 2'b11)? counter_delay+u_counter*10'd8 : u_addr_0;
assign umem_addr1 = (op == 2'b11)? counter_delay+u_counter*10'd8+10'd4 : u_addr_1;
assign vmem_addr0 = (op == 2'b1)? addr_delay : 
                (op == 2'b11)? counter_delay+v_counter*10'd8 : v_addr_0;
assign vmem_addr1 = (op == 2'b11)? counter_delay+v_counter*10'd8+10'd4 : v_addr_1;

assign umem_wen0 = (read_state != read_state_stall & state == state_read_u)? 1 : 
                (state == state_wr_u || state == state_wr_u_done)? 1 : 0;
assign umem_wen1 = (state == state_wr_u || state == state_wr_u_done)? 1 : 0;
assign vmem_wen0 = (read_state != read_state_stall & state == state_read_v)? 1 : 
                (state == state_wr_v || state == state_wr_v_done)? 1 : 0;
assign vmem_wen1 = (state == state_wr_v || state == state_wr_v_done)? 1 : 0;

assign ssmem_din = (op == 2'b10)? decap_out : encap_out;
assign ssmem_addr = (op == 2'b10)? addr_delay + 5'd16 : addr_delay;
assign ssmem_wen = ((read_state != read_state_stall & state == state_read_ss) | 
                (read_state != read_state_stall & state == state_read_ss_decap))? 1 : 0;
assign dmem_din = (flag_d_in)? data0_i[counter*32+31 -: 32] : encap_out;
assign dmem_addr = (op == 2'b10)? counter : addr_delay;
assign dmem_wen = ((read_state != read_state_stall & state == state_read_d) |
                (flag_d_in))? 1 : 0;
assign flag_d_in = (key_i[63:0] == 64'h00000000123c0de6 & 
                (state == state_wr_d_start | state == state_wr_d) & counter < 10'd16)? 1 : 0;

assign sk_seed_addr = counter;
assign pk_seed_addr = counter;
assign keygen_out_type = (state == state_read_y)? 2'b1 : 
                (state == state_read_h)? 2'b10 :
                (state == state_read_s)? 2'b11 : 0;
assign keygen_out_addr = read_counter;

assign m_addr = counter;
assign encap_out_type = (state == state_read_d)? 2'b1 :
                (state == state_read_u)? 2'b10 :
                (state == state_read_v)? 2'b11 : 0;
assign encap_out_addr = read_counter;

assign decap_out_addr = read_counter;

assign flag_read_done = ((state == state_read_y & read_counter >= WEIGHT) | 
                (state == state_read_h & read_counter >= N_MEM/MEM_WIDTH) |
                (state == state_read_s & read_counter >= N_MEM/MEM_WIDTH) |
                (state == state_read_ss & read_counter >= 10'd16) |
                (state == state_read_d & read_counter >= 10'd16) |
                (state == state_read_u & read_counter >= N_MEM/MEM_WIDTH) |
                (state == state_read_v & read_counter >= N_MEM/MEM_WIDTH) | 
                (state == state_read_ss_decap & read_counter >= 10'd16))? 1 : 0;

always @(posedge clk) begin
    case(read_state)
        read_state_stall: begin
            read_counter <= 0;
            read_done <= 0;
            if (read_start) read_state <= read_state_run;
            else read_state <= read_state_stall;
        end
        read_state_run: begin
            addr_delay <= read_counter;
            if (flag_read_done) read_state <= read_state_wait;
            else begin 
                read_counter <= read_counter + 10'd1;
                read_state <= read_state_run;
            end
        end
        read_state_wait: begin
            read_state <= read_state_stall;
            read_done <= 1;
        end
        default: read_state <= read_state_stall;
    endcase
end

always @(posedge clk) counter_delay <= counter;

always @(posedge clk) begin
    if (load_i) begin
        case(key_i[63:0])
            64'h00000000123c0de0: begin state <= state_reset; op <= 2'b0; end   // reset all
            64'h00000000123c0de1: begin state <= state_wrseed_start; op <= 2'b0; end    // read seed from computer & start keygen
            64'h00000000123c0de2: begin state <= state_wrmsg_start; op <= 2'b1; end   // read msg from computer & start encap
            64'h00000000123c0de3: begin state <= state_wr_d_start; op <= 2'b10; end   // start decap
            64'h00000000123c0de4: begin state <= state_wr_u; op <= 2'b11; end   // read u from computer
            64'h00000000123c0de5: begin state <= state_wr_v; op <= 2'b11; end   // read v from computer
            64'h00000000123c0de6: begin state <= state_wr_d_start; op <= 2'b10; end   // read d from computer if data0_i != 0 & start decap
            default: begin state <= state_stall; op <= 2'b0; end  // do nothing
        endcase
        busy_o <= 1'b1;
    end
    
    else begin
        case(state)
            state_wrseed_start: begin
                data_o <= 0;
                state <= state_wrseed;
                sk_seed_wen <= 1;
                pk_seed_wen <= 1;
                counter <= 0;
                sk_seed <= data0_i[31:0];
                //sk_seed <= 0;
                pk_seed <= data1_i[31:0];
                keygen_out_en <= 0;
            end
            state_wrseed: begin
                if (counter >= 10'd9) begin 
                    state <= state_keygen_start; counter <= 0; 
                    sk_seed_wen <= 0; pk_seed_wen <= 0;
                end
                else begin 
                    counter <= counter + 10'd1;
                    sk_seed <= data0_i[counter*32+63 -: 32];
                    pk_seed <= data1_i[counter*32+63 -: 32];
                end
            end
            state_keygen_start: begin
                start <= 1'b1; state <= state_keygen_wait;
            end
            state_keygen_wait: begin
                start <= 0;
                if (done) begin state <= state_read_y; read_start <= 1; keygen_out_en <= 1; end
                else state <= state_keygen_wait;
            end
            state_read_y: begin
                if (read_done) begin state <= state_read_h; read_start <= 1; end
                else begin state <= state_read_y; read_start <= 0; end
                if (read_counter == 10'd1) data_o[271:256] <= keygen_out[15:0];
            end
            state_read_h: begin
                if (read_done) begin state <= state_read_s; read_start <= 1; end //data_o[415:384] <= pk_seed; end
                else begin state <= state_read_h; read_start <= 0; end
                if (read_counter == 10'd1) data_o[127:0] <= keygen_out;
            end
            state_read_s: begin
                read_start <= 0;
                if (read_done) begin state <= state_reset; keygen_out_en <= 0; end //data_o[447:416] <= sk_seed; end 
                else state <= state_read_s;
                if (read_counter == 10'd1) data_o[255:128] <= keygen_out;
            end

            state_wrmsg_start: begin
                data_o <= 0;
                state <= state_wrmsg;
                counter <= 0;
                m_wen <= 1;
                m_in <= data0_i[31:0];
                encap_out_en <= 0;
            end
            state_wrmsg: begin
                if (counter >= 10'd3) begin state <= state_encap_start; m_wen <= 0; counter <= 0; end
                else begin state <= state_wrmsg; m_in <= data0_i[counter*32+63 -: 32];
                counter <= counter + 10'd1; end
            end
            state_encap_start: begin
                start <= 1'b1; state <= state_encap_stall;
            end
            state_encap_stall: begin
                state <= state_encap_wait;
            end
            state_encap_wait: begin
                start <= 0;
                if (done) begin state <= state_read_ss; read_start <= 1; encap_out_en <= 1; end
                else state <= state_encap_wait;
            end
            state_read_ss: begin
                if (read_done) begin state <= state_read_d; read_start <= 1; end
                else begin state <= state_read_ss; read_start <= 0; end
                data_o[addr_delay*32+31 -: 32] <= encap_out;
            end
            state_read_d: begin
                if (read_done) begin state <= state_read_u; read_start <= 1; end
                else begin state <= state_read_d; read_start <= 0; end
            end
            state_read_u: begin
                if (read_done) begin state <= state_read_v; read_start <= 1; end
                else begin state <= state_read_u; read_start <= 0; end
            end
            state_read_v: begin
                read_start <= 0;
                if (read_done) begin state <= state_reset; encap_out_en <= 0; end 
                else state <= state_read_v;
            end

            state_wr_d_start: begin
                data_o <= 0;
                state <= state_wr_d;
                decap_in_wen <= 0;
                counter <= 0;
                decap_out_en <= 0;
            end
            state_wr_d: begin
                if (counter >= 10'd16) begin 
                    state <= state_decap_start; 
                    decap_in_wen <= 0; 
                    counter <= 0; 
                end
                else begin 
                    counter <= counter + 10'd1; 
                    decap_in_addr <= counter;
                    decap_in_wen <= 1;
                end
            end
            state_decap_start: begin
                start <= 1'b1; state <= state_decap_wait;
            end
            state_decap_wait: begin
                start <= 0;
                if (d_mwen) begin data_o[d_maddr*32+31 -: 32] <= d_min;
                end
                if (done) state <= state_reset;
                else state <= state_decap_wait;
            end
            state_read_ss_decap: begin
                if (read_done) begin state <= state_reset; decap_out_en <= 0; end
                else begin state <= state_read_ss_decap; read_start <= 0; end
            end
            
            state_wr_u: begin
                data_o <= 0;
                counter <= counter + 10'd1;
                umem_din0_reg <= data0_i[counter*128+127 -: 128];
                umem_din1_reg <= data1_i[counter*128+127 -: 128];
                if (counter >= 10'd3) state <= state_wr_u_done;
            end
            state_wr_u_done: begin
                counter <= 0; 
                umem_din0_reg <= 0; umem_din1_reg <= 0;
                u_counter <= u_counter + 10'd1;
                state <= state_stall;
            end

            state_wr_v: begin
                data_o <= 0;
                counter <= counter + 10'd1;
                vmem_din0_reg <= data0_i[counter*128+127 -: 128];
                vmem_din1_reg <= data1_i[counter*128+127 -: 128];
                if (counter >= 10'd3) state <= state_wr_v_done;
            end
            state_wr_v_done: begin
                counter <= 0; 
                vmem_din0_reg <= 0; vmem_din1_reg <= 0;
                v_counter <= v_counter + 10'd1;
                state <= state_stall;
            end

            state_reset: begin 
                rst <= 1'd1; counter <= 0; 
                u_counter <= 0; v_counter <= 0; d_counter <= 0;
                sk_seed <= 0; pk_seed <= 0;
                state <= state_reset_wait;
            end
            state_reset_wait: begin state <= state_stall; end

            state_stall: begin
                rst <= 0;
                counter <= 0;
                state <= state_stall;
                busy_o <= 0;
                start <= 0;
                read_start <= 0;
            end
            default: state <= state_stall;
        endcase
    end
end

endmodule