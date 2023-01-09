/*
 * Function name: cin_load_ddr_read
 * Function description: This function loads cin results from off-chip DRAM.
 *                       Two modes are enabled. If the whole feature maps of the layer could fit
 *                       in the on-chip buffer, they will be loaded as a whole. Otherwise, each time,
 *                       LAYER_IN_NUM_T * (LAYER_IN_W_T + FILTER_S - 1) of data are loaded.
 */
void cin_load_ddr_read(
		bus_mem_0     global_cin,
		bus_t0        *cin_burst_buf,
		uint          LAYER_IN_H_HW,
		uint          LAYER_IN_W_HW,
		uint          LAYER_IN_NUM_T,
		uint          LAYER_IN_H_T,
		uint          LAYER_IN_W_T,
		ap_uint<8>    FILTER_S_H,
    ap_uint<8>    FILTER_S_W,
    ap_uint<8>    FILTER_D_H,
    ap_uint<8>    FILTER_D_W,
    uint          STRIDE,
		uint          cin_offset,
		uint          in_num_iter,
		uint          in_h_iter,
		uint          in_w_iter,
		uint          num_tile,
		bool          change,
		bool          max_pool,
		bool          write
){
  // cout<<"cin_load_ddr_read"<<endl;
  // Read the data based on the data layout used.
  // If filter size is 1, the data layout is ceil(N / Tn) * ceil(H / Th) * ceil(W / Tw) * Th * Tw * Tn
  // o.w. ceil(N / Tn) * H * ceil(W / Tw) * Tw * Tn
  // The data in on-chip buffer will have the data layout of Th * Tw * Tn
  // if (change){
  //   for (int hh = 0; hh < 1; hh++){
  //     uint local_cin_offset = 0;
  //     uint global_cin_offset = cin_offset + num_tile * (LAYER_IN_W_T) * (LAYER_IN_H_T) * LAYER_IN_NUM_T;
  //     #ifdef DEBUG_cin
  //             if(write)
  //               cout << global_cin_offset << endl;
  //     #endif
  //     memcpy((void*)&cin_burst_buf[local_cin_offset / BUS_PACK_FACTOR0], (void*)&global_cin[global_cin_offset / BUS_PACK_FACTOR0], sizeof(data_t0) * LAYER_IN_NUM_T * (LAYER_IN_W_T) * (LAYER_IN_H_T));
  //   }
  // } else {
    for (int hh = 0; hh < LAYER_IN_H_T + FILTER_S_H - STRIDE; hh++){
      uint h = in_h_iter + hh;
      uint burst_len = 0;
      // case if the burst len is not divisible by BUS_PACK_FACTOR0
      if (((LAYER_IN_W_T + FILTER_D_W - STRIDE) * LAYER_IN_NUM_T) % BUS_PACK_FACTOR0 == 0){
        burst_len = (LAYER_IN_W_T + FILTER_D_W - STRIDE) * LAYER_IN_NUM_T;
      } else {
        burst_len = (LAYER_IN_W_T + FILTER_D_W - STRIDE) * LAYER_IN_NUM_T + BUS_PACK_FACTOR0 - ((LAYER_IN_W_T + FILTER_D_W - STRIDE) * LAYER_IN_NUM_T) % BUS_PACK_FACTOR0;
      }
      uint local_cin_offset = hh * burst_len;
      uint global_cin_offset = in_num_iter * LAYER_IN_H_HW * LAYER_IN_W_HW + h * LAYER_IN_W_HW * LAYER_IN_NUM_T + in_w_iter * LAYER_IN_NUM_T + cin_offset;
      #ifdef DEBUG_cin
            if(write)
              cout << global_cin_offset << endl;
      #endif
      memcpy((void*)&cin_burst_buf[local_cin_offset / BUS_PACK_FACTOR0], (void*)&global_cin[global_cin_offset / BUS_PACK_FACTOR0], sizeof(data_t0) * burst_len);
    }
  // }
}

/*
 * Function name: cin_load_fifo_write
 * Function description: This function writes cin data to the downstream modules.
 */
void cin_load_fifo_write(
		bus_t0                            *cin_burst_buf,
		tapa::ostream<CinLoadData0Type>   &fifo_cin,
		uint                              LAYER_IN_NUM_T,
		uint                              LAYER_IN_H_T,
		uint                              LAYER_IN_W_T,
		ap_uint<8>                        FILTER_S_H,
    ap_uint<8>                        FILTER_S_W,
    ap_uint<8>                        FILTER_D_H,
    ap_uint<8>                        FILTER_D_W,
    uint                              STRIDE
){
  // cout<<"cin_load_fifo_write"<<endl;
	int ii = 0;
	int hh = 0;
	int ww = 0;
	bool done = 0;
	while(!done){
	#pragma HLS PIPELINE II=1
	// Data layout of the buffer: Th * Tw * Tn
    uint burst_len = 0;
    // case if the burst len is not divisible by BUS_PACK_FACTOR0
    if (((LAYER_IN_W_T + FILTER_D_W - STRIDE) * LAYER_IN_NUM_T) % BUS_PACK_FACTOR0 == 0){
      burst_len = (LAYER_IN_W_T + FILTER_D_W - STRIDE) * LAYER_IN_NUM_T;
    } else {
      burst_len = (LAYER_IN_W_T + FILTER_D_W - STRIDE) * LAYER_IN_NUM_T + BUS_PACK_FACTOR0 - ((LAYER_IN_W_T + FILTER_D_W - STRIDE) * LAYER_IN_NUM_T) % BUS_PACK_FACTOR0;
    }
		uint local_cin_idx = hh * burst_len + ww * LAYER_IN_NUM_T + ii * DEPTH_CONV_LANE;
		uint bus_cin_idx = local_cin_idx / BUS_PACK_FACTOR0;
		uint bus_cin_offset = local_cin_idx % BUS_PACK_FACTOR0;
		bus_t0 bus_cin_data = cin_burst_buf[bus_cin_idx];
		CinLoadData0Type fifo_cin_data;
	// DATA_SEL_FACTOR = BUS_PACK_FACTOR / SIMD_LANE
	// BUS_PACK_FACTOR is the number of elements packed in one to enable memory coalescing
	// Since each entry in FIFOs will be SIMD_LANE elements of the data, we should unpack based on SIMD_LANE
	#if DATA_SEL_FACTOR0 == 1
		fifo_cin_data = bus_cin_data;
	#elif DATA_SEL_FACTOR0 == 2 
		switch(bus_cin_offset / DEPTH_CONV_LANE){
		case 0:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 1 - 1, DATA_W0 * DEPTH_CONV_LANE * 0);
			break;
		case 1:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 2 - 1, DATA_W0 * DEPTH_CONV_LANE * 1);
			break;
		}
	#elif DATA_SEL_FACTOR0 == 4
		switch(bus_cin_offset / DEPTH_CONV_LANE){
		case 0:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 1 - 1, DATA_W0 * DEPTH_CONV_LANE * 0);
			break;
		case 1:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 2 - 1, DATA_W0 * DEPTH_CONV_LANE * 1);
			break;
		case 2:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 3 - 1, DATA_W0 * DEPTH_CONV_LANE * 2);
			break;
		case 3:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 4 - 1, DATA_W0 * DEPTH_CONV_LANE * 3);
			break;
		}
	#elif DATA_SEL_FACTOR0 == 8
		switch(bus_cin_offset / DEPTH_CONV_LANE){
		case 0:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 1 - 1, DATA_W0 * DEPTH_CONV_LANE * 0);
			break;
		case 1:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 2 - 1, DATA_W0 * DEPTH_CONV_LANE * 1);
			break;
		case 2:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 3 - 1, DATA_W0 * DEPTH_CONV_LANE * 2);
			break;
		case 3:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 4 - 1, DATA_W0 * DEPTH_CONV_LANE * 3);
			break;
		case 4:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 5 - 1, DATA_W0 * DEPTH_CONV_LANE * 4);
			break;
		case 5:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 6 - 1, DATA_W0 * DEPTH_CONV_LANE * 5);
			break;
		case 6:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 7 - 1, DATA_W0 * DEPTH_CONV_LANE * 6);
			break;
		case 7:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 8 - 1, DATA_W0 * DEPTH_CONV_LANE * 7);
			break;
		}
	#elif DATA_SEL_FACTOR0 == 16
		switch(bus_cin_offset / DEPTH_CONV_LANE){
		case 0:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 1 - 1, DATA_W0 * DEPTH_CONV_LANE * 0);
			break;
		case 1:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 2 - 1, DATA_W0 * DEPTH_CONV_LANE * 1);
			break;
		case 2:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 3 - 1, DATA_W0 * DEPTH_CONV_LANE * 2);
			break;
		case 3:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 4 - 1, DATA_W0 * DEPTH_CONV_LANE * 3);
			break;
		case 4:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 5 - 1, DATA_W0 * DEPTH_CONV_LANE * 4);
			break;
		case 5:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 6 - 1, DATA_W0 * DEPTH_CONV_LANE * 5);
			break;
		case 6:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 7 - 1, DATA_W0 * DEPTH_CONV_LANE * 6);
			break;
		case 7:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 8 - 1, DATA_W0 * DEPTH_CONV_LANE * 7);
			break;
		case 8:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 9 - 1, DATA_W0 * DEPTH_CONV_LANE * 8);
			break;
		case 9:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 10 - 1, DATA_W0 * DEPTH_CONV_LANE * 9);
			break;
		case 10:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 11 - 1, DATA_W0 * DEPTH_CONV_LANE * 10);
			break;
		case 11:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 12 - 1, DATA_W0 * DEPTH_CONV_LANE * 11);
			break;
		case 12:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 13 - 1, DATA_W0 * DEPTH_CONV_LANE * 12);
			break;
		case 13:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 14 - 1, DATA_W0 * DEPTH_CONV_LANE * 13);
			break;
		case 14:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 15 - 1, DATA_W0 * DEPTH_CONV_LANE * 14);
			break;
		case 15:
			fifo_cin_data = bus_cin_data(DATA_W0 * DEPTH_CONV_LANE * 16 - 1, DATA_W0 * DEPTH_CONV_LANE * 15);
			break;
		}
	#endif     

		fifo_cin.write(fifo_cin_data);
	#ifdef DEBUG_load_change
		if(FILTER_S == 1){
			cout << " cin ";
			for (int lane = 0; lane < RELU_LANE; lane++){
	#pragma HLS UNROLL
				ap_uint<DATA_W0> u32_beta = fifo_cin_data(DATA_W0 - 1, 0);
				data_t2 a = Reinterpret<data_t2>(u32_beta);
				fifo_cin_data = fifo_cin_data >> DATA_W0;
				cout << a << " ";
			}
			cout << endl;
		}
	#endif

			// Repeat until the whole tile is read
		ww++;
		if (ww == LAYER_IN_W_T + FILTER_S_W - STRIDE){	//change here
			ww = 0;
			hh++;
			if (hh == LAYER_IN_H_T + FILTER_S_H - STRIDE){	//change here
				hh = 0;
				ii++;
				if (ii == LAYER_IN_NUM_T / DEPTH_CONV_LANE){
					ii = 0;
					done = 1;
				}
			}
		}
	}
}

/*
 * Function name: cin_load
 * Function description: This function loads and distributes cin and instructions.
 */
void cin_load(
    uint start_inst, uint end_inst,
		bus_mem_0                        global_cin,
		bus_mem_3                        layer_config,
		tapa::ostream<CinLoadData0Type>  &fifo_cin,
		tapa::ostream<ConfigInst>        &fifo_config_out,
    tapa::istream<int>               &in_sync,
    tapa::ostream<int>               &out_sync
){
	#pragma HLS INLINE off 
  cout<<"cin_load"<<endl;
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
  int sync = inst==0? 0 : in_sync.read();
  out_sync.write(inst);
  if (sync == inst) {
  int layer_id = start_inst + inst;
	cout<<"layer_id: "<<layer_id<<endl;
  unsigned int config[CONFIG_PARAMS * MAX_LAYER_BATCH];
  // memcpy((void*)config, (void*)(&layer_config[CONFIG_PARAMS * layer_id]), sizeof(unsigned int) * CONFIG_PARAMS * 1);
  for(int i=0; i<CONFIG_PARAMS; i++){
    #pragma HLS PIPELINE II=1
    config[i] = layer_config[CONFIG_PARAMS * layer_id + i];
  }

		// on-chip buffer for cin data
	bus_t0 cin_burst_buf_ping[CIN_BUFF / BUS_PACK_FACTOR0];
	bus_t0 cin_burst_buf_pong[CIN_BUFF / BUS_PACK_FACTOR0];

	#if cin_load_MEM == 0
		#pragma HLS bind_storage variable=cin_burst_buf_ping type=RAM_T2P impl=BRAM
		#pragma HLS bind_storage variable=cin_burst_buf_pong type=RAM_T2P impl=BRAM  
	#elif cin_load_MEM == 1
		#pragma HLS bind_storage variable=cin_burst_buf_ping type=RAM_T2P impl=URAM
		#pragma HLS bind_storage variable=cin_burst_buf_pong type=RAM_T2P impl=URAM 
	#endif



		// layer batch
	ap_uint<32> LAYER_BATCH = config[28];

		// tiling iterators
	uint in_num_iter = 0;
	uint out_num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

	uint in_num_iter_prev = 0;
	uint out_num_iter_prev = 0;
	uint in_h_iter_prev = 0;
	uint in_w_iter_prev = 0;
	uint layer_iter_prev = 0;

		// parameters
		// inst0
	ap_uint<32> LAYER_IN_NUM_HW;
	ap_uint<32> LAYER_OUT_NUM_HW;
	ap_uint<32> LAYER_IN_H_HW;
	ap_uint<32> LAYER_IN_W_HW;
	ap_uint<32> LAYER_OUT_H_HW;
	ap_uint<32> LAYER_OUT_W_HW;
  // NPD: north padding
  // SPD: south padding
  // WPD: west padding
  // EPD: east padding
  ap_uint<16> LAYER_OUT_H_NPD;
  ap_uint<16> LAYER_OUT_H_SPD;
  ap_uint<16> LAYER_OUT_W_WPD;
  ap_uint<16> LAYER_OUT_W_EPD;
		// inst1
	ap_uint<32> LAYER_IN_NUM;
	ap_uint<32> LAYER_OUT_NUM;
	ap_uint<32> LAYER_IN_H;
	ap_uint<32> LAYER_IN_W;
	ap_uint<32> LAYER_OUT_H;
	ap_uint<32> LAYER_OUT_W;
		// inst2
	ap_uint<32> CIN_OFFSET;
	ap_uint<32> WEIGHT_OFFSET;
	ap_uint<32> BIAS_OFFSET;
	ap_uint<32> COUT_OFFSET;
	ap_uint<16> FILTER_S1;
	ap_uint<8> FILTER_S2_H;
  ap_uint<8> FILTER_S2_W;
	ap_uint<32> STRIDE;
		// inst3
	ap_uint<32> LAYER_EN;
	ap_uint<16> LAYER_IN_NUM_T;
	ap_uint<16> LAYER_OUT_NUM_T;
	ap_uint<32> LAYER_IN_H_T;
	ap_uint<32> LAYER_IN_W_T;
	ap_uint<32> PREV_CIN_OFFSET;
		// inst4
	ap_uint<32> LAYER_TASK_NUM1;
	ap_uint<32> LAYER_TASK_NUM2;
	ap_uint<32> LAYER_LOCAL_ACCUM_NUM;
	ap_uint<32> LAYER_LOCAL_REG_NUM;
	ap_uint<32> LAYER_ROW_IL_FACTOR;
	ap_uint<32> LAYER_COL_IL_FACTOR;
		//inst5
	ap_uint<16> LAYER_CONV_TYPE;
	ap_uint<8> FILTER_D0_H;
  ap_uint<8> FILTER_D0_W;
	ap_uint<8> FILTER_D1_H;
  ap_uint<8> FILTER_D1_W;
	ap_uint<16> LAYER_DILATION_RATE;
	ap_uint<16> LAYER_TCONV_STRIDE;
	ap_uint<16> K_NUM;
  ap_uint<32> KH;
  ap_uint<32> KW;
  ap_uint<16> POOL_NUM;
  ap_uint<16> POOL_NUM_HW;


	ap_uint<1>  CONV_1ST_EN;
	ap_uint<1>  LOAD_PREV_CIN;

	uint LAYER_IN_NUM_T_prev;
	uint LAYER_OUT_NUM_T_prev;
	uint LAYER_IN_H_T_prev;
	uint LAYER_IN_W_T_prev;
	ap_uint<8> FILTER_S_H_prev;
  ap_uint<8> FILTER_S_W_prev;
  ap_uint<8> FILTER_D_H_prev;
  ap_uint<8> FILTER_D_W_prev;
  uint STRIDE_prev;
	

	uint task_cnt = 0;
	bool layer_start = 1;
	bool layer_start_prev = 0;
	bool done = 0;
		// We assum that cin has been pre-padded with zeros
	uint prev = 0;
	uint init = 1;
	uint num_tile = 0;
	bool write_last_cin = 0;
	bool write_last_prev_cin = 0;
	bool start_prev = 0;
	bool done_prev = 0;
	bool change_layout = 0;
	uint inter_tile = 0;
	uint channel_iter = 0;
  int count_fifo = 0;
  int count_dram = 0;
  int count = 0;
  int iter = 0;
	while(!done){
			// Read and extract the parameters/config from the instructions
			// Refer to util.h or the README of the repo to find how the instructions are made
			// inst0 : The hardware sizes of each dimension (the sizes after tiling is applied)
		LAYER_IN_NUM_HW  = config[0 + layer_iter * CONFIG_PARAMS];
		LAYER_OUT_NUM_HW = config[1 + layer_iter * CONFIG_PARAMS];
		LAYER_IN_H_HW    = config[2 + layer_iter * CONFIG_PARAMS];
		LAYER_IN_W_HW    = config[3 + layer_iter * CONFIG_PARAMS];
		// LAYER_OUT_H_HW   = config[4 + layer_iter * CONFIG_PARAMS];
		// LAYER_OUT_W_HW   = config[5 + layer_iter * CONFIG_PARAMS];
    LAYER_OUT_H_NPD = config[4 + layer_iter * CONFIG_PARAMS];
    LAYER_OUT_H_SPD = config[5 + layer_iter * CONFIG_PARAMS];
    LAYER_OUT_W_WPD = config[6 + layer_iter * CONFIG_PARAMS];
    LAYER_OUT_W_EPD = config[7 + layer_iter * CONFIG_PARAMS];

			// inst1 : The actual sizes of each dimension
		LAYER_IN_NUM  = config[8 + layer_iter * CONFIG_PARAMS];
		LAYER_OUT_NUM = config[9 + layer_iter * CONFIG_PARAMS];
		LAYER_IN_H    = config[10 + layer_iter * CONFIG_PARAMS];
		LAYER_IN_W    = config[11 + layer_iter * CONFIG_PARAMS];
		LAYER_OUT_H   = config[12 + layer_iter * CONFIG_PARAMS];
		LAYER_OUT_W   = config[13 + layer_iter * CONFIG_PARAMS];

			// inst2 : The DRAM locations for reading/writing the data of this layer + Filter and Stride sizes
		CIN_OFFSET    = config[14 + layer_iter * CONFIG_PARAMS];
		WEIGHT_OFFSET = config[15 + layer_iter * CONFIG_PARAMS];
		BIAS_OFFSET   = config[16 + layer_iter * CONFIG_PARAMS];
		COUT_OFFSET   = config[17 + layer_iter * CONFIG_PARAMS];
		FILTER_S1     = config[18 + layer_iter * CONFIG_PARAMS];
		FILTER_S2_H   = config[19 + layer_iter * CONFIG_PARAMS];
    FILTER_S2_W   = config[20 + layer_iter * CONFIG_PARAMS];

		STRIDE        = config[21 + layer_iter * CONFIG_PARAMS];

			// inst3 : The enable signlas of the modules + DRAM location of the input to the previous layer + Tile sizes
		LAYER_EN        = config[22 + layer_iter * CONFIG_PARAMS]; 	// contains the enable signals for the modules
		PREV_CIN_OFFSET = config[23 + layer_iter * CONFIG_PARAMS];
		LAYER_IN_NUM_T  = config[24 + layer_iter * CONFIG_PARAMS];
		LAYER_OUT_NUM_T = config[25 + layer_iter * CONFIG_PARAMS];
		LAYER_IN_H_T    = config[26 + layer_iter * CONFIG_PARAMS];
		LAYER_IN_W_T    = config[27 + layer_iter * CONFIG_PARAMS];

		CONV_1ST_EN    = LAYER_EN[0];
		ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
		ap_uint<1>  CONV_EN        = LAYER_EN[2];
		ap_uint<1>  RELU_EN        = LAYER_EN[3];
		ap_uint<1>  RELU6_EN       = LAYER_EN[4];
		ap_uint<1>  POOL_EN        = LAYER_EN[5];
		ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
		ap_uint<1>  BIAS_EN        = LAYER_EN[7];
		ap_uint<1>  INTER_LOAD_EN  = LAYER_EN[8];
		ap_uint<1>  INTER_WRITE_EN = LAYER_EN[9];
		ap_uint<1>  BATCH_NORM_EN  = LAYER_EN[10];
		ap_uint<1>  BATCH_NORM_EN_DEPTH  = LAYER_EN[12];


		LOAD_PREV_CIN  = LAYER_EN[11];

			// inst4 : The info needed to run the systolic array
		LAYER_TASK_NUM1       = config[29 + layer_iter * CONFIG_PARAMS];
		LAYER_TASK_NUM2       = config[30 + layer_iter * CONFIG_PARAMS];
		LAYER_LOCAL_ACCUM_NUM = config[31 + layer_iter * CONFIG_PARAMS];
		LAYER_LOCAL_REG_NUM   = config[32 + layer_iter * CONFIG_PARAMS];
		LAYER_ROW_IL_FACTOR   = config[33 + layer_iter * CONFIG_PARAMS];
		LAYER_COL_IL_FACTOR   = config[34 + layer_iter * CONFIG_PARAMS];
			// inst5 : DT CONV INSTS
		LAYER_CONV_TYPE         = config[35 + layer_iter * CONFIG_PARAMS];
		FILTER_D0_H							= config[36 + layer_iter * CONFIG_PARAMS];
    FILTER_D0_W							= config[37 + layer_iter * CONFIG_PARAMS];
    FILTER_D1_H							= config[38 + layer_iter * CONFIG_PARAMS];
    FILTER_D1_W							= config[39 + layer_iter * CONFIG_PARAMS];
		LAYER_DILATION_RATE			= config[40 + layer_iter * CONFIG_PARAMS];
		LAYER_TCONV_STRIDE  		= config[41 + layer_iter * CONFIG_PARAMS];
		K_NUM               		= config[42 + layer_iter * CONFIG_PARAMS];
    KH(31,24)        		= config[43 + layer_iter * CONFIG_PARAMS];
    KH(23,16)        		= config[44 + layer_iter * CONFIG_PARAMS];
    KH(15,8)         		= config[45 + layer_iter * CONFIG_PARAMS];
    KH(7,0)          		= config[46 + layer_iter * CONFIG_PARAMS];
    KW(31,24)        		= config[47 + layer_iter * CONFIG_PARAMS];
    KW(23,16)        		= config[48 + layer_iter * CONFIG_PARAMS];
    KW(15,8)         		= config[49 + layer_iter * CONFIG_PARAMS];
    KW(7,0)          		= config[50 + layer_iter * CONFIG_PARAMS];
    POOL_NUM          	= config[51 + layer_iter * CONFIG_PARAMS];
    POOL_NUM_HW         = config[52 + layer_iter * CONFIG_PARAMS];

  // #define DEBUG_config_cin
	#ifdef DEBUG_config_cin
  
		cout << LAYER_IN_NUM_HW << " " << LAYER_OUT_NUM_HW << " " << LAYER_IN_H_HW << " " << LAYER_IN_W_HW << " " 
    << LAYER_OUT_H_NPD << " " << LAYER_OUT_H_SPD << " " << LAYER_OUT_W_WPD << " " << LAYER_OUT_W_EPD << endl;
		cout << LAYER_IN_NUM << " " << LAYER_OUT_NUM << " " << LAYER_IN_H << " " << LAYER_IN_W << " " << LAYER_OUT_H << " " << LAYER_OUT_W << endl;
		cout << CIN_OFFSET << " " << WEIGHT_OFFSET << " " << BIAS_OFFSET << " " << COUT_OFFSET << " " << FILTER_S1 << " " << FILTER_S2_H << " " << FILTER_S2_W << " " << STRIDE << endl;
		cout << LAYER_EN << " " << PREV_CIN_OFFSET << " " << LAYER_IN_NUM_T << " " << LAYER_OUT_NUM_T << " " << LAYER_IN_H_T << " " << LAYER_IN_W_T << endl;
    cout << LAYER_TASK_NUM1 << " " << LAYER_TASK_NUM2 << " " << LAYER_LOCAL_ACCUM_NUM << " " << LAYER_LOCAL_REG_NUM << " " << LAYER_ROW_IL_FACTOR << " " << LAYER_COL_IL_FACTOR << endl;
    cout << LAYER_CONV_TYPE << " " << FILTER_D0_H << " " << FILTER_D0_W << " " << FILTER_D1_H << " " << FILTER_D1_W << " " << LAYER_DILATION_RATE << " " << LAYER_TCONV_STRIDE;
    cout << " " << K_NUM << " " << KH << " " << KW << " " << POOL_NUM << " " << POOL_NUM_HW << endl;
	#endif
  
			// Pack the parameters to pass them throught the config FIFOs
		ConfigInst inst0 = (LAYER_OUT_W_EPD, LAYER_OUT_W_WPD, LAYER_OUT_H_SPD, LAYER_OUT_H_NPD, LAYER_IN_W_HW, LAYER_IN_H_HW, LAYER_OUT_NUM_HW, LAYER_IN_NUM_HW);
		ConfigInst inst1 = (LAYER_OUT_W, LAYER_OUT_H, LAYER_IN_W, LAYER_IN_H, LAYER_OUT_NUM, LAYER_IN_NUM);
		ConfigInst inst2 = (STRIDE, FILTER_S2_W, FILTER_S2_H, FILTER_S1, COUT_OFFSET, BIAS_OFFSET, WEIGHT_OFFSET, CIN_OFFSET);
		ConfigInst inst3 = (LAYER_BATCH, LAYER_IN_W_T, LAYER_IN_H_T, LAYER_OUT_NUM_T, LAYER_IN_NUM_T, PREV_CIN_OFFSET, LAYER_EN);
		ConfigInst inst4 = (LAYER_COL_IL_FACTOR, LAYER_ROW_IL_FACTOR, LAYER_LOCAL_REG_NUM, LAYER_LOCAL_ACCUM_NUM, LAYER_TASK_NUM2, LAYER_TASK_NUM1);
		ConfigInst inst5 = (POOL_NUM_HW, POOL_NUM, KW, KH, K_NUM, LAYER_TCONV_STRIDE, LAYER_DILATION_RATE, FILTER_D1_W, FILTER_D1_H, FILTER_D0_W, FILTER_D0_H, LAYER_CONV_TYPE);

			// Pass the config/instructions
		if (layer_start){
			fifo_config_out.write(inst0);
			fifo_config_out.write(inst1);
			fifo_config_out.write(inst2);
			fifo_config_out.write(inst3);
			fifo_config_out.write(inst4);
			fifo_config_out.write(inst5);
			layer_start = 0;
		}

			// offsets
		uint cin_offset = CIN_OFFSET;
		uint prev_cin_offset = PREV_CIN_OFFSET;

		if (prev == 1) start_prev = 1;
    
		// set up some configuration signals
		ap_uint<8> FILTER_S_H = ((CONV_EN == 1)? (ap_uint<8>) ((LAYER_CONV_TYPE == 1)? (ap_uint<8>) unpack(KH, 0) : (ap_uint<8>) FILTER_S2_H) : (ap_uint<8>) 1);
    ap_uint<8> FILTER_S_W = ((CONV_EN == 1)? (ap_uint<8>) ((LAYER_CONV_TYPE == 1)? (ap_uint<8>) unpack(KW, 0) : (ap_uint<8>) FILTER_S2_W) : (ap_uint<8>) 1);
    ap_uint<8> FILTER_D_H = FILTER_D0_H;
    ap_uint<8> FILTER_D_W = FILTER_D0_W;

		bool separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
		bool conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
		bool max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);
		change_layout = (((LAYER_IN_W_HW == LAYER_IN_W) || (LAYER_IN_W_HW == LAYER_IN_W_T)) && ((LAYER_IN_H_HW == LAYER_IN_H) || (LAYER_IN_H_HW == LAYER_IN_H_T))); 	// if next filter = 1 : change the layout to num_tile, Th, Tw, Tn
    if (max_pool && !conv2d){
      iter = out_num_iter;
    }else{
      iter = in_num_iter;
    }
			// If it has to read from DRAM and not the stored data in on-chip storage
		if (INTER_LOAD_EN == 0){
			if ((max_pool && in_num_iter == 0) || separable_conv || conv2d || (UP_SAMPLE_EN && in_num_iter == 0)){
				if (task_cnt == 0){
						// first load cin
					cin_load_ddr_read(global_cin, cin_burst_buf_ping, LAYER_IN_H_HW, LAYER_IN_W_HW, LAYER_IN_NUM_T, LAYER_IN_H_T, LAYER_IN_W_T, FILTER_S_H, FILTER_S_W, FILTER_D_H, FILTER_D_W, STRIDE, cin_offset, iter, in_h_iter, in_w_iter, num_tile, change_layout, max_pool, 0);
        } else {
						// Apply double buffering for reading the data and filling the FIFO
					if (task_cnt % 2 == 1){
						cin_load_ddr_read(global_cin, cin_burst_buf_pong, LAYER_IN_H_HW, LAYER_IN_W_HW, LAYER_IN_NUM_T, LAYER_IN_H_T, LAYER_IN_W_T, FILTER_S_H, FILTER_S_W, FILTER_D_H, FILTER_D_W, STRIDE, cin_offset, iter, in_h_iter, in_w_iter, num_tile, change_layout, max_pool, 0);
            cin_load_fifo_write(cin_burst_buf_ping, fifo_cin, LAYER_IN_NUM_T_prev, LAYER_IN_H_T_prev, LAYER_IN_W_T_prev, FILTER_S_H_prev, FILTER_S_W_prev, FILTER_D_H_prev, FILTER_D_W_prev, STRIDE_prev);
          } else {
						cin_load_ddr_read(global_cin, cin_burst_buf_ping, LAYER_IN_H_HW, LAYER_IN_W_HW, LAYER_IN_NUM_T, LAYER_IN_H_T, LAYER_IN_W_T, FILTER_S_H, FILTER_S_W, FILTER_D_H, FILTER_D_W, STRIDE, cin_offset, iter, in_h_iter, in_w_iter, num_tile, change_layout, max_pool, 0);
            cin_load_fifo_write(cin_burst_buf_pong, fifo_cin, LAYER_IN_NUM_T_prev, LAYER_IN_H_T_prev, LAYER_IN_W_T_prev, FILTER_S_H_prev, FILTER_S_W_prev, FILTER_D_H_prev, FILTER_D_W_prev, STRIDE_prev);
          }
				}

				task_cnt++;
				LAYER_IN_NUM_T_prev = LAYER_IN_NUM_T;
				LAYER_OUT_NUM_T_prev = LAYER_OUT_NUM_T;
				LAYER_IN_H_T_prev = LAYER_IN_H_T;
				LAYER_IN_W_T_prev = LAYER_IN_W_T;
				FILTER_S_H_prev = FILTER_S_H;
        FILTER_S_W_prev = FILTER_S_W;
        FILTER_D_H_prev = FILTER_D_H;
        FILTER_D_W_prev = FILTER_D_W;
        STRIDE_prev = STRIDE;
			}
		}

			// Continue until all the tiles are read
			// Since each layer produces LAYER_OUT_NUM feature maps, 
			// repeat reading the tiles LAYER_OUT_NUM times
		in_num_iter += LAYER_IN_NUM_T;
		if (in_num_iter < LAYER_IN_NUM){
			channel_iter += ((LAYER_IN_W / LAYER_IN_W_T) * (LAYER_IN_H / LAYER_IN_H_T));
		} else {
			channel_iter = 0;
			inter_tile++;
		}

		num_tile = conv2d==1? channel_iter + inter_tile : num_tile + 1;
		if (in_num_iter >= LAYER_IN_NUM){
			in_num_iter = 0;
			channel_iter = 0;
			in_h_iter += LAYER_IN_H_T;
			if (in_h_iter >= LAYER_IN_H){
				in_h_iter = 0;
				in_w_iter += LAYER_IN_W_T; 	
				if (in_w_iter >= LAYER_IN_W){
					in_w_iter = 0;
					out_num_iter += LAYER_OUT_NUM_T;
					num_tile = 0;
					inter_tile = 0;
					channel_iter = 0;
					if (out_num_iter >= LAYER_OUT_NUM){
						out_num_iter = 0;
						layer_iter += 1;
						prev = 0;
						layer_start = 1;
						if (layer_iter == LAYER_BATCH){
							layer_iter = 0;
							out_num_iter = 0;
							in_h_iter = 0;
							in_w_iter = 0;
							done = 1;
						}
					}
				}
			}
		}
	}


		// Fill the FIFOs with the data for the last tile
	if (task_cnt % 2 == 1){
		cin_load_fifo_write(cin_burst_buf_ping, fifo_cin, LAYER_IN_NUM_T_prev, LAYER_IN_H_T_prev, LAYER_IN_W_T_prev, FILTER_S_H_prev, FILTER_S_W_prev, FILTER_D_H_prev, FILTER_D_W_prev, STRIDE_prev);
    count_fifo++;
	} else {
		cin_load_fifo_write(cin_burst_buf_pong, fifo_cin, LAYER_IN_NUM_T_prev, LAYER_IN_H_T_prev, LAYER_IN_W_T_prev, FILTER_S_H_prev, FILTER_S_W_prev, FILTER_D_H_prev, FILTER_D_W_prev, STRIDE_prev);
    count_fifo++;
	}
  inst++;
  }
  if(inst == inst_count){
    inst_done = 1;
  }
  }
}
