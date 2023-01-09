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
  if (change){
    for (int hh = 0; hh < 1; hh++){
      uint local_cin_offset = 0;
      uint global_cin_offset = cin_offset + num_tile * (LAYER_IN_W_T) * (LAYER_IN_H_T) * LAYER_IN_NUM_T;
      #ifdef DEBUG_cin
              if(write)
                cout << global_cin_offset << endl;
      #endif
      memcpy((void*)&cin_burst_buf[local_cin_offset / BUS_PACK_FACTOR0], (void*)&global_cin[global_cin_offset / BUS_PACK_FACTOR0], sizeof(data_t0) * LAYER_IN_NUM_T * (LAYER_IN_W_T) * (LAYER_IN_H_T));
    }
  } else {
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
  }
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

			// If it has to read from DRAM and not the stored data in on-chip storage
		if (INTER_LOAD_EN == 0){
			if ((max_pool && out_num_iter == 0) || separable_conv || conv2d || (UP_SAMPLE_EN && out_num_iter == 0)){
				if (task_cnt == 0){
						// first load cin
					cin_load_ddr_read(global_cin, cin_burst_buf_ping, LAYER_IN_H_HW, LAYER_IN_W_HW, LAYER_IN_NUM_T, LAYER_IN_H_T, LAYER_IN_W_T, FILTER_S_H, FILTER_S_W, FILTER_D_H, FILTER_D_W, STRIDE, cin_offset, in_num_iter, in_h_iter, in_w_iter, num_tile, change_layout, max_pool, 0);
        } else {
						// Apply double buffering for reading the data and filling the FIFO
					if (task_cnt % 2 == 1){
						cin_load_ddr_read(global_cin, cin_burst_buf_pong, LAYER_IN_H_HW, LAYER_IN_W_HW, LAYER_IN_NUM_T, LAYER_IN_H_T, LAYER_IN_W_T, FILTER_S_H, FILTER_S_W, FILTER_D_H, FILTER_D_W, STRIDE, cin_offset, in_num_iter, in_h_iter, in_w_iter, num_tile, change_layout, max_pool, 0);
            cin_load_fifo_write(cin_burst_buf_ping, fifo_cin, LAYER_IN_NUM_T_prev, LAYER_IN_H_T_prev, LAYER_IN_W_T_prev, FILTER_S_H_prev, FILTER_S_W_prev, FILTER_D_H_prev, FILTER_D_W_prev, STRIDE_prev);
          } else {
						cin_load_ddr_read(global_cin, cin_burst_buf_ping, LAYER_IN_H_HW, LAYER_IN_W_HW, LAYER_IN_NUM_T, LAYER_IN_H_T, LAYER_IN_W_T, FILTER_S_H, FILTER_S_W, FILTER_D_H, FILTER_D_W, STRIDE, cin_offset, in_num_iter, in_h_iter, in_w_iter, num_tile, change_layout, max_pool, 0);
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
/*
 * Function name: weight_load_conv_weight_write
 * Function description: this function writes conv weights to conv module.
 * It has the same functionality as weight_load_depth_conv_weight_write
 */
void weight_load_conv_weight_write(
		bus_t1 weight_burst_buf2[],
		tapa::ostream<WeightLoadData1Type> &fifo_conv_weight,
		ConfigInst inst0,
		ConfigInst inst1,
		ConfigInst inst3,
		ap_uint<8> FILTER_H,
    ap_uint<8> FILTER_W,
		uint in_num_iter,
		uint out_num_iter
){
		// inst0
	ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
	ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
	ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
	ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
	ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
	ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
		// inst1
	ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
	ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
	ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
	ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
	ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
	ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
		// 	// inst2
		// ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
		// ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		// ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
		// ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
		// ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
		// ap_uint<16> FILTER_S2        = inst2(32*4+31, 32*4+16);
		// ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);
		// inst3
	ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
	ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
	ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
	ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
	ap_uint<32> LAYER_IN_H_T     = inst3(32*3+31, 32*3);
	ap_uint<32> LAYER_IN_W_T     = inst3(32*4+31, 32*4);

	ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
	ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
	ap_uint<1>  CONV_EN        = LAYER_EN[2];
	ap_uint<1>  RELU_EN        = LAYER_EN[3];
	ap_uint<1>  RELU6_EN       = LAYER_EN[4];
	ap_uint<1>  POOL_EN        = LAYER_EN[5];
	ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
	

	if (CONV_EN == 1){
		int oo = 0;
		int p = 0;
		int q = 0;
		int ii = 0;
		bool done = 0;
		while(!done){
	#pragma HLS PIPELINE II=1

	#ifdef DEBUG_weight2
			cout << "in loading weights " << DATA_SEL_FACTOR1 << " " << ii << " " << q << " " << p << " " << oo <<" " << LAYER_OUT_NUM_T << endl;
	#endif          
			uint local_w_idx = oo * FILTER_H * FILTER_W * LAYER_IN_NUM_T + p * FILTER_W * LAYER_IN_NUM_T + q * LAYER_IN_NUM_T + ii * SIMD_LANE;
			uint bus_w_idx = local_w_idx / BUS_PACK_FACTOR1;
			uint bus_w_offset = local_w_idx % BUS_PACK_FACTOR1;
				// cout<<bus_w_idx<<endl;
			bus_t1 bus_w_data = weight_burst_buf2[bus_w_idx];
				// cout<<weight_burst_buf2[bus_w_idx]<<endl;
			WeightLoadData1Type fifo_w_data;
	#if DATA_SEL_FACTOR1 == 1
			fifo_w_data = bus_w_data;
	#elif DATA_SEL_FACTOR1 == 2
			switch(bus_w_offset / SIMD_LANE){
			case 0:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 1 - 1, DATA_W1 * SIMD_LANE * 0);
				break;
			case 1:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 2 - 1, DATA_W1 * SIMD_LANE * 1);
				break;
			}
	#elif DATA_SEL_FACTOR1 == 4
			switch(bus_w_offset / SIMD_LANE){
			case 0:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 1 - 1, DATA_W1 * SIMD_LANE * 0);
				break;
			case 1:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 2 - 1, DATA_W1 * SIMD_LANE * 1);
				break;
			case 2:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 3 - 1, DATA_W1 * SIMD_LANE * 2);
				break;
			case 3:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 4 - 1, DATA_W1 * SIMD_LANE * 3);
				break;
			}
	#elif DATA_SEL_FACTOR1 == 8
			switch(bus_w_offset / SIMD_LANE){
			case 0:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 1 - 1, DATA_W1 * SIMD_LANE * 0);
				break;
			case 1:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 2 - 1, DATA_W1 * SIMD_LANE * 1);
				break;
			case 2:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 3 - 1, DATA_W1 * SIMD_LANE * 2);
				break;
			case 3:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 4 - 1, DATA_W1 * SIMD_LANE * 3);
				break;
			case 4:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 5 - 1, DATA_W1 * SIMD_LANE * 4);
				break;
			case 5:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 6 - 1, DATA_W1 * SIMD_LANE * 5);
				break;
			case 6:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 7 - 1, DATA_W1 * SIMD_LANE * 6);
				break;
			case 7:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 8 - 1, DATA_W1 * SIMD_LANE * 7);
				break;
			}
	#elif DATA_SEL_FACTOR1 == 16
			switch(bus_w_offset / SIMD_LANE){
			case 0:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 1 - 1, DATA_W1 * SIMD_LANE * 0);
				break;
			case 1:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 2 - 1, DATA_W1 * SIMD_LANE * 1);
				break;
			case 2:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 3 - 1, DATA_W1 * SIMD_LANE * 2);
				break;
			case 3:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 4 - 1, DATA_W1 * SIMD_LANE * 3);
				break;
			case 4:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 5 - 1, DATA_W1 * SIMD_LANE * 4);
				break;
			case 5:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 6 - 1, DATA_W1 * SIMD_LANE * 5);
				break;
			case 6:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 7 - 1, DATA_W1 * SIMD_LANE * 6);
				break;
			case 7:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 8 - 1, DATA_W1 * SIMD_LANE * 7);
				break;
			case 8:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 9 - 1, DATA_W1 * SIMD_LANE * 8);
				break;
			case 9:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 10 - 1, DATA_W1 * SIMD_LANE * 9);
				break;
			case 10:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 11 - 1, DATA_W1 * SIMD_LANE * 10);
				break;
			case 11:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 12 - 1, DATA_W1 * SIMD_LANE * 11);
				break;
			case 12:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 13 - 1, DATA_W1 * SIMD_LANE * 12);
				break;
			case 13:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 14 - 1, DATA_W1 * SIMD_LANE * 13);
				break;
			case 14:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 15 - 1, DATA_W1 * SIMD_LANE * 14);
				break;
			case 15:
				fifo_w_data = bus_w_data(DATA_W1 * SIMD_LANE * 16 - 1, DATA_W1 * SIMD_LANE * 15);
				break;
			}
	#endif          

			fifo_conv_weight.write(fifo_w_data);

			ii++;
			if (ii == LAYER_IN_NUM_T / SIMD_LANE){
				ii = 0;
				q++;
				if (q == FILTER_W){
					q = 0;
					p++;
					if (p == FILTER_H){
						p = 0;
						oo++;
						if (oo == LAYER_OUT_NUM_T){
							oo = 0;
							done = 1;
						}
					}
				}
			}
		}
	}
}

/**
 * Function name: weight_load
 * Function description: This function loads weights and distributes them to downstream modules.
 */
void weight_load(
    uint start_inst, uint end_inst,
		bus_mem_1                          global_weight,
		tapa::istream<ConfigInst>          &fifo_config_in,
		tapa::ostream<WeightLoadData1Type> &fifo_conv_weight,
		tapa::ostream<ConfigInst>          &fifo_config_out
){
	#pragma HLS INLINE off 
  cout<<"weight_load"<<endl;
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
		// on-chip buffers
	bus_t1 weight_burst_buf2[WEIGHT_BUFF / BUS_PACK_FACTOR1];
	#if weight_load_MEM == 0
		#pragma HLS bind_storage variable=weight_burst_buf2 type=RAM_T2P impl=BRAM  
	#elif weight_load_MEM == 1
		#pragma HLS bind_storage variable=weight_burst_buf2 type=RAM_T2P impl=URAM  
	#endif

		// tiling iterators
	uint in_num_iter = 0;
	uint out_num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

		// Read instructions
	ConfigInst inst0 = fifo_config_in.read();
	fifo_config_out.write(inst0);
	ConfigInst inst1 = fifo_config_in.read();
	fifo_config_out.write(inst1);
	ConfigInst inst2 = fifo_config_in.read();
	fifo_config_out.write(inst2);
	ConfigInst inst3 = fifo_config_in.read();
	fifo_config_out.write(inst3);
	ConfigInst inst4 = fifo_config_in.read();
	fifo_config_out.write(inst4);
  ConfigInst inst5 = fifo_config_in.read();
	fifo_config_out.write(inst5);

	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);

  // fifo_config_out.write(inst0);
	// fifo_config_out.write(inst1);
	// fifo_config_out.write(inst2);
	// fifo_config_out.write(inst3);
	// fifo_config_out.write(inst4);
  // fifo_config_out.write(inst5);

	bool layer_start = 0;
	bool done = 0;
  int counter = 0;
		// We assum that cin has been pre-padded with zeros
	while(!done){
    
		if (layer_start){
			inst0 = fifo_config_in.read();
			fifo_config_out.write(inst0);
			inst1 = fifo_config_in.read();
			fifo_config_out.write(inst1);
			inst2 = fifo_config_in.read();
			fifo_config_out.write(inst2);
			inst3 = fifo_config_in.read();
			fifo_config_out.write(inst3);
			inst4 = fifo_config_in.read();
			fifo_config_out.write(inst4);
      inst5 = fifo_config_in.read();
			fifo_config_out.write(inst5);
			layer_start = 0;
		}

			// Refer to cin_load module to understand the meaning of the instructions
			// inst0
		ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
    // cout<<"LAYER_IN_NUM_HW: "<<LAYER_IN_NUM_HW<<endl;
    // cout<<"LAYER_OUT_NUM_HW: "<<LAYER_OUT_NUM_HW<<endl;
    // cout<<"LAYER_IN_H_HW: "<<LAYER_IN_H_HW<<endl;
    // cout<<"LAYER_IN_W_HW: "<<LAYER_IN_W_HW<<endl;
    // cout<<"LAYER_OUT_H_HW: "<<LAYER_OUT_H_HW<<endl;
    // cout<<"LAYER_OUT_W_HW: "<<LAYER_OUT_W_HW<<endl;
    // exit(0);
			// inst1
		ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
			// inst2
		ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
		ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
		ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
		ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
		ap_uint<8>  FILTER_S2_H      = inst2(32*4+23, 32*4+16);
    ap_uint<8>  FILTER_S2_W      = inst2(32*4+31, 32*4+24);
		ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);

			// inst3
		ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
		ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
		ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
		ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
		ap_uint<32> LAYER_IN_H_T     = inst3(32*3+31, 32*3);
		ap_uint<32> LAYER_IN_W_T     = inst3(32*4+31, 32*4);

		ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
		ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
		ap_uint<1>  CONV_EN        = LAYER_EN[2];
		ap_uint<1>  RELU_EN        = LAYER_EN[3];
		ap_uint<1>  RELU6_EN       = LAYER_EN[4];
		ap_uint<1>  POOL_EN        = LAYER_EN[5];
		ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
    ap_uint<1>  BIAS_EN        = LAYER_EN[7];
		ap_uint<1>  BATCH_NORM_EN  = LAYER_EN[10];
    ap_uint<1>  BATCH_NORM_EN_DEPTH  = LAYER_EN[12];
    ap_uint<1>  BIAS_1_EN             = LAYER_EN[14];
    ap_uint<1>  BATCH_NORM_1_EN       = LAYER_EN[15];

    	//	//	///inst5	//	//	//	//	//	//	///
    ap_uint<16> LAYER_CONV_TYPE   = inst5(32*0+15, 32*0);
    ap_uint<8>  FILTER_D1_H       = inst5(32*1+7, 32*1);
    ap_uint<8>  FILTER_D1_W       = inst5(32*1+15, 32*1+8);
    // cout<<"FILTER_S2_H: "<<FILTER_S2_H<<" FILTER_S2_W: "<<FILTER_S2_W<<endl;
    // cout<<"FILTER_D1_H: "<<FILTER_D1_H<<" FILTER_D1_W: "<<FILTER_D1_W<<endl;
    // exit( 0 );
			// ap_uint<32> KH_KW 			    = inst5(32*3+31, 32*3);
    // ap_uint<32> KH              = inst5(32*5+31, 32*4+16);


	#ifdef DEBUG_config
		cout << LAYER_IN_NUM_HW << " " << LAYER_OUT_NUM_HW << " " << LAYER_IN_H_HW << " " << LAYER_IN_W_HW << " " << LAYER_OUT_H_HW << " " << LAYER_OUT_W_HW << endl;
		cout << LAYER_IN_NUM << " " << LAYER_OUT_NUM << " " << LAYER_IN_H << " " << LAYER_IN_W << " " << LAYER_OUT_H << " " << LAYER_OUT_W << endl;
		cout << CIN_OFFSET << " " << WEIGHT_OFFSET << " " << BIAS_OFFSET << " " << COUT_OFFSET << " " << FILTER_S1 << " " << FILTER_S2 << " " << STRIDE << endl;
		cout << LAYER_EN << " " << PREV_CIN_OFFSET << " " << LAYER_IN_NUM_T << " " << LAYER_OUT_NUM_T << " " << LAYER_IN_H_T << " " << LAYER_IN_W_T << endl;
	#endif
	// 	#define DEBUG_weight
			// Set up some configuration signals
		bool norm_conv_en = (CONV_EN == 1 && BATCH_NORM_EN == 1);
    ap_uint<8> FILTER_H = (LAYER_CONV_TYPE == 2)? FILTER_D1_H : FILTER_S2_H;
    ap_uint<8> FILTER_W = (LAYER_CONV_TYPE == 2)? FILTER_D1_W : FILTER_S2_W;
    // cout<<FILTER_D1<<" "<<FILTER_S2<<" "<<FILTER<<endl;
			// Set the offsets if batch normalization is used (final_result = gamma * computed_result + beta)
			// Depthwise separable convolution has two sublayers of computation,
			// one is the DW sublayer and the other is the normal 1x1 conv sublayer
			// Both of these layers may need normalization
			// In DRAM, for each layer, first the BETAs are stored and then the GAMMAs are stored


			// offsets
		uint weight_offset2 = 0;
      
		weight_offset2 = WEIGHT_OFFSET;

			// Load weights of the conv module
		if (CONV_EN == 1){
			uint global_weight_offset = weight_offset2 + out_num_iter * LAYER_IN_NUM_HW *  FILTER_H * FILTER_W + in_num_iter * LAYER_OUT_NUM_T * FILTER_H * FILTER_W; 	//	//this may need a fix for TCONV
      memcpy((void*)&weight_burst_buf2[0], (void*)&global_weight[global_weight_offset / BUS_PACK_FACTOR1], sizeof(data_t1) * LAYER_OUT_NUM_T * LAYER_IN_NUM_T *  FILTER_H * FILTER_W);
		}

			// Fill the FIFOs with the loaded data
			// weight_load_depth_conv_weight_write(weight_burst_buf1, fifo_depth_conv_weight, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter);

	#ifdef DEBUG_weight
		cout << "loaded weights" << endl;
	#endif

    weight_load_conv_weight_write(weight_burst_buf2, fifo_conv_weight, inst0, inst1, inst3,  FILTER_H, FILTER_W, in_num_iter, out_num_iter);

    counter++;
    
    
	#ifdef DEBUG_weight
		cout << "loaded weights" << endl;
	#endif

			// weight_load_depth_norm_write(beta_depth_burst_buf, fifo_beta_depth, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter);
			// weight_load_depth_norm_write(gamma_depth_burst_buf, fifo_gamma_depth, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter);
	#ifdef DEBUG_weight
		cout << in_num_iter << " in num iter " << endl;
	#endif
			// Repeat until all the tiles are read
			// Then, have to repeat reading to calculate all LAYER_OUT_NUM output feature maps
		in_num_iter += LAYER_IN_NUM_T;
		if (in_num_iter >= LAYER_IN_NUM){
			in_num_iter = 0;
			in_h_iter += LAYER_IN_H_T;
			if (in_h_iter >= LAYER_IN_H){
				in_h_iter = 0;
				in_w_iter += LAYER_IN_W_T;
				if (in_w_iter >= LAYER_IN_W){
					in_w_iter = 0;
					out_num_iter += LAYER_OUT_NUM_T;
					if (out_num_iter >= LAYER_OUT_NUM){
						out_num_iter = 0;
						layer_iter += 1;
						layer_start = 1;
						if (layer_iter == LAYER_BATCH){
							layer_iter = 0;
							done = 1;
						}
					}
				}
			}
		}
	}
    inst++;
    if(inst == inst_count){
      inst_done = 1;
    }
  }
}
/*
 * Function name: weight_load_bias_write
 * Function description: This function writes bias to relu module.
 */
void weight_load_bias_write(
		bus_t2 bias_burst_buf[],
		tapa::ostream<WeightLoadData2Type> &fifo_bias,
		ConfigInst inst0,
		ConfigInst inst1,
		ConfigInst inst2,
		ConfigInst inst3,
		uint in_num_iter,
		uint out_num_iter,
    bool relu_before_conv_flag
){
		// inst0
	ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
	ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
	ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
	ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
	ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
	ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
		// inst1
	ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
	ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
	ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
	ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
	ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
	ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
		// inst2
	ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
	ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
	ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
	ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
	ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
	ap_uint<16> FILTER_S2        = inst2(32*4+31, 32*4+16);
	ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);
		// inst3
	ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
	ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
	ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
	ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
	ap_uint<32> LAYER_IN_H_T     = inst3(32*3+31, 32*3);
	ap_uint<32> LAYER_IN_W_T     = inst3(32*4+31, 32*4);

	ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
	ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
	ap_uint<1>  CONV_EN        = LAYER_EN[2];
	ap_uint<1>  RELU_EN        = LAYER_EN[3];
	ap_uint<1>  RELU6_EN       = LAYER_EN[4];
	ap_uint<1>  POOL_EN        = LAYER_EN[5];
	ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved

  ap_uint<32> out_iter = relu_before_conv_flag? LAYER_IN_NUM_T : LAYER_OUT_NUM_T;
	if (CONV_EN == 1){
		// if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM || relu_before_conv_flag){
    	// if (in_num_iter + LAYER_IN_NUM_T <= LAYER_IN_NUM){
			bias_write_loop: for (int oo = 0; oo < out_iter / SIMD_LANE; oo++){
	#pragma HLS PIPELINE II=1
				uint local_b_idx = oo * SIMD_LANE;
				uint bus_b_idx = local_b_idx / BUS_PACK_FACTOR2;
				uint bus_b_offset = local_b_idx % BUS_PACK_FACTOR2;
				bus_t2 bus_b_data = bias_burst_buf[bus_b_idx];
				WeightLoadData2Type fifo_b_data;
				
	// DATA_SEL_FACTOR = BUS_PACK_FACTOR / SIMD_LANE
	// BUS_PACK_FACTOR is the number of elements packed in one to enable memory coalescing
	// Since each entry in FIFOs will be SIMD_LANE elements of the data, we should unpack based on SIMD_LANE
	#if DATA_SEL_FACTOR2 == 1
				fifo_b_data = bus_b_data;
	#elif DATA_SEL_FACTOR2 == 2
				switch(bus_b_offset / SIMD_LANE){
				case 0:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 1 - 1, DATA_W2 * SIMD_LANE * 0);
					break;
				case 1:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 2 - 1, DATA_W2 * SIMD_LANE * 1);
					break;
				}
	#elif DATA_SEL_FACTOR2 == 4
				switch(bus_b_offset / SIMD_LANE){
				case 0:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 1 - 1, DATA_W2 * SIMD_LANE * 0);
					break;
				case 1:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 2 - 1, DATA_W2 * SIMD_LANE * 1);
					break;
				case 2:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 3 - 1, DATA_W2 * SIMD_LANE * 2);
					break;
				case 3:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 4 - 1, DATA_W2 * SIMD_LANE * 3);
					break;
				}
	#elif DATA_SEL_FACTOR2 == 8
				switch(bus_b_offset / SIMD_LANE){
				case 0:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 1 - 1, DATA_W2 * SIMD_LANE * 0);
					break;
				case 1:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 2 - 1, DATA_W2 * SIMD_LANE * 1);
					break;
				case 2:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 3 - 1, DATA_W2 * SIMD_LANE * 2);
					break;
				case 3:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 4 - 1, DATA_W2 * SIMD_LANE * 3);
					break;
				case 4:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 5 - 1, DATA_W2 * SIMD_LANE * 4);
					break;
				case 5:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 6 - 1, DATA_W2 * SIMD_LANE * 5);
					break;
				case 6:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 7 - 1, DATA_W2 * SIMD_LANE * 6);
					break;
				case 7:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 8 - 1, DATA_W2 * SIMD_LANE * 7);
					break;
				}
	#elif DATA_SEL_FACTOR2 == 16
				switch(bus_b_offset / SIMD_LANE){
				case 0:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 1 - 1, DATA_W2 * SIMD_LANE * 0);
					break;
				case 1:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 2 - 1, DATA_W2 * SIMD_LANE * 1);
					break;
				case 2:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 3 - 1, DATA_W2 * SIMD_LANE * 2);
					break;
				case 3:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 4 - 1, DATA_W2 * SIMD_LANE * 3);
					break;
				case 4:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 5 - 1, DATA_W2 * SIMD_LANE * 4);
					break;
				case 5:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 6 - 1, DATA_W2 * SIMD_LANE * 5);
					break;
				case 6:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 7 - 1, DATA_W2 * SIMD_LANE * 6);
					break;
				case 7:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 8 - 1, DATA_W2 * SIMD_LANE * 7);
					break;
				case 8:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 9 - 1, DATA_W2 * SIMD_LANE * 8);
					break;
				case 9:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 10 - 1, DATA_W2 * SIMD_LANE * 9);
					break;
				case 10:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 11 - 1, DATA_W2 * SIMD_LANE * 10);
					break;
				case 11:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 12 - 1, DATA_W2 * SIMD_LANE * 11);
					break;
				case 12:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 13 - 1, DATA_W2 * SIMD_LANE * 12);
					break;
				case 13:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 14 - 1, DATA_W2 * SIMD_LANE * 13);
					break;
				case 14:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 15 - 1, DATA_W2 * SIMD_LANE * 14);
					break;
				case 15:
					fifo_b_data = bus_b_data(DATA_W2 * SIMD_LANE * 16 - 1, DATA_W2 * SIMD_LANE * 15);
					break;
				}
	#endif       
				fifo_bias.write(fifo_b_data);
			}
		// }
	}
}

void bias_load(
    uint start_inst, uint end_inst,
		bus_mem_2                           global_bias,
		tapa::istream<ConfigInst>          &fifo_config_in,
		// hls::stream<ConvData0Type>       &fifo_gamma_conv_in,
		// hls::stream<ConvData0Type>       &fifo_beta_conv_in,
    tapa::ostream<ConvData0Type>       &fifo_gamma_conv_out,
    tapa::ostream<ConvData0Type>       &fifo_beta_conv_out,
		tapa::ostream<ConfigInst>          &fifo_config_out
){
  cout<<"bias_load"<<endl;
	#pragma HLS INLINE off 
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
		// on-chip buffers
	//  bus_t2 beta_conv_burst_buf_in[IN_NUM_T / BUS_PACK_FACTOR2];
	//  bus_t2 gamma_conv_burst_buf_in[IN_NUM_T / BUS_PACK_FACTOR2];
   bus_t2 beta_conv_burst_buf_out[MAX_OUT_NUM_T / BUS_PACK_FACTOR2];
	 bus_t2 gamma_conv_burst_buf_out[MAX_OUT_NUM_T / BUS_PACK_FACTOR2];

	#if bias_load_MEM == 0
		#pragma HLS bind_storage variable=beta_conv_burst_buf_out type=RAM_T2P impl=BRAM
		#pragma HLS bind_storage variable=gamma_conv_burst_buf_out type=RAM_T2P impl=BRAM
	#elif bias_load_MEM == 1
		#pragma HLS bind_storage variable=beta_conv_burst_buf_out type=RAM_T2P impl=URAM
		#pragma HLS bind_storage variable=gamma_conv_burst_buf_out type=RAM_T2P impl=URAM
	#endif

		// tiling iterators
	uint in_num_iter = 0;
	uint out_num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

		// Read instructions
	ConfigInst inst0 = fifo_config_in.read();
	fifo_config_out.write(inst0);
	ConfigInst inst1 = fifo_config_in.read();
	fifo_config_out.write(inst1);
	ConfigInst inst2 = fifo_config_in.read();
	fifo_config_out.write(inst2);
	ConfigInst inst3 = fifo_config_in.read();
	fifo_config_out.write(inst3);
	ConfigInst inst4 = fifo_config_in.read();
	fifo_config_out.write(inst4);
  ConfigInst inst5 = fifo_config_in.read();
	fifo_config_out.write(inst5);

	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);

  	// fifo_config_out.write(inst0);
		// fifo_config_out.write(inst1);
		// fifo_config_out.write(inst2);
		// fifo_config_out.write(inst3);
		// fifo_config_out.write(inst4);
  	// fifo_config_out.write(inst5);

	bool layer_start = 0;
	bool done = 0;
  int count = 0;
		// We assum that cin has been pre-padded with zeros
	while(!done){
		if (layer_start){
			inst0 = fifo_config_in.read();
			fifo_config_out.write(inst0);
			inst1 = fifo_config_in.read();
			fifo_config_out.write(inst1);
			inst2 = fifo_config_in.read();
			fifo_config_out.write(inst2);
			inst3 = fifo_config_in.read();
			fifo_config_out.write(inst3);
			inst4 = fifo_config_in.read();
			fifo_config_out.write(inst4);
      inst5 = fifo_config_in.read();
			fifo_config_out.write(inst5);
			layer_start = 0;
		}

			// Refer to cin_load module to understand the meaning of the instructions
			// inst0
		ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
			// inst1
		ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
			// inst2
		ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
		ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
		ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
		ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
		ap_uint<16> FILTER_S2        = inst2(32*4+31, 32*4+16);
		ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);
			// inst3
		ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
		ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
		ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
		ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
		ap_uint<32> LAYER_IN_H_T     = inst3(32*3+31, 32*3);
		ap_uint<32> LAYER_IN_W_T     = inst3(32*4+31, 32*4);

		ap_uint<1>  CONV_1ST_EN           = LAYER_EN[0];
		ap_uint<1>  DEPTH_CONV_EN         = LAYER_EN[1];
		ap_uint<1>  CONV_EN               = LAYER_EN[2];
		ap_uint<1>  RELU_EN               = LAYER_EN[3];
		ap_uint<1>  RELU6_EN              = LAYER_EN[4];
		ap_uint<1>  POOL_EN               = LAYER_EN[5];
		ap_uint<1>  UP_SAMPLE_EN          = LAYER_EN[6];  	// reserved
    ap_uint<1>  BIAS_EN               = LAYER_EN[7];
		ap_uint<1>  BATCH_NORM_EN         = LAYER_EN[10];
    ap_uint<1>  BATCH_NORM_EN_DEPTH   = LAYER_EN[12];
    ap_uint<1>  RELU_1_EN             = LAYER_EN[13];
    ap_uint<1>  BIAS_1_EN             = LAYER_EN[14];
    ap_uint<1>  BATCH_NORM_1_EN       = LAYER_EN[15];

    	//	//	///inst5	//	//	//	//	//	//	///
		ap_uint<16> LAYER_CONV_TYPE = inst5(32*0+15, 32*0);


	#ifdef DEBUG_config
		cout << LAYER_IN_NUM_HW << " " << LAYER_OUT_NUM_HW << " " << LAYER_IN_H_HW << " " << LAYER_IN_W_HW << " " << LAYER_OUT_H_HW << " " << LAYER_OUT_W_HW << endl;
		cout << LAYER_IN_NUM << " " << LAYER_OUT_NUM << " " << LAYER_IN_H << " " << LAYER_IN_W << " " << LAYER_OUT_H << " " << LAYER_OUT_W << endl;
		cout << CIN_OFFSET << " " << WEIGHT_OFFSET << " " << BIAS_OFFSET << " " << COUT_OFFSET << " " << FILTER_S1 << " " << FILTER_S2 << " " << STRIDE << endl;
		cout << LAYER_EN << " " << PREV_CIN_OFFSET << " " << LAYER_IN_NUM_T << " " << LAYER_OUT_NUM_T << " " << LAYER_IN_H_T << " " << LAYER_IN_W_T << endl;
	#endif
	// 	#define DEBUG_weight
			// Set up some configuration signals
    	//	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#in bias load	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#
		// bool bias_en_1 = (CONV_EN == 1 && BIAS_1_EN == 1);
		// bool norm_conv_en_1 = (CONV_EN == 1 && BATCH_NORM_1_EN == 1);
		// uint beta_conv_offset_1 = 0;
		// uint gamma_conv_offset_1 = 0;
		// uint bias_offset_1 = BIAS_OFFSET;
    // if (norm_conv_en_1) {
		// 	beta_conv_offset_1 = bias_offset_1;
    //   if(LAYER_IN_NUM_HW<BUS_PACK_FACTOR2)
		// 	  gamma_conv_offset_1 = beta_conv_offset_1 + BUS_PACK_FACTOR2;
    //   else
    //     gamma_conv_offset_1 = beta_conv_offset_1 + LAYER_IN_NUM_HW;	//tweak
		// }
    
    	//	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#out bias load	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#
    bool bias_en = (CONV_EN == 1 && BIAS_EN == 1);
    // cout<<"bias_en: "<<bias_en<<endl;
		bool norm_conv_en = (CONV_EN == 1 && BATCH_NORM_EN == 1);  
    uint beta_conv_offset = 0;
		uint gamma_conv_offset = 0;
    uint bias_offset = BIAS_OFFSET;
    // if(LAYER_IN_NUM_HW<BUS_PACK_FACTOR2){
    //   if(BATCH_NORM_1_EN)
    //     bias_offset = BIAS_OFFSET + 2*BUS_PACK_FACTOR2;
    //   else if(BIAS_1_EN)
    //     bias_offset = BUS_PACK_FACTOR2;
    //   else
    //     bias_offset =  BIAS_OFFSET;
    // }else{
    //   if(BATCH_NORM_1_EN)
    //     bias_offset = BIAS_OFFSET + 2*LAYER_IN_NUM_HW;
    //   else if(BIAS_1_EN)
    //     bias_offset = LAYER_IN_NUM_HW;
    //   else
    //     bias_offset =  BIAS_OFFSET;
    // }

    	// cout<<beta_conv_offset<<" "<<gamma_conv_offset<<endl;

    	//	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#in bias load	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#
			// Load bias (when batch normalization is not used: final_result = computed_result + bias)
			// Set GAMMAs to zero
		// if (bias_en_1){
		// 		// Only write out in the last iteration
		// 		// if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM){
		// 		uint global_bias_offset_1 = bias_offset_1 + out_num_iter;
    //     for (int i = 0; i < OUT_NUM_T / BUS_PACK_FACTOR2; i++){
    //     	#pragma HLS pipeline
		// 		  gamma_conv_burst_buf_in[i] = 0;
    //     }
		// 		memcpy((void*)beta_conv_burst_buf_in, (void*)&global_bias[global_bias_offset_1 / BUS_PACK_FACTOR2], sizeof(data_t2) * LAYER_IN_NUM_T);
		// 		// }
		// } else{

    //   		// Load batch normalization info for conv
    //   	if (norm_conv_en_1){
    //   			// if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM){	//those may need to change
    //   			uint global_beta_offset_1 = beta_conv_offset_1 + in_num_iter;
    //   	#ifdef DEBUG_weight
    //   			cout << global_beta_offset_1 << " beta " << beta_conv_offset_1 << " " <<BUS_PACK_FACTOR2<<endl;
    //   	#endif
    //   			memcpy((void*)beta_conv_burst_buf_in, (void*)&global_bias[global_beta_offset_1 / BUS_PACK_FACTOR2], sizeof(data_t2) * LAYER_IN_NUM_T);
    //   			// }

    //   			// if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM){	//those may need to change
    //   			uint global_gamma_offset_1 = gamma_conv_offset_1 + in_num_iter;
    //   	#ifdef DEBUG_weight
    //   			cout << global_gamma_offset_1 << " gamma " << gamma_conv_offset_1 << " " <<BUS_PACK_FACTOR2<<endl;

    //   	#endif
    //   			memcpy((void*)gamma_conv_burst_buf_in, (void*)&global_bias[global_gamma_offset_1 / BUS_PACK_FACTOR2], sizeof(data_t2) * LAYER_IN_NUM_T);
    //   			// }
    //   	}
    // }
    	//	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#out bias load	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#
    if (bias_en){
				// Only write out in the last iteration
			// if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM){
				uint global_bias_offset = bias_offset + out_num_iter;
        for (int i = 0; i < MAX_OUT_NUM_T / BUS_PACK_FACTOR2; i++){
        	#pragma HLS pipeline
				  gamma_conv_burst_buf_out[i] = 0;
        }
				memcpy((void*)beta_conv_burst_buf_out, (void*)&global_bias[global_bias_offset / BUS_PACK_FACTOR2], sizeof(data_t2) * LAYER_OUT_NUM_T);
			// }
		} //else{

    //   		// Load batch normalization info for conv
    //   	if (norm_conv_en){
    //   		if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM){
    //   			uint global_bias_offset = beta_conv_offset + out_num_iter;
    //   	#ifdef DEBUG_weight
    //   			cout << global_bias_offset << " beta " << beta_conv_offset << endl;
    //   	#endif
    //   			memcpy((void*)beta_conv_burst_buf_out, (void*)&global_bias[global_bias_offset / BUS_PACK_FACTOR2], sizeof(data_t2) * LAYER_OUT_NUM_T);
    //   		}
      
    //   		if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM){
    //   			uint global_bias_offset = gamma_conv_offset + out_num_iter;
    //   	#ifdef DEBUG_weight
    //   			cout << global_bias_offset << " gamma " << gamma_conv_offset << endl;
    //   	#endif
    //   			memcpy((void*)gamma_conv_burst_buf_out, (void*)&global_bias[global_bias_offset / BUS_PACK_FACTOR2], sizeof(data_t2) * LAYER_OUT_NUM_T);
    //   		}
    //   	}
    // }
    	// exit(0);
	#ifdef DEBUG_weight
		cout << "loaded beta and gamma" << endl;
	#endif

		// Load BETAs and GAMMAs to their FIFOs
		// If there doesn't exist a batch normalization and it's a normal bias,
		// beta = bias, gamma = 0
    	//	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#in bias load	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#
    // if (bias_en_1) {
		//   weight_load_bias_write(beta_conv_burst_buf_in, fifo_beta_conv_in, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter,1);
    //   weight_load_bias_write(gamma_conv_burst_buf_in, fifo_gamma_conv_in, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter,1);
    // }
		// else if(norm_conv_en_1){
		// 	weight_load_bias_write(beta_conv_burst_buf_in, fifo_beta_conv_in, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter,1);
		// 	weight_load_bias_write(gamma_conv_burst_buf_in, fifo_gamma_conv_in, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter,1);
		// }

    	//	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#out bias load	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#	#
    if (bias_en) {
		  weight_load_bias_write(beta_conv_burst_buf_out, fifo_beta_conv_out, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter, 0);
      weight_load_bias_write(gamma_conv_burst_buf_out, fifo_gamma_conv_out, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter, 0);
    }
		// else if(norm_conv_en){
		// 	weight_load_bias_write(beta_conv_burst_buf_out, fifo_beta_conv_out, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter, 0);
		// 	weight_load_bias_write(gamma_conv_burst_buf_out, fifo_gamma_conv_out, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter, 0);
		// }

	#ifdef DEBUG_weight
		cout << in_num_iter << " in num iter " << endl;
	#endif
			// Repeat until all the tiles are read
			// Then, have to repeat reading to calculate all LAYER_OUT_NUM output feature maps
		// in_num_iter += LAYER_IN_NUM_T;
		// if (in_num_iter >= LAYER_IN_NUM){
		// 	in_num_iter = 0;
			in_h_iter += LAYER_IN_H_T;
			if (in_h_iter >= LAYER_IN_H){
				in_h_iter = 0;
				in_w_iter += LAYER_IN_W_T;
				if (in_w_iter >= LAYER_IN_W){
					in_w_iter = 0;
					out_num_iter += LAYER_OUT_NUM_T;
					if (out_num_iter >= LAYER_OUT_NUM){
						out_num_iter = 0;
						layer_iter += 1;
						layer_start = 1;
						if (layer_iter == LAYER_BATCH){
							layer_iter = 0;
							done = 1;
						}
					}
				}
			}
		// }
	}
  inst++;
  if(inst == inst_count){
    inst_done = 1;
  }
  }
}/*
 * Function name: cin_load_fifo_write_prev
 * Function description: This function writes cin data to the downstream modules.
 * It has the same functionality as cin_load_fifo_write
 */
void cin_load_fifo_write_prev(
		bus_t0                         cin_burst_buf[],
		tapa::ostream<CinLoadData0Type>  &fifo_cin,
		uint                           LAYER_IN_NUM_T,
		uint                           LAYER_IN_H_T,
		uint                           LAYER_IN_W_T,
		uint                           FILTER_S,
    bool                           pad_en
){
	int ii = 0;
	int hh = 0;
	int ww = 0;
	bool done = 0;
	while(!done){
	#pragma HLS PIPELINE II=1
	// Data layout of the buffer: Th * Tw * Tn
		uint local_cin_idx = hh * (LAYER_IN_W_T + FILTER_S - 1) * LAYER_IN_NUM_T + ww * LAYER_IN_NUM_T + ii * DEPTH_CONV_LANE;
		uint bus_cin_idx = local_cin_idx / BUS_PACK_FACTOR0;
		uint bus_cin_offset = local_cin_idx % BUS_PACK_FACTOR0;
		bus_t0 bus_cin_data = pad_en? (bus_t0) 0 : cin_burst_buf[bus_cin_idx];
		CinLoadData0Type fifo_cin_data;

	// DATA_SEL_FACTOR = BUS_PACK_FACTOR / SIMD_LANE
	// BUS_PACK_FACTOR is the number of elements packed in one to enable memory coalescing
	// Since each entry is FIFOs will be SIMD_LANE elements of the data, we should unpack based on SIMD_LANE
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
	#ifdef DEBUG_load_prev
		for (int lane = 0; lane < RELU_LANE; lane++){
	#pragma HLS UNROLL
			ap_uint<DATA_W0> u32_beta = fifo_cin_data(DATA_W0 - 1, 0);
			data_t2 a = Reinterpret<data_t2>(u32_beta);
			fifo_cin_data = fifo_cin_data >> DATA_W0;
			cout << a << " ";
		}
		cout << endl;
	#endif
		
			// Repeat until the whole tile is read
		ww++;
		if (ww == LAYER_IN_W_T + FILTER_S - 1){
			ww = 0;
			hh++;
			if (hh == LAYER_IN_H_T + FILTER_S - 1){
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

void cin_load_prev(
    uint start_inst, uint end_inst,
		bus_mem_0                      global_cin,
		tapa::istream<ConfigInst>        &fifo_config_in,
		tapa::ostream<CinLoadData0Type>  &fifo_cin_prev,
		tapa::ostream<ConfigInst>        &fifo_config_out
){
  cout<<"cin_load_prev"<<endl;
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
	#pragma HLS INLINE off 
		// on-chip buffer for cin data
	bus_t0 prev_cin_burst_buf_ping[CIN_PREV_BUFF / BUS_PACK_FACTOR0];
	bus_t0 prev_cin_burst_buf_pong[CIN_PREV_BUFF / BUS_PACK_FACTOR0];
	#if cin_load_prev_MEM == 0
		#pragma HLS bind_storage variable=prev_cin_burst_buf_ping type=RAM_T2P impl=BRAM
		#pragma HLS bind_storage variable=prev_cin_burst_buf_pong type=RAM_T2P impl=BRAM  
	#elif cin_load_prev_MEM == 1
		#pragma HLS bind_storage variable=prev_cin_burst_buf_ping type=RAM_T2P impl=URAM
		#pragma HLS bind_storage variable=prev_cin_burst_buf_pong type=RAM_T2P impl=URAM  
	#endif


		// tiling iterators
	// uint in_num_iter = 0;
	// uint out_num_iter = 0;
  uint num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;
  bool pad_en = 0;

	// uint in_num_iter_prev = 0;
	// uint out_num_iter_prev = 0;
  uint num_iter_prev = 0;
	uint in_h_iter_prev = 0;
	uint in_w_iter_prev = 0;
	uint layer_iter_prev = 0;
  bool pad_en_prev = 0;


	uint LAYER_NUM_T_prev;
	// uint LAYER_OUT_NUM_T_prev;
	uint LAYER_IN_H_T_prev;
	uint LAYER_IN_W_T_prev;
	uint FILTER_prev;

	ConfigInst inst0 = fifo_config_in.read();
	fifo_config_out.write(inst0);
	ConfigInst inst1 = fifo_config_in.read();
	fifo_config_out.write(inst1);
	ConfigInst inst2 = fifo_config_in.read();
	fifo_config_out.write(inst2);
	ConfigInst inst3 = fifo_config_in.read();
	fifo_config_out.write(inst3);
	ConfigInst inst4 = fifo_config_in.read();
	fifo_config_out.write(inst4);
  ConfigInst inst5 = fifo_config_in.read();
	fifo_config_out.write(inst5);

	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);
	ap_uint<1>  LOAD_PREV_CIN = 0;


	uint task_cnt = 0;
	bool layer_start = 0;
	bool layer_start_prev = 0;
	bool done = 0;
		// We assum that cin has been pre-padded with zeros
	uint prev = 0;
	uint init = 1;
	uint num_tile = 0;
  uint channel_iter = 0;
  uint inter_tile = 0;
	bool write_last_cin = 0;
	bool write_last_prev_cin = 0;
	bool start_prev = 0;
	bool done_prev = 0;
	bool change_layout = 0;
		//uint prev_iter = 0;
	while(!done_prev){

		if (layer_start){
			inst0 = fifo_config_in.read();
			fifo_config_out.write(inst0);
			inst1 = fifo_config_in.read();
			fifo_config_out.write(inst1);
			inst2 = fifo_config_in.read();
			fifo_config_out.write(inst2);
			inst3 = fifo_config_in.read();
			fifo_config_out.write(inst3);
			inst4 = fifo_config_in.read();
			fifo_config_out.write(inst4);
			inst5 = fifo_config_in.read();
			fifo_config_out.write(inst5);
			layer_start = 0;
		}

			// Refer to cin_load module to understand the meaning of the instructions
			// inst0
		ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
			// inst1
		ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
			// inst2
		ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
		ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
		ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
		// ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
		// ap_uint<16> FILTER_S2        = inst2(32*4+31, 32*4+16);
		// ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);
			// inst3
		ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
		ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
		ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
		ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);


    	// ap_uint<16> LAYER_CONV_TYPE 		= inst5(32*0+15, 32*0);
    	// ap_uint<32> KH              		= inst5(32*5+31, 32*4+16);
    ap_uint<16> LAYER_TCONV_STRIDE 	= inst5(32*2+15, 32*2);
    ap_uint<32> LAYER_IN_H_T    		= inst3(32*3+31, 32*3)*LAYER_TCONV_STRIDE;
		ap_uint<32> LAYER_IN_W_T    		= inst3(32*4+31, 32*4)*LAYER_TCONV_STRIDE;

    ap_uint<16> POOL_NUM 					  = inst5(32*5+16, 32*5);
    ap_uint<16> POOL_NUM_HW 				= inst5(32*5+31, 32*5+16);


		ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
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
    ap_uint<1>  ADD_EN         = LAYER_EN[17];
		LOAD_PREV_CIN  = LAYER_EN[11];

    LAYER_IN_W = ADD_EN && POOL_EN? (ap_uint<32>) (LAYER_IN_W<<1) :  (ap_uint<32>) LAYER_IN_W;
    LAYER_IN_H = ADD_EN && POOL_EN? (ap_uint<32>) (LAYER_IN_H<<1) :  (ap_uint<32>) LAYER_IN_H;
    LAYER_IN_W_T = ADD_EN && POOL_EN? (ap_uint<32>) (LAYER_IN_W_T<<1) :  (ap_uint<32>) LAYER_IN_W_T;
    LAYER_IN_H_T = ADD_EN && POOL_EN? (ap_uint<32>) (LAYER_IN_H_T<<1) :  (ap_uint<32>) LAYER_IN_H_T;
    ap_uint<32> LAYER_NUM_T = ADD_EN? LAYER_OUT_NUM_T : LAYER_IN_NUM_T;

	// #ifdef DEBUG_config_cin
	// 	cout << LAYER_IN_NUM_HW << " " << LAYER_OUT_NUM_HW << " " << LAYER_IN_H_HW << " " << LAYER_IN_W_HW << " " << LAYER_OUT_H_HW << " " << LAYER_OUT_W_HW << endl;
	// 	cout << LAYER_IN_NUM << " " << LAYER_OUT_NUM << " " << LAYER_IN_H << " " << LAYER_IN_W << " " << LAYER_OUT_H << " " << LAYER_OUT_W << endl;
	// 	cout << CIN_OFFSET << " " << WEIGHT_OFFSET << " " << BIAS_OFFSET << " " << COUT_OFFSET << " " << FILTER_S1 << " " << FILTER_S2 << " " << STRIDE << endl;
	// 	cout << LAYER_EN << " " << PREV_CIN_OFFSET << " " << LAYER_IN_NUM_T << " " << LAYER_OUT_NUM_T << " " << LAYER_IN_H_T << " " << LAYER_IN_W_T << endl;

	// #endif


			// offsets
		uint cin_offset = CIN_OFFSET;
		uint prev_cin_offset = PREV_CIN_OFFSET;
    	// ap_uint<4>  FILTER = (LAYER_CONV_TYPE == 1)? (ap_uint<4>)(KH_KW>>28) : (ap_uint<4>) FILTER_S2;

		if (prev == 1) start_prev = 1;

			// set up some configuration signals
		ap_uint<8> FILTER = 1;//(DEPTH_CONV_EN == 1)? (uint)FILTER_S1: ((CONV_EN == 1)? (uint)FILTER_S2: 1);
		bool separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
		bool conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
		bool max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);
		change_layout = 1;//(((LAYER_OUT_W_HW == LAYER_OUT_W) || (LAYER_OUT_W_HW == LAYER_IN_W_T)) && ((LAYER_OUT_H_HW == LAYER_OUT_H) || (LAYER_OUT_H_HW == LAYER_IN_H_T))); 	// if next filter = 1 : change the layout to num_tile, h_t, w_t, in_t
		uint layer_out_h = LAYER_IN_H;//(LAYER_IN_H_T > LAYER_OUT_H) ? LAYER_IN_H_T : LAYER_OUT_H;
		uint layer_out_w = LAYER_IN_W;//(LAYER_IN_W_T > LAYER_OUT_W) ? LAYER_IN_W_T : LAYER_OUT_W;

    pad_en = (num_iter_prev >= POOL_NUM); 
		if (INTER_LOAD_EN == 0){
			// if (((max_pool || UP_SAMPLE_EN) && out_num_iter_prev == 0) || separable_conv || conv2d){
				if (task_cnt == 0){
						// First load previous cin
						// Load only if the enable signal for this module is on
					if(LOAD_PREV_CIN == 1 && !pad_en){
						cin_load_ddr_read(global_cin, prev_cin_burst_buf_ping, layer_out_h, layer_out_w, LAYER_NUM_T, LAYER_IN_H_T, LAYER_IN_W_T, FILTER, FILTER, FILTER, FILTER, 1, prev_cin_offset, num_iter_prev, in_h_iter_prev, in_w_iter_prev, num_tile, change_layout, max_pool, 1);
            // cout<<"prev_cin_burst_buf_ping: "<<pad_en<<endl;
            // print<256>(prev_cin_burst_buf_ping[0]);
          } 
				} else {
						// Apply double buffering for reading the data and filling the FIFO
						// Load only if the enable signal for this module is on
					if(LOAD_PREV_CIN == 1){
						if (task_cnt % 2 == 1){
              if(!pad_en){
							  cin_load_ddr_read(global_cin, prev_cin_burst_buf_pong, layer_out_h, layer_out_w, LAYER_NUM_T, LAYER_IN_H_T, LAYER_IN_W_T, FILTER, FILTER, FILTER, FILTER, 1, prev_cin_offset, num_iter_prev, in_h_iter_prev, in_w_iter_prev, num_tile, change_layout, max_pool, 1);
              }
              cin_load_fifo_write_prev(prev_cin_burst_buf_ping, fifo_cin_prev, LAYER_NUM_T_prev, LAYER_IN_H_T_prev, LAYER_IN_W_T_prev, 1, pad_en_prev);
              // cout<<"cin_load_fifo_write_prev: "<<pad_en_prev<<endl;
              // print<256>(fifo_cin_prev.read());
						} else {
							if(!pad_en){
                cin_load_ddr_read(global_cin, prev_cin_burst_buf_ping, layer_out_h, layer_out_w, LAYER_NUM_T, LAYER_IN_H_T, LAYER_IN_W_T, FILTER, FILTER, FILTER, FILTER, 1, prev_cin_offset, num_iter_prev, in_h_iter_prev, in_w_iter_prev, num_tile, change_layout, max_pool, 1);
              }
              cin_load_fifo_write_prev(prev_cin_burst_buf_pong, fifo_cin_prev, LAYER_NUM_T_prev, LAYER_IN_H_T_prev, LAYER_IN_W_T_prev, 1, pad_en_prev);
						}
					} 
				}

				task_cnt++;
        	// cout<<task_cnt<<endl;
				// LAYER_IN_NUM_T_prev = LAYER_IN_NUM_T;
				// LAYER_OUT_NUM_T_prev = LAYER_OUT_NUM_T;
        LAYER_NUM_T_prev = LAYER_NUM_T;
				LAYER_IN_H_T_prev = LAYER_IN_H_T;
				LAYER_IN_W_T_prev = LAYER_IN_W_T;
        pad_en_prev = pad_en;
					// FILTER_prev = FILTER;
			// }
		}

			// Continue until all tiles are read
			// No need to read tiles multiple times 
			// since it's only read to add them to the result of this layer
		// num_tile++;
    // cout<<"in_num_iter_prev: "<<in_num_iter_prev<<" pad_en: "<<pad_en<<" num_tile: "<<num_tile<<endl;
    // in_num_iter_prev += LAYER_IN_NUM_T;
		// if (in_num_iter_prev < POOL_NUM_HW){ //not sure if this should be POOL_NUM or POOL_NUM_HW
		// 	channel_iter += ((LAYER_IN_W / LAYER_IN_W_T) * (LAYER_IN_H / LAYER_IN_H_T));
		// } else {
		// 	channel_iter = 0;
		// 	inter_tile++;
		// }
		num_tile++;// = channel_iter + inter_tile;

			// channel_iter = 0;
    in_h_iter_prev += LAYER_IN_H_T;
    if (in_h_iter_prev >= LAYER_IN_H){
      in_h_iter_prev = 0;
      in_w_iter_prev += LAYER_IN_W_T;
      if (in_w_iter_prev >= LAYER_IN_W){
        in_w_iter_prev = 0;
        // inter_tile = 0;
        // channel_iter = 0;
        num_iter_prev += LAYER_NUM_T;
        if (num_iter_prev >= POOL_NUM_HW){
          num_iter_prev = 0;
          layer_iter_prev += 1;
          layer_start_prev = 1;
          if (layer_iter_prev == LAYER_BATCH){
            layer_iter_prev = 0;
            num_iter_prev = 0;
            in_h_iter_prev = 0;
            in_w_iter_prev = 0;
            done_prev = 1;
					}
				}
			}
		}
	}
	
	if (LOAD_PREV_CIN){
			// Load the last tile to its FIFO
		if (task_cnt % 2 == 1){
			cin_load_fifo_write_prev(prev_cin_burst_buf_ping, fifo_cin_prev, LAYER_NUM_T_prev, LAYER_IN_H_T_prev, LAYER_IN_W_T_prev, 1, pad_en_prev);
		} else {
			cin_load_fifo_write_prev(prev_cin_burst_buf_pong, fifo_cin_prev, LAYER_NUM_T_prev, LAYER_IN_H_T_prev, LAYER_IN_W_T_prev, 1, pad_en_prev);
		}
	}
  inst++; 
  if(inst == inst_count){
    inst_done = 1;
  }
  }

}/**
 * Function name: pool
 * Function description: This functions performs max-pooling operation.
 */
void pool(
    uint start_inst, uint end_inst,
		tapa::istream<ReluData0Type>  &fifo_cin,
		tapa::istream<ConfigInst>     &fifo_config_in,
		tapa::ostream<PoolData0Type>  &fifo_cout,
		tapa::ostream<ConfigInst>     &fifo_config_out
){
  cout<<"pool"<<endl;
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
  uint num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

		// Read instructions
	ConfigInst inst0 = fifo_config_in.read();
	fifo_config_out.write(inst0);
	ConfigInst inst1 = fifo_config_in.read();
	fifo_config_out.write(inst1);
	ConfigInst inst2 = fifo_config_in.read();
	fifo_config_out.write(inst2);
	ConfigInst inst3 = fifo_config_in.read();
	fifo_config_out.write(inst3);
	ConfigInst inst4 = fifo_config_in.read();
	fifo_config_out.write(inst4);
  ConfigInst inst5 = fifo_config_in.read();
	fifo_config_out.write(inst5);

	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);

	bool layer_start = 0;
	bool done = 0;
	while(!done){

		if (layer_start){
			inst0 = fifo_config_in.read();
			fifo_config_out.write(inst0);
			inst1 = fifo_config_in.read();
			fifo_config_out.write(inst1);
			inst2 = fifo_config_in.read();
			fifo_config_out.write(inst2);
			inst3 = fifo_config_in.read();
			fifo_config_out.write(inst3);
			inst4 = fifo_config_in.read();
			fifo_config_out.write(inst4);
      inst5 = fifo_config_in.read();
	    fifo_config_out.write(inst5);

			layer_start = 0;
		}
		
			// Refer to cin_load module to understand the meaning of the instructions
			// inst0
		ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
			// inst1
		ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
			// inst2
		ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
		ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
		ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
		ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
		ap_uint<16> FILTER_S2        = inst2(32*4+31, 32*4+16);
		ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);
			// inst3
		ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
		ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
		ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
		ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
		ap_uint<32> LAYER_IN_H_T     = inst3(32*3+31, 32*3);
		ap_uint<32> LAYER_IN_W_T     = inst3(32*4+31, 32*4);

    ap_uint<32> LAYER_TCONV_STRIDE 	= inst5(32*2+15, 32*2);
    ap_uint<16> POOL_NUM 					  = inst5(32*5+16, 32*5);
    ap_uint<16> POOL_NUM_HW 				= inst5(32*5+31, 32*5+16);

		ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
		ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
		ap_uint<1>  CONV_EN        = LAYER_EN[2];
		ap_uint<1>  RELU_EN        = LAYER_EN[3];
		ap_uint<1>  RELU6_EN       = LAYER_EN[4];
		ap_uint<1>  POOL_EN        = LAYER_EN[5];
		ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
		ap_uint<1>  BIAS_EN        = LAYER_EN[7];
		ap_uint<1>  LOAD_PREV_CIN  = LAYER_EN[11];
		ap_uint<1>  ADD_EN          = LAYER_EN[17];
    LAYER_IN_W = ADD_EN && POOL_EN? (ap_uint<32>) (LAYER_IN_W<<1) :  (ap_uint<32>) LAYER_IN_W;
    LAYER_IN_H = ADD_EN && POOL_EN? (ap_uint<32>) (LAYER_IN_H<<1) :  (ap_uint<32>) LAYER_IN_H;
    LAYER_IN_W_T = ADD_EN && POOL_EN? (ap_uint<32>) (LAYER_IN_W_T<<1) :  (ap_uint<32>) LAYER_IN_W_T;
    LAYER_IN_H_T = ADD_EN && POOL_EN? (ap_uint<32>) (LAYER_IN_H_T<<1) :  (ap_uint<32>) LAYER_IN_H_T;
    ap_uint<32> LAYER_NUM_T = ADD_EN? LAYER_OUT_NUM_T : LAYER_IN_NUM_T;
    STRIDE = ADD_EN && POOL_EN? (ap_uint<32>) 2 :  (ap_uint<32>) STRIDE;
			// Set up some configuration signals
		bool en = POOL_EN;
		bool separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
		bool conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
		bool max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);
    if(LOAD_PREV_CIN == 1){
		switch(en){
			// bypass this module
		case 0:{
			// if ((UP_SAMPLE_EN && out_num_iter == 0) || (!UP_SAMPLE_EN && in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM)){
				int o = 0;
				int h = 0;
				int w = 0;
				bool done1 = 0;

				int w_bound = (LAYER_IN_W_T / STRIDE)*LAYER_TCONV_STRIDE;
				int h_bound = (LAYER_IN_H_T / STRIDE)*LAYER_TCONV_STRIDE;

				while(!done1){
	#pragma HLS PIPELINE II=1
					PoolData0Type tmp = fifo_cin.read();
					fifo_cout.write(tmp);
					
						// Repeat until the whole tile is read
					w++;
					if (w == w_bound){
						w = 0;
						h++;
						if (h == h_bound){
							h = 0;
							o++;
							if (o == LAYER_NUM_T / POOL_LANE){
								o = 0;
								done1 = 1;
							}
						}
					}
				}
			}
			break;
			// compute
		case 1:
			{
				int o = 0;
				int h = 0;
				int w = 0;
				bool done1 = 0;

				int w_bound = LAYER_IN_W_T;
				int h_bound = LAYER_IN_H_T;
        PoolData0Type line_buf_w_curr[MAX_IN_W_T];
        PoolData0Type line_buf_w_next[MAX_IN_W_T];

        data_t0 pixel_1 = 0;
        data_t0 pixel_2 = 0;
        data_t0 pixel_3 = 0;
        data_t0 pixel_4 = 0;
        PoolData0Type max_buff;

        PoolData0Type line_buf_h;
				pool2 : while(!done1){
          #pragma HLS PIPELINE II=1
          if(h==0){
            line_buf_w_curr[w] = fifo_cin.read();
          }else{
            PoolData0Type tmp = fifo_cin.read();
            line_buf_w_next[w] = tmp;
            if(w%STRIDE == 0){
              line_buf_h = tmp;
            }
            if(h%STRIDE == 0){
              line_buf_w_curr[w] = line_buf_w_next[w];
            }
            if(w%STRIDE == 1 && h%STRIDE == 1){
              pool3 : for(int i=0; i<POOL_LANE; i++){
                #pragma HLS UNROLL
                // pixel 1
                pixel_1 = Reinterpret<data_t0>((ap_uint<DATA_W0>) line_buf_w_curr[w-1]((i+1)*DATA_W0-1, i*DATA_W0));
                // pixel 2
                pixel_2 = Reinterpret<data_t0>((ap_uint<DATA_W0>) line_buf_w_curr[w]((i+1)*DATA_W0-1, i*DATA_W0));
                // pixel 3
                pixel_3 = Reinterpret<data_t0>((ap_uint<DATA_W0>) line_buf_h((i+1)*DATA_W0-1, i*DATA_W0));
                // pixel 4
                pixel_4 = Reinterpret<data_t0>((ap_uint<DATA_W0>) tmp((i+1)*DATA_W0-1, i*DATA_W0));
                // max
                data_t0 val_1 = max(pixel_1, pixel_2);
                data_t0 val_2 = max(pixel_3, pixel_4);
                data_t0 val_3 = max(val_1, val_2);
                max_buff((i+1)*DATA_W0-1, i*DATA_W0) = Reinterpret<ap_uint<DATA_W0> >(val_3);
              }
              fifo_cout.write(max_buff);
            }
          }
          // Repeat until the whole tile is read
					w++;
					if (w == w_bound){
						w = 0;
						h++;
						if (h == h_bound){
							h = 0;
							o++;
							if (o == LAYER_NUM_T / POOL_LANE){
								o = 0;
								done1 = 1;
							}
						}
					}
				}
			}
			break;
		}
		}
		
			// Repeat until all the tiles are read
			// Must repeat the computation until LAYER_OUT_NUM output feature maps are generated
		// in_num_iter += LAYER_IN_NUM_T;
		// if (in_num_iter >= POOL_NUM_HW){
		// 	in_num_iter = 0;
      // not sure if out or in iters to be used
    in_h_iter += LAYER_IN_H_T;
    if (in_h_iter >= LAYER_IN_H){
      in_h_iter = 0;
      in_w_iter += LAYER_IN_W_T;
      if (in_w_iter >= LAYER_IN_W){
        in_w_iter = 0;
        num_iter += LAYER_NUM_T;
        if (num_iter >= POOL_NUM_HW){
          num_iter = 0;
          layer_iter += 1;
          layer_start = 1;
          if (layer_iter == LAYER_BATCH){
            layer_iter = 0;
            done = 1;
          }
        }
      }
    }
		// }
	}
    inst++;
    if(inst == inst_count){
      inst_done = 1;
    }
  }
}void upsample(
    uint start_inst, uint end_inst,
  	tapa::istream<ReluData0Type>  &fifo_cin,
		tapa::istream<ConfigInst>     &fifo_config_in,
		tapa::ostream<PoolData0Type>  &fifo_cout,
		tapa::ostream<ConfigInst>     &fifo_config_out
){
 #pragma HLS INLINE off
  cout<<"upsample"<<endl;
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
		// tiling iterators
	uint in_num_iter = 0;
	uint out_num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

		// Read instructions
	ConfigInst inst0 = fifo_config_in.read();
	fifo_config_out.write(inst0);
	ConfigInst inst1 = fifo_config_in.read();
	fifo_config_out.write(inst1);
	ConfigInst inst2 = fifo_config_in.read();
	fifo_config_out.write(inst2);
	ConfigInst inst3 = fifo_config_in.read();
	fifo_config_out.write(inst3);
	ConfigInst inst4 = fifo_config_in.read();
	fifo_config_out.write(inst4);
  ConfigInst inst5 = fifo_config_in.read();
	fifo_config_out.write(inst5);


	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);

	bool layer_start = 0;
	bool done = 0;
  ConvData0Type line_buf[MAX_IN_W_T];

	while(!done){
		if (layer_start){
			inst0 = fifo_config_in.read();
			fifo_config_out.write(inst0);
			inst1 = fifo_config_in.read();
			fifo_config_out.write(inst1);
			inst2 = fifo_config_in.read();
			fifo_config_out.write(inst2);
			inst3 = fifo_config_in.read();
			fifo_config_out.write(inst3);
			inst4 = fifo_config_in.read();
			fifo_config_out.write(inst4);
      inst5 = fifo_config_in.read();
			fifo_config_out.write(inst5);

			layer_start = 0;
		}

			// Refer to cin_load module to understand the meaning of the instructions
			// inst0
		ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
			// inst1
		ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
			// inst2
		ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
		ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
		ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
		ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
		ap_uint<16> FILTER_S2        = inst2(32*4+31, 32*4+16);
		ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);
			// inst3
		ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
		ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
		ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
		ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
		ap_uint<32> LAYER_IN_H_T     = inst3(32*3+31, 32*3);
		ap_uint<32> LAYER_IN_W_T     = inst3(32*4+31, 32*4);

    ap_uint<16> LAYER_CONV_TYPE 		= inst5(32*0+15, 32*0);
    ap_uint<16> LAYER_TCONV_STRIDE 	= inst5(32*2+15, 32*2);

		ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
		ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
		ap_uint<1>  CONV_EN        = LAYER_EN[2];
		ap_uint<1>  RELU_EN        = LAYER_EN[3];
		ap_uint<1>  RELU6_EN       = LAYER_EN[4];
		ap_uint<1>  POOL_EN        = LAYER_EN[5];
		ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
		ap_uint<1>  BIAS_EN        = LAYER_EN[7];
		ap_uint<1>  BATCH_NORM_EN  = LAYER_EN[10];
		ap_uint<1>  LOAD_PREV_CIN  = LAYER_EN[11];
    ap_uint<1>  BATCH_NORM_EN_DEPTH  = LAYER_EN[12];


			// Set up some configuration signals
		uint upsample_factor = 2;
    uint en = UP_SAMPLE_EN;
	#ifdef DEBUG
		uint relu_cout_cnt = 0;
		ofstream relu_data;
		relu_data.open("relu_patch.dat", ios::app);
	#endif
		switch(en){
      case 0:
      {
        // bypass this module
        // if (((max_pool || UP_SAMPLE_EN) && out_num_iter == 0) || (!max_pool && (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM))){
        int o = 0;
        int h = 0;
        int w = 0;
        bool done1 = 0;

        int w_bound = (LAYER_IN_W_T / STRIDE)*LAYER_TCONV_STRIDE;
        int h_bound = (LAYER_IN_H_T / STRIDE)*LAYER_TCONV_STRIDE;
        while(!done1){
          #pragma HLS PIPELINE II=1
          ConvData0Type tmp = fifo_cin.read();
          fifo_cout.write(tmp);

            // Repeat until the whole tile is read
          w++;
          if (w == w_bound){
            w = 0;
            h++;
            if (h == h_bound){
              h = 0;
              o++;
              if (o == LAYER_OUT_NUM_T / RELU_LANE){
                o = 0;
                done1 = 1;
              }
            }
          }
        }
      }
      break;
        // compute
      case 1:
      {
        int o = 0;
        int h = 0;
        int w = 0;
        int up = 0;
        bool done2 = 0;
        
        int w_bound = (LAYER_IN_W_T / STRIDE)*LAYER_TCONV_STRIDE;
        int h_bound = (LAYER_IN_H_T / STRIDE)*LAYER_TCONV_STRIDE;
        int up_bound = upsample_factor;
        while(!done2){
          
          if (up==0){
            line_buf[w] = fifo_cin.read();
          }
          for(int i=0; i<up_bound; i++){
            #pragma HLS PIPELINE II=1
            fifo_cout.write(line_buf[w]);
          }
          // fifo_cout.write(line_buf[w]);
          // Repeat until the whole tile is read
          w++;
          if (w == w_bound){
            w = 0;
            up++;
            if (up == up_bound){
              up = 0;
              h++;
              if (h == h_bound){
                h = 0;
                o++;
                if (o == LAYER_OUT_NUM_T / RELU_LANE){
                  o = 0;
                  done2 = 1;
                }
              }
            }
          }
        }
      }
      break;
		}

    // Repeat until all the tiles are read
    // Must repeat the computation until LAYER_OUT_NUM output feature maps are generated
    in_h_iter += LAYER_IN_H_T;
    if (in_h_iter >= LAYER_IN_H){
      in_h_iter = 0;
      in_w_iter += LAYER_IN_W_T;
      if (in_w_iter >= LAYER_IN_W){
        in_w_iter = 0;
        out_num_iter += LAYER_OUT_NUM_T;
        if (out_num_iter >= LAYER_OUT_NUM){
          out_num_iter = 0;
          layer_iter += 1;
          layer_start = 1;
          if (layer_iter == LAYER_BATCH){
            layer_iter = 0;
            done = 1;
          }
        }
      }
    }
  }
    inst++;
    if(inst == inst_count){
      inst_done = 1;
    }
  }
	
}/**
 * Function name: add
 * Function description: This functions add the input of previous layer to result of this layer.
 *                       It helps supporting building blocks such as residual bottleneck layer in MobileNetV2
 */
void concat(
    uint start_inst, uint end_inst,
		tapa::istream<ConvData0Type>        &fifo_cin,
		tapa::istream<ConvData0Type>        &fifo_conv,
		tapa::istream<ConfigInst>           &fifo_config_in,
		tapa::ostream<ConvData0Type>        &fifo_cout_0,
    tapa::ostream<ConvData0Type>        &fifo_cout_1,
		tapa::ostream<ConfigInst>           &fifo_config_out
){
  cout<<"concat"<<endl;
	#pragma HLS INLINE off
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
  	// tiling iterators
	uint in_num_iter = 0;
	uint out_num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

		// Read instructions
	ConfigInst inst0 = fifo_config_in.read();
	fifo_config_out.write(inst0);
	ConfigInst inst1 = fifo_config_in.read();
	fifo_config_out.write(inst1);
	ConfigInst inst2 = fifo_config_in.read();
	fifo_config_out.write(inst2);
	ConfigInst inst3 = fifo_config_in.read();
	fifo_config_out.write(inst3);
	ConfigInst inst4 = fifo_config_in.read();
	fifo_config_out.write(inst4);
  ConfigInst inst5 = fifo_config_in.read();
	fifo_config_out.write(inst5);

	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);

  int count = 0;
	bool layer_start = 0;
	bool done = 0;
	while(!done){
    
		if (layer_start){
			inst0 = fifo_config_in.read();
			fifo_config_out.write(inst0);
			inst1 = fifo_config_in.read();
			fifo_config_out.write(inst1);
			inst2 = fifo_config_in.read();
			fifo_config_out.write(inst2);
			inst3 = fifo_config_in.read();
			fifo_config_out.write(inst3);
			inst4 = fifo_config_in.read();
			fifo_config_out.write(inst4);
      inst5 = fifo_config_in.read();
			fifo_config_out.write(inst5);

			layer_start = 0;
		}

			// Refer to cin_load module to understand the meaning of the instructions
			// inst0
		ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
			// inst1
		ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
			// inst2
		ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
		ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
		ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
		ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
		ap_uint<16> FILTER_S2        = inst2(32*4+31, 32*4+16);
		ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);
			// inst3
		ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
		ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
		ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
		ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
		ap_uint<32> LAYER_IN_H_T     = inst3(32*3+31, 32*3);
		ap_uint<32> LAYER_IN_W_T     = inst3(32*4+31, 32*4);

    ap_uint<16> LAYER_TCONV_STRIDE 	= inst5(32*2+15, 32*2);
		
		ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
		ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
		ap_uint<1>  CONV_EN        = LAYER_EN[2];
		ap_uint<1>  RELU_EN        = LAYER_EN[3];
		ap_uint<1>  RELU6_EN       = LAYER_EN[4];
		ap_uint<1>  POOL_EN        = LAYER_EN[5];
		ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
		ap_uint<1>  BIAS_EN        = LAYER_EN[7];
		ap_uint<1>  LOAD_PREV_CIN  = LAYER_EN[11];
    ap_uint<1>  CONCAT_EN      = LAYER_EN[16];

	#ifdef DEBUG_config_add
		cout << LAYER_IN_NUM_HW << " " << LAYER_OUT_NUM_HW << " " << LAYER_IN_H_HW << " " << LAYER_IN_W_HW << " " << LAYER_OUT_H_HW << " " << LAYER_OUT_W_HW << endl;
		cout << LAYER_IN_NUM << " " << LAYER_OUT_NUM << " " << LAYER_IN_H << " " << LAYER_IN_W << " " << LAYER_OUT_H << " " << LAYER_OUT_W << endl;
		cout << CIN_OFFSET << " " << WEIGHT_OFFSET << " " << BIAS_OFFSET << " " << COUT_OFFSET << " " << FILTER_S1 << " " << FILTER_S2 << " " << STRIDE << endl;
		cout << LAYER_EN << " " << PREV_CIN_OFFSET << " " << LAYER_IN_NUM_T << " " << LAYER_OUT_NUM_T << " " << LAYER_IN_H_T << " " << LAYER_IN_W_T << endl;
	#endif

		data_t0 cin_buf[CONV_LANE];
		data_t0 conv_buf_0[CONV_LANE];
    data_t0 conv_buf_1[CONV_LANE];
		ap_uint<DATA_W0> cout_buf[CONV_LANE];
	#pragma HLS ARRAY_PARTITION variable=cin_buf complete
	#pragma HLS ARRAY_PARTITION variable=conv_buf_0 complete
	#pragma HLS ARRAY_PARTITION variable=conv_buf_1 complete
	#pragma HLS ARRAY_PARTITION variable=cout_buf complete

			// Set up some configuration signals
		uint FILTER_S = 1;
		bool separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
		bool conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
		bool max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);
		uint stride = (max_pool == 1)? 1 : (uint)STRIDE;
    uint upsample_factor = (UP_SAMPLE_EN == 1)? 2 : 1;
		bool en = CONCAT_EN;

	#ifdef DEBUG
		uint relu_cout_cnt = 0;
		ofstream relu_data;
		relu_data.open("relu_patch.dat", ios::app);
	#endif
		switch(en){
		case 0:{
				// bypass this module
      // cout<<(((max_pool || UP_SAMPLE_EN) && out_num_iter == 0) || (!max_pool && (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM)))<<endl;
			// if (((max_pool || UP_SAMPLE_EN) && out_num_iter == 0) || (!max_pool && (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM))){
				int o = 0;
				int h = 0;
				int w = 0;
				bool done1 = 0;
        
					// int w_bound = LAYER_IN_W_T / stride + FILTER_S - 1;
					// int h_bound = LAYER_IN_H_T / stride + FILTER_S - 1;
        int w_bound = (LAYER_IN_W_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE*upsample_factor;
				int h_bound = (LAYER_IN_H_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE*upsample_factor;
        // cout<<"w_bound: "<<w_bound<<" h_bound: "<<h_bound<<endl;
        	
				while(!done1){
	#pragma HLS PIPELINE II=1
					// if(!fifo_conv.empty()){
            fifo_cout_1.write(fifo_conv.read());
            ConvData0Type tmp_cin = 0;
            if (LOAD_PREV_CIN){
              fifo_cout_0.write(fifo_cin.read());
            }
            // Repeat until the whole tile is read
						w++;
						if (w == w_bound){
							w = 0;
							h++;
							if (h == h_bound){
								h = 0;
								o++;
								if (o == LAYER_OUT_NUM_T / CONV_LANE){
									o = 0;
									done1 = 1;
								}
							}
						}
					// }
				}
			}
			break;
			// compute
		case 1:
		{
				int o = 0;
				int h = 0;
				int w = 0;
				bool done2 = 0;

        int w_bound = (LAYER_IN_W_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE;
				int h_bound = (LAYER_IN_H_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE;

				while(!done2){
	#pragma HLS PIPELINE II=1
          // cout<<(!fifo_cin.empty() && !fifo_conv.empty())<<" "<<(!fifo_cin.empty())<<" "<<(!fifo_conv.empty())<<endl;
					// if(!fifo_cin.empty() && !fifo_conv.empty()){
							// Read the result of this layer and the previous cin
            ConvData0Type cin_tmp = 0;
            ConvData0Type conv_tmp_0 = 0;
            ConvData0Type conv_tmp_1 = 0;
            #if SIMD_LANE == 8
              if (o == 1){
                cin_tmp = fifo_cin.read();
                conv_tmp_1 = fifo_conv.read();
                for (int lane = 0; lane < CONV_LANE; lane++){
                  #pragma HLS UNROLL
                  ap_uint<DATA_W0> u32_tmp = cin_tmp(DATA_W0 - 1, 0);
                  cin_buf[lane] = Reinterpret<data_t0>(u32_tmp);
                  cin_tmp = cin_tmp >> DATA_W0;
                }
                for (int lane = 0; lane < CONV_LANE; lane++){
                  #pragma HLS UNROLL
                  ap_uint<DATA_W0> u32_tmp = conv_tmp_1(DATA_W0 - 1, 0);
                  conv_buf_1[lane] = Reinterpret<data_t0>(u32_tmp);
                  conv_tmp_1 = conv_tmp_1 >> DATA_W0;
                }
                cout_buf[0] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[0]);
                cout_buf[1] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[1]);
                cout_buf[2] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[2]);
                cout_buf[3] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[3]);
                cout_buf[4] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[4]);
                cout_buf[5] = Reinterpret<ap_uint<DATA_W0> >(cin_buf[0]);
                cout_buf[6] = Reinterpret<ap_uint<DATA_W0> >(cin_buf[1]);
                cout_buf[7] = Reinterpret<ap_uint<DATA_W0> >(cin_buf[2]);
              }else{
                conv_tmp_0 = fifo_conv.read();
                for (int lane = 0; lane < CONV_LANE; lane++){
                  #pragma HLS UNROLL
                  ap_uint<DATA_W0> u32_tmp = conv_tmp_0(DATA_W0 - 1, 0);
                  conv_buf_0[lane] = Reinterpret<data_t0>(u32_tmp);
                  cout_buf[lane] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_0[lane]);
                  conv_tmp_0 = conv_tmp_0 >> DATA_W0;
                }
              }
						#endif
            #if SIMD_LANE == 16
              cin_tmp = fifo_cin.read();
              conv_tmp_1 = fifo_conv.read();
              for (int lane = 0; lane < CONV_LANE; lane++){
                #pragma HLS UNROLL
                ap_uint<DATA_W0> u32_tmp = cin_tmp(DATA_W0 - 1, 0);
                cin_buf[lane] = Reinterpret<data_t0>(u32_tmp);
                cin_tmp = cin_tmp >> DATA_W0;
              }
              for (int lane = 0; lane < CONV_LANE; lane++){
                #pragma HLS UNROLL
                ap_uint<DATA_W0> u32_tmp = conv_tmp_1(DATA_W0 - 1, 0);
                conv_buf_1[lane] = Reinterpret<data_t0>(u32_tmp);
                conv_tmp_1 = conv_tmp_1 >> DATA_W0;
              }
              cout_buf[0] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[0]);
              cout_buf[1] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[1]);
              cout_buf[2] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[2]);
              cout_buf[3] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[3]);
              cout_buf[4] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[4]);
              cout_buf[5] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[5]);
              cout_buf[6] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[6]);
              cout_buf[7] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[7]);
              cout_buf[8] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[8]);
              cout_buf[9] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[9]);
              cout_buf[10] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[10]);
              cout_buf[11] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[11]);
              cout_buf[12] = Reinterpret<ap_uint<DATA_W0> >(conv_buf_1[12]);
              cout_buf[13] = Reinterpret<ap_uint<DATA_W0> >(cin_buf[0]);
              cout_buf[14] = Reinterpret<ap_uint<DATA_W0> >(cin_buf[1]);
              cout_buf[15] = Reinterpret<ap_uint<DATA_W0> >(cin_buf[2]);
            #endif


							// write out
							// Pack the data according to SIMD_LANE
						ReluData0Type wide_tmp = (
	#if RELU_LANE == 16
								cout_buf[15], cout_buf[14], cout_buf[13], cout_buf[12],
								cout_buf[11], cout_buf[10], cout_buf[9], cout_buf[8],
								cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
								cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif RELU_LANE == 8
																				cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
																				cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif RELU_LANE == 4
																																cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif RELU_LANE == 2              
																																												cout_buf[1], cout_buf[0]
	#elif RELU_LANE == 1
																																																	  cout_buf[0]
	#endif                
						);
						fifo_cout_1.write(wide_tmp);

							// Repeat until the whole tile is read
						w++;
						if (w == w_bound){
							w = 0;
							h++;
							if (h == h_bound){
								h = 0;
								o++;
								if (o == LAYER_OUT_NUM_T / RELU_LANE){
									o = 0;
									done2 = 1;
								}
							}
						}
					// }
				}
			
			break;
		}
		}
    // cout<<count++<<endl;
			// Repeat until all the tiles are read
			// Must repeat the computation until LAYER_OUT_NUM output feature maps are generated
    in_h_iter += LAYER_IN_H_T;
    if (in_h_iter >= LAYER_IN_H){
      in_h_iter = 0;
      in_w_iter += LAYER_IN_W_T;
      if (in_w_iter >= LAYER_IN_W){
        in_w_iter = 0;
        out_num_iter += LAYER_OUT_NUM_T;
        if (out_num_iter >= LAYER_OUT_NUM){
          out_num_iter = 0;
          layer_iter += 1;
          layer_start = 1;
          if (layer_iter == LAYER_BATCH){
            layer_iter = 0;
            done = 1;
          }
        }
      }
    }
	}
    inst++;
    if(inst == inst_count){
      inst_done = 1;
    }
  }
  // int fifo_conv_count = 0;
  // int fifo_cin_count = 0;
  // while(!fifo_conv.empty()){
  //   fifo_conv.read();
  //   fifo_conv_count++;
  // }
  // while(!fifo_cin.empty()){
  //   fifo_cin.read();
  //   fifo_cin_count++;
  // }
  // cout<<"fifo_conv_count: "<<fifo_conv_count<<endl;
  // cout<<"fifo_cin_count: "<<fifo_cin_count<<endl;
}
/**
 * Function name: add
 * Function description: This functions add the input of previous layer to result of this layer.
 *                       It helps supporting building blocks such as residual bottleneck layer in MobileNetV2
 */
void add(
    uint start_inst, uint end_inst,
		tapa::istream<ConvData0Type>        &fifo_cin,
		tapa::istream<ConvData0Type>        &fifo_conv,
		tapa::istream<ConfigInst>           &fifo_config_in,
		tapa::ostream<ConvData0Type>        &fifo_cout,
		tapa::ostream<ConfigInst>           &fifo_config_out
){
  float cin_prev_sum = 0;
  float conv_sum = 0;
  cout<<"add"<<endl;
	#pragma HLS INLINE off
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
		// tiling iterators
	uint in_num_iter = 0;
	uint out_num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

		// Read instructions
	ConfigInst inst0 = fifo_config_in.read();
	fifo_config_out.write(inst0);
	ConfigInst inst1 = fifo_config_in.read();
	fifo_config_out.write(inst1);
	ConfigInst inst2 = fifo_config_in.read();
	fifo_config_out.write(inst2);
	ConfigInst inst3 = fifo_config_in.read();
	fifo_config_out.write(inst3);
	ConfigInst inst4 = fifo_config_in.read();
	fifo_config_out.write(inst4);
  ConfigInst inst5 = fifo_config_in.read();
	fifo_config_out.write(inst5);

	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);
  int count = 0;

	bool layer_start = 0;
	bool done = 0;
	while(!done){
    
		if (layer_start){
			inst0 = fifo_config_in.read();
			fifo_config_out.write(inst0);
			inst1 = fifo_config_in.read();
			fifo_config_out.write(inst1);
			inst2 = fifo_config_in.read();
			fifo_config_out.write(inst2);
			inst3 = fifo_config_in.read();
			fifo_config_out.write(inst3);
			inst4 = fifo_config_in.read();
			fifo_config_out.write(inst4);
      inst5 = fifo_config_in.read();
			fifo_config_out.write(inst5);

			layer_start = 0;
		}

			// Refer to cin_load module to understand the meaning of the instructions
			// inst0
		ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
			// inst1
		ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
			// inst2
		ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
		ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
		ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
		ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
		ap_uint<16> FILTER_S2        = inst2(32*4+31, 32*4+16);
		ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);
			// inst3
		ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
		ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
		ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
		ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
		ap_uint<32> LAYER_IN_H_T     = inst3(32*3+31, 32*3);
		ap_uint<32> LAYER_IN_W_T     = inst3(32*4+31, 32*4);

    ap_uint<16> LAYER_TCONV_STRIDE 	= inst5(32*2+15, 32*2);
		
		ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
		ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
		ap_uint<1>  CONV_EN        = LAYER_EN[2];
		ap_uint<1>  RELU_EN        = LAYER_EN[3];
		ap_uint<1>  RELU6_EN       = LAYER_EN[4];
		ap_uint<1>  POOL_EN        = LAYER_EN[5];
		ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
		ap_uint<1>  BIAS_EN        = LAYER_EN[7];
		ap_uint<1>  LOAD_PREV_CIN  = LAYER_EN[11];
    ap_uint<1>  ADD_EN         = LAYER_EN[17];

	#ifdef DEBUG_config_add
		cout << LAYER_IN_NUM_HW << " " << LAYER_OUT_NUM_HW << " " << LAYER_IN_H_HW << " " << LAYER_IN_W_HW << " " << LAYER_OUT_H_HW << " " << LAYER_OUT_W_HW << endl;
		cout << LAYER_IN_NUM << " " << LAYER_OUT_NUM << " " << LAYER_IN_H << " " << LAYER_IN_W << " " << LAYER_OUT_H << " " << LAYER_OUT_W << endl;
		cout << CIN_OFFSET << " " << WEIGHT_OFFSET << " " << BIAS_OFFSET << " " << COUT_OFFSET << " " << FILTER_S1 << " " << FILTER_S2 << " " << STRIDE << endl;
		cout << LAYER_EN << " " << PREV_CIN_OFFSET << " " << LAYER_IN_NUM_T << " " << LAYER_OUT_NUM_T << " " << LAYER_IN_H_T << " " << LAYER_IN_W_T << endl;
	#endif

		data_t0 cin_buf[CONV_LANE];
		data_t0 conv_buf[CONV_LANE];
		ap_uint<DATA_W0> cout_buf[CONV_LANE];
	#pragma HLS ARRAY_PARTITION variable=cin_buf complete
	#pragma HLS ARRAY_PARTITION variable=conv_buf complete
	#pragma HLS ARRAY_PARTITION variable=cout_buf complete

			// Set up some configuration signals
		uint FILTER_S = 1;
    uint upsample_factor = (UP_SAMPLE_EN==1)? 2 : 1;
		bool separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
		bool conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
		bool max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);
		uint stride = (max_pool == 1)? 1 : (uint)STRIDE;
		bool en = ADD_EN;
    #ifdef DEBUG
      uint relu_cout_cnt = 0;
      ofstream relu_data;
      relu_data.open("relu_patch.dat", ios::app);
    #endif
    switch(en){
      case 0:
      {
        // bypass this module
        // if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM){
        int o = 0;
        int h = 0;
        int w = 0;
        bool done1 = 0;

          // int w_bound = LAYER_IN_W_T / stride + FILTER_S - 1;
          // int h_bound = LAYER_IN_H_T / stride + FILTER_S - 1;
        int w_bound = (LAYER_IN_W_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE*upsample_factor;
        int h_bound = (LAYER_IN_H_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE*upsample_factor;
          // cout<<w_bound<<" "<<h_bound<<" "<<LAYER_OUT_NUM_T<<" "<<LAYER_TCONV_STRIDE<<" "<<stride<<endl;
        while(!done1){
          #pragma HLS PIPELINE II=1
          // if(!fifo_conv.empty()/* && !fifo_cin.empty()*/){
            ConvData0Type tmp = 0;
            fifo_cout.write(fifo_conv.read());
            
            // Repeat until the whole tile is read
            w++;
            if (w == w_bound){
              w = 0;
              h++;
              if (h == h_bound){
                h = 0;
                o++;
                if (o == LAYER_OUT_NUM_T / CONV_LANE){
                  o = 0;
                  done1 = 1;
                }
              }
            }
          // }
        }
      }
      break;
      // compute
      case 1:
      {
        // if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM){
        int o = 0;
        int h = 0;
        int w = 0;
        bool done2 = 0;

        int w_bound = (LAYER_IN_W_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE;
        int h_bound = (LAYER_IN_H_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE;

        while(!done2){
          #pragma HLS PIPELINE II=1
            // cout<<(!fifo_cin.empty() && !fifo_conv.empty())<<" "<<(!fifo_cin.empty())<<" "<<(!fifo_conv.empty())<<endl;
          // if(!fifo_cin.empty() && !fifo_conv.empty()){
            // Read the result of this layer and the previous cin
            ConvData0Type cin_tmp = 0;
            cin_tmp = fifo_cin.read();
            // data_t2 num[SIMD_LANE];
            // for(int i=0; i<SIMD_LANE; i++){
            //   num[i] = Reinterpret<data_t2>((ap_uint<32>)cin_tmp((i+1)*32-1, 32*i));
            //   cin_prev_sum += num[i];
            // }
            ConvData0Type conv_tmp = 0;
            conv_tmp = fifo_conv.read();
            // for(int i=0; i<SIMD_LANE; i++){
            //   num[i] = Reinterpret<data_t2>((ap_uint<32>)conv_tmp((i+1)*32-1, 32*i));
            //   conv_sum += num[i];
            // }
              // Unpack the data according to SIMD_LANE
            for (int lane = 0; lane < CONV_LANE; lane++){
              #pragma HLS UNROLL
              ap_uint<DATA_W0> u32_tmp = cin_tmp(DATA_W0 - 1, 0);
              cin_buf[lane] = Reinterpret<data_t0>(u32_tmp);
              cin_tmp = cin_tmp >> DATA_W0;
            }

            for (int lane = 0; lane < CONV_LANE; lane++){
              #pragma HLS UNROLL
              ap_uint<DATA_W0> u32_tmp = conv_tmp(DATA_W0 - 1, 0);
              conv_buf[lane] = Reinterpret<data_t0>(u32_tmp);
              conv_tmp = conv_tmp >> DATA_W0;
            }

              // Add
            for (int lane = 0; lane < CONV_LANE; lane++){
              #pragma HLS UNROLL    
              data_t0 cin_data = cin_buf[lane];
              data_t0 conv_data = conv_buf[lane];
              data_t0 tmp = cin_data + conv_data;
              cout_buf[lane] = Reinterpret<ap_uint<DATA_W0> >(tmp);
                // 	#define DEBUG_add
              #ifdef DEBUG_add
                        if(lane==0 && o==0)
                          cout << "cin: " << cin_data << " conv: " << conv_data << " tmp: " << tmp << endl;
              #endif
            }

              // write out
              // Pack the data according to SIMD_LANE
            ReluData0Type wide_tmp = (
            #if RELU_LANE == 16
                          cout_buf[15], cout_buf[14], cout_buf[13], cout_buf[12],
                          cout_buf[11], cout_buf[10], cout_buf[9], cout_buf[8],
                          cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
                          cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
            #elif RELU_LANE == 8
                                                  cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
                                                  cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
            #elif RELU_LANE == 4
                                                                          cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
            #elif RELU_LANE == 2              
                                                                                                  cout_buf[1], cout_buf[0]
            #elif RELU_LANE == 1
                                                                                                              cout_buf[0]
            #endif                
            );
            fifo_cout.write(wide_tmp);

              // Repeat until the whole tile is read
            w++;
            if (w == w_bound){
              w = 0;
              h++;
              if (h == h_bound){
                h = 0;
                o++;
                if (o == LAYER_OUT_NUM_T / RELU_LANE){
                  o = 0;
                  done2 = 1;
                }
              }
            }
          // }
        }
      }
      break;
		}
    	// cout<<count++<<endl;
			// Repeat until all the tiles are read
			// Must repeat the computation until LAYER_OUT_NUM output feature maps are generated
		// in_num_iter += LAYER_IN_NUM_T;
		// if (in_num_iter >= LAYER_IN_NUM){
		// 	in_num_iter = 0;
			in_h_iter += LAYER_IN_H_T;
			if (in_h_iter >= LAYER_IN_H){
				in_h_iter = 0;
				in_w_iter += LAYER_IN_W_T;
				if (in_w_iter >= LAYER_IN_W){
					in_w_iter = 0;
					out_num_iter += LAYER_OUT_NUM_T;
					if (out_num_iter >= LAYER_OUT_NUM){
						out_num_iter = 0;
						layer_iter += 1;
						layer_start = 1;
						if (layer_iter == LAYER_BATCH){
							layer_iter = 0;
							done = 1;
						}
					}
				}
			}
		// }

	}
    cout<<"prev_cin_sum: "<<cin_prev_sum<<endl;
    cout<<"conv_sum: "<<conv_sum<<endl;
    cin_prev_sum = 0;
    conv_sum = 0;
    inst++;
    if(inst == inst_count){
      inst_done = 1;
    }
  }

}void act_and_bn(
    uint start_inst, uint end_inst,
		tapa::istream<ConvData0Type>        &fifo_gamma_conv,
		tapa::istream<ConvData0Type>        &fifo_beta_conv,
		tapa::istream<ConvData0Type>        &fifo_cin,
		tapa::istream<ConfigInst>           &fifo_config_in,
		tapa::ostream<ReluData0Type>        &fifo_cout,
		tapa::ostream<ConfigInst>           &fifo_config_out
){
  cout<<"relu"<<endl;
	#pragma HLS INLINE off
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
		// tiling iterators
	uint in_num_iter = 0;
	uint out_num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

		// Read instructions
	ConfigInst inst0 = fifo_config_in.read();
	fifo_config_out.write(inst0);
	ConfigInst inst1 = fifo_config_in.read();
	fifo_config_out.write(inst1);
	ConfigInst inst2 = fifo_config_in.read();
	fifo_config_out.write(inst2);
	ConfigInst inst3 = fifo_config_in.read();
	fifo_config_out.write(inst3);
	ConfigInst inst4 = fifo_config_in.read();
	fifo_config_out.write(inst4);
  ConfigInst inst5 = fifo_config_in.read();
	fifo_config_out.write(inst5);

  	// fifo_config_out.write(inst0);
		// fifo_config_out.write(inst1);
		// fifo_config_out.write(inst2);
		// fifo_config_out.write(inst3);
		// fifo_config_out.write(inst4);
  	// fifo_config_out.write(inst5);

	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);
	bool layer_start = 0;
	bool done = 0;
	while(!done){

		if (layer_start){
			inst0 = fifo_config_in.read();
			fifo_config_out.write(inst0);
			inst1 = fifo_config_in.read();
			fifo_config_out.write(inst1);
			inst2 = fifo_config_in.read();
			fifo_config_out.write(inst2);
			inst3 = fifo_config_in.read();
			fifo_config_out.write(inst3);
			inst4 = fifo_config_in.read();
			fifo_config_out.write(inst4);
      inst5 = fifo_config_in.read();
			fifo_config_out.write(inst5);

			layer_start = 0;
		}

			// Refer to cin_load module to understand the meaning of the instructions
			// inst0
		ap_uint<32> LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
			// inst1
		ap_uint<32> LAYER_IN_NUM     = inst1(32*0+31, 32*0);
		ap_uint<32> LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
		ap_uint<32> LAYER_IN_H       = inst1(32*2+31, 32*2);
		ap_uint<32> LAYER_IN_W       = inst1(32*3+31, 32*3);
		ap_uint<32> LAYER_OUT_H      = inst1(32*4+31, 32*4);
		ap_uint<32> LAYER_OUT_W      = inst1(32*5+31, 32*5);
			// inst2
		ap_uint<32> CIN_OFFSET       = inst2(32*0+31, 32*0);
		ap_uint<32> WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		ap_uint<32> BIAS_OFFSET      = inst2(32*2+31, 32*2);
		ap_uint<32> COUT_OFFSET      = inst2(32*3+31, 32*3);
		ap_uint<16> FILTER_S1        = inst2(32*4+15, 32*4);
		ap_uint<16> FILTER_S2        = inst2(32*4+31, 32*4+16);
		ap_uint<32> STRIDE           = inst2(32*5+31, 32*5);
			// inst3
		ap_uint<32> LAYER_EN         = inst3(32*0+31, 32*0);
		ap_uint<32> PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
		ap_uint<16> LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
		ap_uint<16> LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
		ap_uint<32> LAYER_IN_H_T     = inst3(32*3+31, 32*3);
		ap_uint<32> LAYER_IN_W_T     = inst3(32*4+31, 32*4);

    ap_uint<16> LAYER_CONV_TYPE 		= inst5(32*0+15, 32*0);
    ap_uint<16> LAYER_TCONV_STRIDE 	= inst5(32*2+15, 32*2);

		ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
		ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
		ap_uint<1>  CONV_EN        = LAYER_EN[2];
		ap_uint<1>  RELU_EN        = LAYER_EN[3];
		ap_uint<1>  RELU6_EN       = LAYER_EN[4];
		ap_uint<1>  POOL_EN        = LAYER_EN[5];
		ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
		ap_uint<1>  BIAS_EN        = LAYER_EN[7];
		ap_uint<1>  BATCH_NORM_EN  = LAYER_EN[10];
		ap_uint<1>  LOAD_PREV_CIN  = LAYER_EN[11];
    ap_uint<1>  BATCH_NORM_EN_DEPTH  = LAYER_EN[12];

		data_t2 beta_buf[MAX_OUT_NUM_T / RELU_LANE][RELU_LANE];
		data_t2 gamma_buf[MAX_OUT_NUM_T / RELU_LANE][RELU_LANE]; 
	#pragma HLS ARRAY_PARTITION variable=beta_buf dim=2 complete 
	#pragma HLS ARRAY_PARTITION variable=gamma_buf dim=2 complete 
		data_t0 cin_buf[RELU_LANE];
		ap_uint<DATA_W0> cout_buf[RELU_LANE];
	#pragma HLS ARRAY_PARTITION variable=cin_buf complete
	#pragma HLS ARRAY_PARTITION variable=cout_buf complete

			// Set up some configuration signals
		uint FILTER_S = 1;
		bool separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
		bool conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
		bool max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);
		uint stride = (max_pool == 1)? 1 : (uint)STRIDE;
		bool en = RELU_EN || BIAS_EN || RELU6_EN || BATCH_NORM_EN;
    bool norm_conv_en = (CONV_EN == 1 && BATCH_NORM_EN == 1);
    bool bias_en = (CONV_EN == 1 && BIAS_EN == 1);
    uint upsample_factor = (UP_SAMPLE_EN == 1)? 2 : 1;

    #ifdef DEBUG
      uint relu_cout_cnt = 0;
      ofstream relu_data;
      relu_data.open("relu_patch.dat", ios::app);
    #endif
		switch(en){
		case 0:{
				// bypass this module
			// if (((max_pool || UP_SAMPLE_EN) && out_num_iter == 0) || (!max_pool && (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM))){
				int o = 0;
				int h = 0;
				int w = 0;
				bool done1 = 0;

				int w_bound = (LAYER_IN_W_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE*upsample_factor;
				int h_bound = (LAYER_IN_H_T / stride + FILTER_S - 1)*LAYER_TCONV_STRIDE*upsample_factor;
				while(!done1){
	#pragma HLS PIPELINE II=1
					ConvData0Type tmp = fifo_cin.read();
					fifo_cout.write(tmp);

						// If after conv module neither exists bias nor batch normalization layer, there is no data to read from these FIFOs
					if (norm_conv_en == 1){
					  ConvData0Type beta_conv = fifo_beta_conv.read();
					  ConvData0Type gamma_conv = fifo_gamma_conv.read();
          }
						// Repeat until the whole tile is read
					w++;
					if (w == w_bound){
						w = 0;
						h++;
						if (h == h_bound){
							h = 0;
							o++;
							if (o == LAYER_OUT_NUM_T / RELU_LANE){
								o = 0;
								done1 = 1;
							}
						}
					}
				}
			}
			break;
			// compute
		case 1:
		{
			// if (in_num_iter + LAYER_IN_NUM_T >= LAYER_IN_NUM){
        // cout<<"RELU_EN: "<<RELU_EN<<endl;
					// Read beta and gamma for the batch normalization
					// If there doesn't exist a batch normalization and it's a normal bias,
					// beta = bias, gamma = 0
				for (int o = 0; o < LAYER_OUT_NUM_T / RELU_LANE; o++){
	#pragma HLS PIPELINE II=1
					ConvData0Type beta = 0;//fifo_beta_conv.read();
					ConvData0Type gamma = 0;//fifo_gamma_conv.read();
          if (bias_en){
            beta = fifo_beta_conv.read();
            gamma = fifo_gamma_conv.read();
          }
					for (int lane = 0; lane < RELU_LANE; lane++){
	#pragma HLS UNROLL
						ap_uint<DATA_W0> u32_beta = beta(DATA_W0 - 1, 0);
						beta_buf[o][lane] = Reinterpret<data_t2>(u32_beta);
						beta = beta >> DATA_W0;
					}

					for (int lane = 0; lane < RELU_LANE; lane++){
	#pragma HLS UNROLL
						ap_uint<DATA_W0> u32_gamma = gamma(DATA_W0 - 1, 0);
						gamma_buf[o][lane] = Reinterpret<data_t2>(u32_gamma);
						gamma = gamma >> DATA_W0;
					}
				}

				int o = 0;
				int h = 0;
				int w = 0;
				bool done2 = 0;

				int w_bound = (LAYER_IN_W_T / STRIDE)*LAYER_TCONV_STRIDE*upsample_factor;
				int h_bound = (LAYER_IN_H_T / STRIDE)*LAYER_TCONV_STRIDE*upsample_factor;

				while(!done2){
	#pragma HLS PIPELINE II=1
					ConvData0Type cin_tmp = fifo_cin.read();
						// Unpack data according to SIMD_LANE
					for (int lane = 0; lane < RELU_LANE; lane++){
	#pragma HLS UNROLL
						ap_uint<DATA_W0> u32_tmp = cin_tmp(DATA_W0 - 1, 0);
						cin_buf[lane] = Reinterpret<data_t0>(u32_tmp);
						cin_tmp = cin_tmp >> DATA_W0;
					}
						// Apply beta and gamma + ReLU(6)
					for (int lane = 0; lane < RELU_LANE; lane++){
	#pragma HLS UNROLL    
						data_t0 cin_data = cin_buf[lane];
						data_t0 tmp = cin_data;
            if (BATCH_NORM_EN){
              tmp = tmp * (data_t0) 0.9995003746640602;
            }
            if (RELU_EN)
              tmp = max(tmp, (data_t0) 0);
						// if (bias_en || BATCH_NORM_EN)
						// 	tmp = cin_data + beta_buf[o][lane];
						// 	//else if(BATCH_NORM_EN)
						// 	//	tmp = gamma_buf[o][lane]*tmp + beta_buf[o][lane];
						// if (RELU6_EN && !BATCH_NORM_EN_DEPTH)
						// 	tmp = min(max(0, tmp), 6);
						// else if (RELU_EN && LAYER_CONV_TYPE!=1)
						// 	tmp = max(tmp*0.001, tmp);
						cout_buf[lane] = Reinterpret<ap_uint<DATA_W0> >(tmp);
	#ifdef DEBUG_relu
						if(out_num_iter == 0 && in_h_iter == 0){
							cout << cin_data << " " << tmp << endl;
						}
	#endif
	#ifdef DEBUG_conv_relu
						if(DEPTH_CONV_EN && lane == 0)
							cout << "in: " << cin_buf[lane] << " gamma: " << gamma_buf[o][lane] << " beta: " << beta_buf[o][lane] << " norm: " << gamma_buf[o][lane]*cin_buf[lane] + beta_buf[o][lane] << " tmp: " << tmp << endl;

	#endif
					}
						// write out
						// Pack according to SIMD_LANE
					ReluData0Type wide_tmp = (
	#if RELU_LANE == 16
							cout_buf[15], cout_buf[14], cout_buf[13], cout_buf[12],
							cout_buf[11], cout_buf[10], cout_buf[9], cout_buf[8],
							cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
							cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif RELU_LANE == 8
																			cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
																			cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif RELU_LANE == 4
																															cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif RELU_LANE == 2              
																																											cout_buf[1], cout_buf[0]
	#elif RELU_LANE == 1
																																																  cout_buf[0]
	#endif                
					);
					fifo_cout.write(wide_tmp);

						// Repeat until the whole tile is read
					w++;
					if (w == w_bound){
						w = 0;
						h++;
						if (h == h_bound){
							h = 0;
							o++;
							if (o == LAYER_OUT_NUM_T / RELU_LANE){
								o = 0;
								done2 = 1;
							}
						}
					}
				}
			}
			break;
		// }
		}
			// Repeat until all the tiles are read
			// Must repeat the computation until LAYER_OUT_NUM output feature maps are generated
		// in_num_iter += LAYER_IN_NUM_T;
		// if (in_num_iter >= LAYER_IN_NUM){
		// 	in_num_iter = 0;
			in_h_iter += LAYER_IN_H_T;
			if (in_h_iter >= LAYER_IN_H){
				in_h_iter = 0;
				in_w_iter += LAYER_IN_W_T;
				if (in_w_iter >= LAYER_IN_W){
					in_w_iter = 0;
					out_num_iter += LAYER_OUT_NUM_T;
					if (out_num_iter >= LAYER_OUT_NUM){
						out_num_iter = 0;
						layer_iter += 1;
						layer_start = 1;
						if (layer_iter == LAYER_BATCH){
							layer_iter = 0;
							done = 1;
						}
					}
				}
			}
		// }
	}
    inst++;
    if(inst == inst_count){
      inst_done = 1;
    }
  }
}
/**
 * Function name: cout_write_fifo_read
 * Function description: This function reads cout data.
 */
void cout_write_fifo_read(
		bus_t0 *cout_burst_buf,
		tapa::istream<PoolData0Type>  &fifo_cout,
		bool en,
		bool up_sample,
		uint LAYER_IN_NUM,
		uint LAYER_OUT_H,
		uint LAYER_OUT_W,
		uint LAYER_IN_NUM_T,
		uint LAYER_OUT_NUM_T,
		uint LAYER_IN_H_T,
		uint LAYER_IN_W_T,
		uint in_h_iter,
		uint in_w_iter,
    ap_uint<16> TCONV_STRIDE
){
  // cout<<"cout_write_fifo_read"<<endl;
	PoolData0Type cout_buf[DATA_SEL_FACTOR0];
	#pragma HLS ARRAY_PARTITION variable=cout_buf complete

	#ifdef DEBUG
	ofstream cout_debug;
	cout_debug.open("hw_cout_write_patch.dat", ios::app);
	#endif        

	uint write = 0;
		// Set up the writing mode
	if (en == 0 && up_sample == 0) write = 0; 	// normal writing
	else if (en == 1 && up_sample == 0) write = 1; 	// writing after pooling
	else if (up_sample == 1) write = 2; 	// writing after upsampling
		// Should store the data as Th * Tw * Tn
		// write = 2;
  int count = 0;
	switch(write){
	case 0:
	{
		int o = 0;
		int h = 0;
		int w = 0;
		bool done = 0;
		while(!done){
	#pragma HLS PIPELINE II=1       
	#pragma HLS dependence variable=cout_burst_buf type=inter false
			uint local_cout_idx = h * LAYER_IN_W_T * LAYER_OUT_NUM_T + w * LAYER_OUT_NUM_T + o * POOL_LANE;
			bus_t0 wide_tmp = cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0];
			for (int lane = 0; lane < DATA_SEL_FACTOR0; lane++){
	#pragma HLS UNROLL
				cout_buf[lane] = wide_tmp(DATA_W0 * POOL_LANE - 1, 0);
				wide_tmp = wide_tmp >> DATA_W0 * POOL_LANE;
			}
			PoolData0Type tmp = fifo_cout.read();
			if (in_h_iter + h < LAYER_OUT_H && in_w_iter + w < LAYER_OUT_W)
				cout_buf[(local_cout_idx % BUS_PACK_FACTOR0) / POOL_LANE] = tmp;
			else
				cout_buf[(local_cout_idx % BUS_PACK_FACTOR0) / POOL_LANE] = tmp;

			bus_t0 wide_pack = (
	#if DATA_SEL_FACTOR0 == 1
					cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 2
					cout_buf[1], cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 4
					cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 8
					cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
					cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 16
				    cout_buf[15], cout_buf[14], cout_buf[13], cout_buf[12],
				    cout_buf[11], cout_buf[10], cout_buf[9], cout_buf[8],
				    cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
				    cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#endif                  
			);
			cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0] = wide_pack;
      	//  print<512>(cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0]);

				// Repeat until the whole tile is read
			w++;
			if (w == LAYER_IN_W_T){
				w = 0;
				h++;
				if (h == LAYER_IN_H_T){
					h = 0;
					o++;
					if (o == LAYER_OUT_NUM_T / POOL_LANE){
						o = 0;
						done = 1;
					}
				}
			}
		}
			//        }
		break;
	}
	case 1:
	{
		int o = 0;
		int h = 0;
		int w = 0;
		bool done = 0;
		while(!done){
	#pragma HLS PIPELINE II=1
				#pragma HLS dependence variable=cout_burst_buf type=inter false
			uint local_cout_idx = h * (LAYER_IN_W_T / 2) * LAYER_OUT_NUM_T + w * LAYER_OUT_NUM_T + o * POOL_LANE;
			bus_t0 wide_tmp = cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0];
			for (int lane = 0; lane < DATA_SEL_FACTOR0; lane++){
	#pragma HLS UNROLL
				cout_buf[lane] = wide_tmp(DATA_W0 * POOL_LANE - 1, 0);
				wide_tmp = wide_tmp >> DATA_W0 * POOL_LANE;
        	// cout<<"lane1: "<<lane<<endl;
			}
			PoolData0Type tmp = fifo_cout.read();
      	// cout<<"lane2: "<<(local_cout_idx % BUS_PACK_FACTOR0) / POOL_LANE<<endl;
      	// cout<<h<<" "<<w<<" "<<o<<" "<<in_h_iter<<" "<<in_w_iter<<" "<<LAYER_OUT_H<<" "<<LAYER_OUT_W<<endl;
			if (in_h_iter / 2 + h < LAYER_OUT_H && in_w_iter / 2 + w < LAYER_OUT_W){
				cout_buf[(local_cout_idx % BUS_PACK_FACTOR0) / POOL_LANE] = tmp;
        	// print<256>(tmp);
      }
			else{
				cout_buf[(local_cout_idx % BUS_PACK_FACTOR0) / POOL_LANE] = 0;
      }
      	// print<256>(cout_buf[0]);
      	// print<256>(cout_buf[1]);
      	// cout<<(local_cout_idx % BUS_PACK_FACTOR0) / POOL_LANE<<endl;
      	// print<256>(cout_buf[(local_cout_idx % BUS_PACK_FACTOR0) / POOL_LANE] );
			bus_t0 wide_pack = (
	#if DATA_SEL_FACTOR0 == 1
					cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 2
					cout_buf[1], cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 4
					cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 8
					cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
					cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 16
					cout_buf[15], cout_buf[14], cout_buf[13], cout_buf[12],
					cout_buf[11], cout_buf[10], cout_buf[9], cout_buf[8],
					cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
					cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#endif                  
			);
			cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0] = wide_pack;
      	// cout<<cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0]<<endl;
      	// print<512>(cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0]);

				// Repeat until the whole tile is read
			w++;
			if (w == LAYER_IN_W_T / 2){
				w = 0;
				h++;
				if (h == LAYER_IN_H_T / 2){
					h = 0;
					o++;
					if (o == LAYER_OUT_NUM_T / POOL_LANE){
						o = 0;
						done = 1;
					}
				}
			}
		}
    	// exit(0);
	}
	break;
	case 2:
	{
		int o = 0;
		int h = 0;
		int w = 0;
		bool done = 0;
		while(!done){
	#pragma HLS PIPELINE II=1
				#pragma HLS dependence variable=cout_burst_buf type=inter false
			uint local_cout_idx = h * (LAYER_IN_W_T * TCONV_STRIDE) * LAYER_OUT_NUM_T + w * LAYER_OUT_NUM_T + o * POOL_LANE;
			bus_t0 wide_tmp = cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0];
			for (int lane = 0; lane < DATA_SEL_FACTOR0; lane++){
	#pragma HLS UNROLL
				cout_buf[lane] = wide_tmp(DATA_W0 * POOL_LANE - 1, 0);
				wide_tmp = wide_tmp >> DATA_W0 * POOL_LANE;
			}
			PoolData0Type tmp = fifo_cout.read();

			if (in_h_iter * TCONV_STRIDE + h < LAYER_OUT_H && in_w_iter * TCONV_STRIDE + w < LAYER_OUT_W)
				cout_buf[(local_cout_idx % BUS_PACK_FACTOR0) / POOL_LANE] = tmp;
			else
				cout_buf[(local_cout_idx % BUS_PACK_FACTOR0) / POOL_LANE] = 0;
			bus_t0 wide_pack = (
	#if DATA_SEL_FACTOR0 == 1
					cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 2
					cout_buf[1], cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 4
					cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 8
					cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
					cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif DATA_SEL_FACTOR0 == 16
					cout_buf[15], cout_buf[14], cout_buf[13], cout_buf[12],
					cout_buf[11], cout_buf[10], cout_buf[9], cout_buf[8],
					cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4],
					cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#endif                  
			);
			cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0] = wide_pack;
	#ifdef DEBUG_up_fifo
			cout << "after merge: " << local_cout_idx / BUS_PACK_FACTOR0 << " " ;
			for (int lane = 0; lane < CONV_LANE*2; lane++){
				ap_uint<DATA_W0> u32_tmp = wide_pack(DATA_W0 - 1, 0);
				cout << "lane: " << lane<< " " << Reinterpret<data_t0>(u32_tmp) << " ";
				wide_pack = wide_pack >> DATA_W0;
			}
			cout << endl;
	#endif
				// Repeat until the whole tile is read
			w++;
			if (w == LAYER_IN_W_T * TCONV_STRIDE){
				w = 0;
				h++;
				if (h == LAYER_IN_H_T * TCONV_STRIDE){
					h = 0;
					o++;
					if (o == LAYER_OUT_NUM_T / POOL_LANE){
						o = 0;
						done = 1;
					}
				}
			}
		}
	}
	break;
	}
  	// exit(0);
}

/**
 * Function name: cout_write_ddr_write
 * Function description: This function writes out cout results to off-chip DRAM.
 */
void cout_write_ddr_write(
		bus_t0 *cout_burst_buf,
		bus_mem_0 global_cout,
		bool en,
		bool up_sample,
		uint num_iter,
		uint in_h_iter,
		uint in_w_iter,
		uint LAYER_IN_NUM,
		uint LAYER_OUT_NUM,
		uint LAYER_IN_NUM_T,
		uint LAYER_OUT_NUM_T,
		uint LAYER_IN_H_T,
		uint LAYER_IN_W_T,
		uint LAYER_OUT_H_HW,
		uint LAYER_OUT_W_HW,
		uint num_tile,
		uint ind_w_t,
		uint ind_w,
		uint cout_offset,
		bool change_layout,
		bool run,
    ap_uint<16> TCONV_STRIDE
){
  // cout<<"cout_write_ddr_write"<<endl;
		// Set up the writing mode
	uint write = 0;
	if (up_sample == 1) write = 2;// writing after upsampling
	else if (en == 0) write = 0; 	// normal writing
	else if (en == 1) write = 1; 	// writing after pooling
		// The default data layout is ceil(N / Tn) * H * ceil(W / Tw) * Tw * Tn
		// If filter size is 1, the data layout should change to ceil(N / Tn) * ceil(H / Th) * ceil(W / Tw) * Th * Tw * Tn
  if (change_layout) write += 3; 
  // cout<<"write: "<<write<<endl;
  	// write = 2;
	if (run){
		switch(write){
		case 0:
		{
				// write out
			for (int hh  = 0; hh < LAYER_IN_H_T; hh++){
				uint h = in_h_iter + hh;
				uint global_cout_idx = num_iter / LAYER_OUT_NUM_T * LAYER_OUT_H_HW * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + h * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + in_w_iter * LAYER_OUT_NUM_T + cout_offset;
				uint local_cout_idx = hh * LAYER_IN_W_T * LAYER_OUT_NUM_T;
				for(int i=0; i<LAYER_IN_W_T * LAYER_OUT_NUM_T/BUS_PACK_FACTOR0; i++){
          #pragma HLS PIPELINE II=1
          global_cout[global_cout_idx/BUS_PACK_FACTOR0 + i] = cout_burst_buf[local_cout_idx/BUS_PACK_FACTOR0 + i];
        }
				// memcpy((void*)&global_cout[global_cout_idx / BUS_PACK_FACTOR0], (void*)&cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0], sizeof(data_t0) * LAYER_IN_W_T * LAYER_OUT_NUM_T);
      }
		}
		break;
		case 1:
		{
			for (int hh = 0; hh < LAYER_IN_H_T / 2; hh++){
				uint h = in_h_iter / 2 + hh;
				uint global_cout_idx = num_iter / LAYER_OUT_NUM_T * LAYER_OUT_H_HW * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + h * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + in_w_iter / 2 * LAYER_OUT_NUM_T + cout_offset;
				uint local_cout_idx = hh * LAYER_IN_W_T / 2 * LAYER_OUT_NUM_T;
				for(int i=0; i<(LAYER_IN_W_T / 2 * LAYER_OUT_NUM_T)/BUS_PACK_FACTOR0; i++){
          #pragma HLS PIPELINE II=1
          global_cout[global_cout_idx/BUS_PACK_FACTOR0 + i] = cout_burst_buf[local_cout_idx/BUS_PACK_FACTOR0 + i];
        }
        // memcpy((void*)&global_cout[global_cout_idx / BUS_PACK_FACTOR0], (void*)&cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0], sizeof(data_t0) * LAYER_IN_W_T / 2 * LAYER_OUT_NUM_T);
			}
		}
		break;
		case 2:
		{
			for (int hh = 0; hh < LAYER_IN_H_T * TCONV_STRIDE; hh++){
				uint h = in_h_iter * TCONV_STRIDE + hh;
				uint global_cout_idx;
				global_cout_idx = num_iter / LAYER_OUT_NUM_T * LAYER_OUT_H_HW * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + h * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + in_w_iter * TCONV_STRIDE * LAYER_OUT_NUM_T + cout_offset;
				uint local_cout_idx = hh * LAYER_IN_W_T * TCONV_STRIDE * LAYER_OUT_NUM_T;
        for(int i=0; i<(LAYER_IN_W_T * TCONV_STRIDE * LAYER_OUT_NUM_T)/BUS_PACK_FACTOR0; i++){
          #pragma HLS PIPELINE II=1
          global_cout[global_cout_idx/BUS_PACK_FACTOR0 + i] = cout_burst_buf[local_cout_idx/BUS_PACK_FACTOR0 + i];
        }
        // memcpy((void*)&global_cout[global_cout_idx / BUS_PACK_FACTOR0], (void*)&cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0], sizeof(data_t0) * LAYER_IN_W_T * TCONV_STRIDE * LAYER_OUT_NUM_T);
			}
		}
		break;
    case 3:
		{
			// write out
			for (int hh  = 0; hh < 1; hh++){
				uint global_cout_idx = cout_offset + num_tile * LAYER_OUT_NUM_T * LAYER_IN_W_T * LAYER_IN_H_T;
				uint local_cout_idx = hh * LAYER_IN_W_T * LAYER_OUT_NUM_T;
        for(int i=0; i<(LAYER_IN_W_T * LAYER_OUT_NUM_T * LAYER_IN_H_T)/BUS_PACK_FACTOR0; i++){
          #pragma HLS PIPELINE II=1
          global_cout[global_cout_idx/BUS_PACK_FACTOR0 + i] = cout_burst_buf[local_cout_idx/BUS_PACK_FACTOR0 + i];
        }
        // memcpy((void*)&global_cout[global_cout_idx / BUS_PACK_FACTOR0], (void*)&cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0], sizeof(data_t0) * LAYER_IN_W_T * LAYER_OUT_NUM_T * LAYER_IN_H_T);
			}
		}
		break;
		case 4:
		{
			for (int hh = 0; hh < 1; hh++){
				uint global_cout_idx = cout_offset + num_tile * LAYER_OUT_NUM_T * LAYER_IN_W_T/2 * LAYER_IN_H_T/2;
				uint local_cout_idx = hh * LAYER_IN_W_T/2 * LAYER_OUT_NUM_T;
				// cout<<"global:"<<global_cout_idx<<" h: "<<hh<<" in_h_iter: "<<in_h_iter<<" local: "<<local_cout_idx<<endl;
        for(int i=0; i<(LAYER_IN_W_T/2 * LAYER_OUT_NUM_T * LAYER_IN_H_T/2)/BUS_PACK_FACTOR0; i++){
          #pragma HLS PIPELINE II=1
          global_cout[global_cout_idx/BUS_PACK_FACTOR0 + i] = cout_burst_buf[local_cout_idx/BUS_PACK_FACTOR0 + i];
        }
        // memcpy((void*)&global_cout[global_cout_idx / BUS_PACK_FACTOR0], (void*)&cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0], sizeof(data_t0) * LAYER_IN_W_T/2 * LAYER_OUT_NUM_T * LAYER_IN_H_T/2);
			}
		}
		break;
		case 5:
		{
			for (int hh = 0; hh < 1; hh++){
				uint global_cout_idx = cout_offset + num_tile * LAYER_OUT_NUM_T * LAYER_IN_W_T * LAYER_IN_H_T * 4;
				uint local_cout_idx = hh * LAYER_IN_W_T * 2 * LAYER_OUT_NUM_T;
        for(int i=0; i<(LAYER_IN_W_T * 2 * LAYER_OUT_NUM_T * LAYER_IN_H_T * 2)/BUS_PACK_FACTOR0; i++){
          #pragma HLS PIPELINE II=1
          global_cout[global_cout_idx/BUS_PACK_FACTOR0 + i] = cout_burst_buf[local_cout_idx/BUS_PACK_FACTOR0 + i];
        }
        // memcpy((void*)&global_cout[global_cout_idx / BUS_PACK_FACTOR0], (void*)&cout_burst_buf[local_cout_idx / BUS_PACK_FACTOR0], sizeof(data_t0) * LAYER_IN_W_T * 2 * LAYER_OUT_NUM_T * LAYER_IN_H_T * 2);
			}
		}
		break;
		}
	}
  	// exit(0);
}

/**
 * Function name: cout_write
 * Function description: This function collects and writes out cout results.
 */
void cout_write(
    uint start_inst, uint end_inst,
		tapa::istream<PoolData0Type>  &fifo_cout,
		tapa::istream<ConfigInst>     &fifo_config_in,
		bus_mem_0                     global_cout,
    tapa::istream<int>            &in_sync,
    tapa::ostream<int>            &out_sync
){
  cout<<"cout_write"<<endl;
  bool inst_done = 0;
  int inst = 0;
  int inst_count = end_inst - start_inst;
  while(!inst_done){
  int sync = in_sync.read();    
  if (sync == inst) {
	bus_t0 cout_burst_buf_ping[COUT_BUFF / BUS_PACK_FACTOR0];
	bus_t0 cout_burst_buf_pong[COUT_BUFF / BUS_PACK_FACTOR0];

	#if cout_write_MEM == 0
		#pragma HLS bind_storage variable=cout_burst_buf_ping type=RAM_T2P impl=BRAM
		#pragma HLS bind_storage variable=cout_burst_buf_pong type=RAM_T2P impl=BRAM 
	#elif cout_write_MEM == 1
		#pragma HLS bind_storage variable=cout_burst_buf_ping type=RAM_T2P impl=URAM
		#pragma HLS bind_storage variable=cout_burst_buf_pong type=RAM_T2P impl=URAM  
	#endif

		// iterators
	uint num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

	uint cout_offset = 0;

	uint num_iter_prev = 0;
	uint in_h_iter_prev = 0;
	uint in_w_iter_prev = 0;

		// parameters
		// inst0
	ap_uint<32> LAYER_IN_NUM_HW;
	ap_uint<32> LAYER_OUT_NUM_HW;
	ap_uint<32> LAYER_IN_H_HW;
	ap_uint<32> LAYER_IN_W_HW;
	ap_uint<32> LAYER_OUT_H_HW;
	ap_uint<32> LAYER_OUT_W_HW;
  ap_uint<16> LAYER_OUT_H_NP;
  ap_uint<16> LAYER_OUT_H_SP;
  ap_uint<16> LAYER_OUT_W_EP;
  ap_uint<16> LAYER_OUT_W_WP;
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
	ap_uint<16> FILTER_S2;
	ap_uint<32> STRIDE;
		// inst3
	ap_uint<32> LAYER_EN;
	ap_uint<32> PREV_CIN_OFFSET;
	ap_uint<16> LAYER_IN_NUM_T;
	ap_uint<16> LAYER_OUT_NUM_T;
	ap_uint<32> LAYER_IN_H_T;
	ap_uint<32> LAYER_IN_W_T;
	ap_uint<1>  CONV_1ST_EN;
	ap_uint<1>  DEPTH_CONV_EN;
	ap_uint<1>  CONV_EN;
	ap_uint<1>  RELU_EN;
	ap_uint<1>  RELU6_EN;
	ap_uint<1>  POOL_EN;
	ap_uint<1>  UP_SAMPLE_EN;
	ap_uint<1>  BIAS_EN;
	ap_uint<1>  INTER_LOAD_EN;
	ap_uint<1>  INTER_WRITE_EN;
  ap_uint<1>  ADD_EN;

	ap_uint<16> LAYER_CONV_TYPE;	
  ap_uint<16> LAYER_TCONV_STRIDE;


		// Read instructions
	ConfigInst inst0 = fifo_config_in.read();
	ConfigInst inst1 = fifo_config_in.read();
	ConfigInst inst2 = fifo_config_in.read();
	ConfigInst inst3 = fifo_config_in.read();
	ConfigInst inst4 = fifo_config_in.read();
	ConfigInst inst5 = fifo_config_in.read();

	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);

	bool en_prev;
	bool max_pool_prev;
	bool up_sample_prev;
  bool t_conv_prev;
	bool change_layout;
	uint LAYER_IN_NUM_prev;
	uint LAYER_OUT_NUM_prev;
	uint LAYER_IN_NUM_T_prev;
	uint LAYER_OUT_NUM_T_prev;
	uint LAYER_IN_H_T_prev;
	uint LAYER_IN_W_T_prev;
	uint LAYER_OUT_H_HW_prev;
	uint LAYER_OUT_W_HW_prev;
	uint cout_offset_prev;

	bool write_done = 0;
	uint task_cnt = 0;
	uint iter_h = 1;
	uint num_tile = 0;
  uint channel_iter = 0;
	uint inter_tile = 0;
	uint ind_w = 0;
	uint ind_w_t = 0;
	uint num_tile_prev = 0;
	uint ind_w_t_prev = 0;
	uint ind_w_prev = 0;
	bool layer_start = 0;
	bool done = 0;
	bool change_layout_prev = 0;
  int count = 0;
		// We assum that cin has been pre-padded with zeros
	while(!done){
    	// cout<<count++<<endl;
		if (layer_start){
			inst0 = fifo_config_in.read();
			inst1 = fifo_config_in.read();
			inst2 = fifo_config_in.read();
			inst3 = fifo_config_in.read();
			inst4 = fifo_config_in.read();
			inst5 = fifo_config_in.read();
			layer_start = 0;
		}

			// Refer to cin_load module to understand the meaning of the instructions
			// inst0
		LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
		LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
		LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
		LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
    LAYER_OUT_H_NP   = inst0(32*4+15, 32*4);
    LAYER_OUT_H_SP   = inst0(32*4+31, 32*4+16);
    LAYER_OUT_W_EP   = inst0(32*5+15, 32*5);
    LAYER_OUT_W_WP   = inst0(32*5+31, 32*5+16);
    // cout<<"LAYER_OUT_H_NP: "<<LAYER_OUT_H_NP<<endl;
    // cout<<"LAYER_OUT_H_SP: "<<LAYER_OUT_H_SP<<endl;
    // cout<<"LAYER_OUT_W_EP: "<<LAYER_OUT_W_EP<<endl;
    // cout<<"LAYER_OUT_W_WP: "<<LAYER_OUT_W_WP<<endl;
		// LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
		// LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
			// inst1
		LAYER_IN_NUM     = inst1(32*0+31, 32*0);
		LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
		LAYER_IN_H       = inst1(32*2+31, 32*2);
		LAYER_IN_W       = inst1(32*3+31, 32*3);
		LAYER_OUT_H      = inst1(32*4+31, 32*4);
		LAYER_OUT_W      = inst1(32*5+31, 32*5);
    LAYER_OUT_H_HW   = LAYER_OUT_H + LAYER_OUT_H_NP + LAYER_OUT_H_SP;
		LAYER_OUT_W_HW   = LAYER_OUT_W + LAYER_OUT_W_EP + LAYER_OUT_W_WP;
    // cout<<"LAYER_OUT_H_HW: "<<LAYER_OUT_H_HW<<endl;
    // cout<<"LAYER_OUT_W_HW: "<<LAYER_OUT_W_HW<<endl;
    // exit(0);
			// inst2
		CIN_OFFSET       = inst2(32*0+31, 32*0);
		WEIGHT_OFFSET    = inst2(32*1+31, 32*1);
		BIAS_OFFSET      = inst2(32*2+31, 32*2);
		COUT_OFFSET      = inst2(32*3+31, 32*3);
		FILTER_S1        = inst2(32*4+15, 32*4);
		FILTER_S2        = inst2(32*4+31, 32*4+16);
		STRIDE           = inst2(32*5+31, 32*5);
			// inst3
		LAYER_EN         = inst3(32*0+31, 32*0);
		PREV_CIN_OFFSET  = inst3(32*1+31, 32*1);
		LAYER_IN_NUM_T   = inst3(32*2+15, 32*2);
		LAYER_OUT_NUM_T  = inst3(32*2+31, 32*2+16);
		LAYER_IN_H_T     = inst3(32*3+31, 32*3);
		LAYER_IN_W_T     = inst3(32*4+31, 32*4);

			//	//	///inst5	//	//	//	//	//	//	///
		LAYER_CONV_TYPE = inst5(32*0+15, 32*0);
    LAYER_TCONV_STRIDE 	= inst5(32*2+15, 32*2);

		CONV_1ST_EN    = LAYER_EN[0];
		DEPTH_CONV_EN  = LAYER_EN[1];
		CONV_EN        = LAYER_EN[2];
		RELU_EN        = LAYER_EN[3];
		RELU6_EN       = LAYER_EN[4];
		POOL_EN        = LAYER_EN[5];
		UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
		BIAS_EN        = LAYER_EN[7];
		INTER_LOAD_EN  = LAYER_EN[8];
		INTER_WRITE_EN = LAYER_EN[9];
    ADD_EN         = LAYER_EN[17];



		// Set up some configuration signals
		cout_offset = COUT_OFFSET;
		bool en = (POOL_EN && !ADD_EN) || (POOL_EN == 0 && STRIDE == 2);
		bool separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
		bool conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
		bool max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);
		bool up_sample = UP_SAMPLE_EN;
    LAYER_TCONV_STRIDE = UP_SAMPLE_EN || (LAYER_CONV_TYPE==1)? 2 : 1;
    bool t_conv = (LAYER_CONV_TYPE == 1)? 1 : 0;
    // cout<<LAYER_OUT_H_HW<<" "<<LAYER_OUT_H<<endl;
    // cout<<LAYER_OUT_W_HW<<" "<<LAYER_OUT_W<<endl;
    
		change_layout = (((LAYER_OUT_W_HW == LAYER_OUT_W) || (LAYER_OUT_W_HW == LAYER_IN_W_T)) && ((LAYER_OUT_H_HW == LAYER_OUT_H) || (LAYER_OUT_H_HW == LAYER_IN_H_T))); 	// if next filter = 1 : change the layout to num_tile, h_t, w_t, in_t
    // cout<<"change_layout: "<<change_layout<<" t_conv: "<<t_conv<<" t_conv_prev: "<<t_conv_prev<<" LAYER_CONV_TYPE: "<<LAYER_CONV_TYPE<<endl;
			// If it is supposed to store the result in DRAM
		if (INTER_WRITE_EN == 0){
			if (task_cnt == 0){
					// First, read the data of the first tile from FIFO
				cout_write_fifo_read(
						cout_burst_buf_ping, fifo_cout, en, (up_sample || t_conv),
						LAYER_IN_NUM, LAYER_OUT_H, LAYER_OUT_W,
						LAYER_IN_NUM_T, LAYER_OUT_NUM_T,
						LAYER_IN_H_T, LAYER_IN_W_T,
						in_h_iter, in_w_iter,
            LAYER_TCONV_STRIDE
				);
			} else {
					// Apply double buffering for reading the data from FIFO and writing to DRAM
				if (task_cnt % 2 == 1){
					cout_write_fifo_read(
							cout_burst_buf_pong, fifo_cout, en, (up_sample || t_conv),
							LAYER_IN_NUM, LAYER_OUT_H, LAYER_OUT_W,
							LAYER_IN_NUM_T, LAYER_OUT_NUM_T,
							LAYER_IN_H_T, LAYER_IN_W_T,
							in_h_iter, in_w_iter,
              LAYER_TCONV_STRIDE
					);
					cout_write_ddr_write(
							cout_burst_buf_ping, global_cout,
							en_prev, (up_sample_prev || t_conv_prev),
							num_iter_prev, in_h_iter_prev, in_w_iter_prev,
							LAYER_IN_NUM_prev, LAYER_OUT_NUM_prev,
							LAYER_IN_NUM_T_prev, LAYER_OUT_NUM_T_prev,
							LAYER_IN_H_T_prev, LAYER_IN_W_T_prev,
							LAYER_OUT_H_HW_prev, LAYER_OUT_W_HW_prev,
							num_tile_prev,
							ind_w_t_prev,
							ind_w_prev,
							cout_offset_prev,
							change_layout_prev,
							!write_done,
              LAYER_TCONV_STRIDE
					);
				} else {
					cout_write_fifo_read(
							cout_burst_buf_ping, fifo_cout, en, (up_sample || t_conv),
							LAYER_IN_NUM, LAYER_OUT_H, LAYER_OUT_W,
							LAYER_IN_NUM_T, LAYER_OUT_NUM_T,
							LAYER_IN_H_T, LAYER_IN_W_T,
							in_h_iter, in_w_iter,
              LAYER_TCONV_STRIDE
					);

					cout_write_ddr_write(
							cout_burst_buf_pong, global_cout,
							en_prev, (up_sample_prev || t_conv_prev),
							num_iter_prev, in_h_iter_prev, in_w_iter_prev,
							LAYER_IN_NUM_prev, LAYER_OUT_NUM_prev,
							LAYER_IN_NUM_T_prev, LAYER_OUT_NUM_T_prev,
							LAYER_IN_H_T_prev, LAYER_IN_W_T_prev,
							LAYER_OUT_H_HW_prev, LAYER_OUT_W_HW_prev,
							num_tile_prev,
							ind_w_t_prev,
							ind_w_prev,
							cout_offset_prev,
							change_layout_prev,
							!write_done,
              LAYER_TCONV_STRIDE
					);
				}
			}

			if (task_cnt > 0){
				write_done = 1;
			}
			
				// need to know the config of the current tile in the next iteration since we are using double buffering
			task_cnt++;
			num_iter_prev = num_iter;
			in_h_iter_prev = in_h_iter;
			in_w_iter_prev = in_w_iter;
			en_prev = en;
			up_sample_prev = up_sample;
      t_conv_prev = t_conv;
			max_pool_prev = max_pool;
			LAYER_IN_NUM_prev = LAYER_IN_NUM;
			LAYER_OUT_NUM_prev = LAYER_OUT_NUM;
			LAYER_IN_NUM_T_prev = LAYER_IN_NUM_T;
			LAYER_OUT_NUM_T_prev = LAYER_OUT_NUM_T;
			LAYER_IN_H_T_prev = LAYER_IN_H_T;
			LAYER_IN_W_T_prev = LAYER_IN_W_T;
			LAYER_OUT_H_HW_prev = LAYER_OUT_H_HW;
			LAYER_OUT_W_HW_prev = LAYER_OUT_W_HW;
			cout_offset_prev = cout_offset;
			num_tile_prev = num_tile;
			ind_w_t_prev = ind_w_t;
			ind_w_prev = ind_w;
			change_layout_prev = change_layout;
			write_done = 0;

		}
    	// cout<<num_tile<<endl;
			// Repeat until all the tiles are stored
		// if (max_pool || up_sample){
    // cout<<num_tile<<endl;
    // num_iter += LAYER_OUT_NUM_T;
    // if (num_iter < LAYER_OUT_NUM){
    //   channel_iter += ((LAYER_IN_W / LAYER_IN_W_T) * (LAYER_IN_H / LAYER_IN_H_T));
    // } else {
    //   channel_iter = 0;
    //   inter_tile++;
    // }
    // num_tile = channel_iter + inter_tile;
    // cout<<num_tile<<" "<<channel_iter<<" "<<inter_tile<<endl;
    num_tile++;
    in_h_iter += LAYER_IN_H_T;
    if (in_h_iter >= LAYER_IN_H){
      in_h_iter = 0;
      in_w_iter += LAYER_IN_W_T;
      if (in_w_iter >= LAYER_IN_W){
        in_w_iter = 0;
        num_iter += LAYER_OUT_NUM_T;
        if (num_iter >= LAYER_OUT_NUM){
          num_iter = 0;
          num_tile = 0;
          layer_start = 1;
          layer_iter += 1;
          if (layer_iter == LAYER_BATCH){
            layer_iter = 0;
            done = 1;
          }
        }
      }
    }
    // }
		// } else if (STRIDE == 2){
		// 	num_tile += 1;
		// 	if (num_tile == (LAYER_IN_H / LAYER_IN_H_T)){
		// 		num_tile = 0;
		// 		ind_w_t += 1;
		// 		if (LAYER_IN_W_T / 2 == LAYER_OUT_W_HW) ind_w_t += 1;
		// 		if (ind_w_t == 2){
		// 			ind_w_t = 0;
		// 			ind_w += (LAYER_IN_H / LAYER_IN_H_T);
		// 		}
		// 	}
		// 	in_h_iter += LAYER_IN_H_T;
		// 	if (in_h_iter >= LAYER_IN_H){
		// 		in_h_iter = 0;
		// 		in_w_iter += LAYER_IN_W_T;
		// 		if (in_w_iter >= LAYER_IN_W){
		// 			in_w_iter = 0;
		// 			num_iter += LAYER_OUT_NUM_T;
		// 			if (num_iter >= LAYER_OUT_NUM){
		// 				num_iter = 0;
		// 				layer_iter += 1;
		// 				layer_start = 1;
		// 				if (layer_iter == LAYER_BATCH){
		// 					layer_iter = 0;
		// 					done = 1;
		// 				}
		// 			}
		// 		}
		// 	}
		// } else {
		// 		//num_tile = task_cnt - 1;
		// 	num_tile = task_cnt;
		// 	in_h_iter += LAYER_IN_H_T;
		// 	if (in_h_iter >= LAYER_IN_H){
		// 		in_h_iter = 0;
		// 		in_w_iter += LAYER_IN_W_T;
		// 		if (in_w_iter >= LAYER_IN_W){
		// 			in_w_iter = 0;
		// 			num_iter += LAYER_OUT_NUM_T;
		// 			if (num_iter >= LAYER_OUT_NUM){
		// 				num_iter = 0;
		// 				layer_iter += 1;
		// 				layer_start = 1;
		// 				if (layer_iter == LAYER_BATCH){
		// 					layer_iter = 0;
		// 					done = 1;
		// 				}
		// 			}
		// 		}
		// 	}

		// }

	}

		// Store the last tile
	if (INTER_WRITE_EN == 0){
		if (task_cnt % 2 == 1){
			cout_write_ddr_write(
					cout_burst_buf_ping, global_cout,
					en_prev, (up_sample_prev || t_conv_prev),
					num_iter_prev, in_h_iter_prev, in_w_iter_prev,
					LAYER_IN_NUM_prev, LAYER_OUT_NUM_prev,
					LAYER_IN_NUM_T_prev, LAYER_OUT_NUM_T_prev,
					LAYER_IN_H_T_prev, LAYER_IN_W_T_prev,
					LAYER_OUT_H_HW_prev, LAYER_OUT_W_HW_prev,
					num_tile_prev,
					ind_w_t_prev,
					ind_w_prev,
					cout_offset_prev,
					change_layout_prev,
					!write_done,
          LAYER_TCONV_STRIDE
			);
		} else {
			cout_write_ddr_write(
					cout_burst_buf_pong, global_cout,
					en_prev, (up_sample_prev || t_conv_prev),
					num_iter_prev, in_h_iter_prev, in_w_iter_prev,
					LAYER_IN_NUM_prev, LAYER_OUT_NUM_prev,
					LAYER_IN_NUM_T_prev, LAYER_OUT_NUM_T_prev,
					LAYER_IN_H_T_prev, LAYER_IN_W_T_prev,
					LAYER_OUT_H_HW_prev, LAYER_OUT_W_HW_prev,
					num_tile_prev,
					ind_w_t_prev,
					ind_w_prev,
					cout_offset_prev,
					change_layout_prev,
					!write_done,
          LAYER_TCONV_STRIDE
			);
		}
	}
    inst++;
    if(inst<inst_count)
      out_sync.write(inst);
  }
  if(inst == inst_count){
    inst_done = 1;
  }
  }
}void top_kernel(
    bus_mem_0 dram_b0,
    bus_mem_1 dram_weights,
    bus_mem_2 dram_biases,
    bus_mem_3 layer_config,
    uint start_inst,
    uint end_inst
){
  // FIFOs
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_0("fifo0_feed0_0");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_1("fifo0_feed0_1");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_2("fifo0_feed0_2");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_3("fifo0_feed0_3");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_4("fifo0_feed0_4");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_5("fifo0_feed0_5");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_6("fifo0_feed0_6");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_7("fifo0_feed0_7");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_8("fifo0_feed0_8");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed0_9("fifo0_feed0_9");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_0("fifo0_feed1_0");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_1("fifo0_feed1_1");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_2("fifo0_feed1_2");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_3("fifo0_feed1_3");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_4("fifo0_feed1_4");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_5("fifo0_feed1_5");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_6("fifo0_feed1_6");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_7("fifo0_feed1_7");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_8("fifo0_feed1_8");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed1_9("fifo0_feed1_9");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_0("fifo0_feed2_0");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_1("fifo0_feed2_1");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_2("fifo0_feed2_2");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_3("fifo0_feed2_3");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_4("fifo0_feed2_4");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_5("fifo0_feed2_5");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_6("fifo0_feed2_6");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_7("fifo0_feed2_7");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_8("fifo0_feed2_8");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed2_9("fifo0_feed2_9");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_0("fifo0_feed3_0");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_1("fifo0_feed3_1");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_2("fifo0_feed3_2");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_3("fifo0_feed3_3");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_4("fifo0_feed3_4");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_5("fifo0_feed3_5");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_6("fifo0_feed3_6");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_7("fifo0_feed3_7");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_8("fifo0_feed3_8");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed3_9("fifo0_feed3_9");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_0("fifo0_feed4_0");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_1("fifo0_feed4_1");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_2("fifo0_feed4_2");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_3("fifo0_feed4_3");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_4("fifo0_feed4_4");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_5("fifo0_feed4_5");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_6("fifo0_feed4_6");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_7("fifo0_feed4_7");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_8("fifo0_feed4_8");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed4_9("fifo0_feed4_9");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_0("fifo0_feed5_0");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_1("fifo0_feed5_1");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_2("fifo0_feed5_2");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_3("fifo0_feed5_3");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_4("fifo0_feed5_4");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_5("fifo0_feed5_5");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_6("fifo0_feed5_6");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_7("fifo0_feed5_7");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_8("fifo0_feed5_8");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed5_9("fifo0_feed5_9");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_0("fifo0_feed6_0");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_1("fifo0_feed6_1");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_2("fifo0_feed6_2");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_3("fifo0_feed6_3");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_4("fifo0_feed6_4");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_5("fifo0_feed6_5");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_6("fifo0_feed6_6");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_7("fifo0_feed6_7");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_8("fifo0_feed6_8");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed6_9("fifo0_feed6_9");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_0("fifo0_feed7_0");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_1("fifo0_feed7_1");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_2("fifo0_feed7_2");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_3("fifo0_feed7_3");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_4("fifo0_feed7_4");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_5("fifo0_feed7_5");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_6("fifo0_feed7_6");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_7("fifo0_feed7_7");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_8("fifo0_feed7_8");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed7_9("fifo0_feed7_9");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_0("fifo0_feed8_0");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_1("fifo0_feed8_1");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_2("fifo0_feed8_2");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_3("fifo0_feed8_3");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_4("fifo0_feed8_4");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_5("fifo0_feed8_5");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_6("fifo0_feed8_6");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_7("fifo0_feed8_7");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_8("fifo0_feed8_8");
  tapa::stream<U1_Data0PEChannelType, 2> fifo0_feed8_9("fifo0_feed8_9");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_0("fifo1_feed0_0");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_1("fifo1_feed0_1");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_2("fifo1_feed0_2");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_3("fifo1_feed0_3");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_4("fifo1_feed0_4");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_5("fifo1_feed0_5");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_6("fifo1_feed0_6");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_7("fifo1_feed0_7");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_8("fifo1_feed0_8");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed0_9("fifo1_feed0_9");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_0("fifo1_feed1_0");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_1("fifo1_feed1_1");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_2("fifo1_feed1_2");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_3("fifo1_feed1_3");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_4("fifo1_feed1_4");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_5("fifo1_feed1_5");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_6("fifo1_feed1_6");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_7("fifo1_feed1_7");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_8("fifo1_feed1_8");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed1_9("fifo1_feed1_9");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_0("fifo1_feed2_0");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_1("fifo1_feed2_1");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_2("fifo1_feed2_2");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_3("fifo1_feed2_3");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_4("fifo1_feed2_4");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_5("fifo1_feed2_5");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_6("fifo1_feed2_6");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_7("fifo1_feed2_7");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_8("fifo1_feed2_8");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed2_9("fifo1_feed2_9");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_0("fifo1_feed3_0");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_1("fifo1_feed3_1");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_2("fifo1_feed3_2");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_3("fifo1_feed3_3");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_4("fifo1_feed3_4");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_5("fifo1_feed3_5");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_6("fifo1_feed3_6");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_7("fifo1_feed3_7");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_8("fifo1_feed3_8");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed3_9("fifo1_feed3_9");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_0("fifo1_feed4_0");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_1("fifo1_feed4_1");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_2("fifo1_feed4_2");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_3("fifo1_feed4_3");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_4("fifo1_feed4_4");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_5("fifo1_feed4_5");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_6("fifo1_feed4_6");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_7("fifo1_feed4_7");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_8("fifo1_feed4_8");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed4_9("fifo1_feed4_9");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_0("fifo1_feed5_0");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_1("fifo1_feed5_1");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_2("fifo1_feed5_2");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_3("fifo1_feed5_3");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_4("fifo1_feed5_4");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_5("fifo1_feed5_5");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_6("fifo1_feed5_6");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_7("fifo1_feed5_7");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_8("fifo1_feed5_8");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed5_9("fifo1_feed5_9");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_0("fifo1_feed6_0");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_1("fifo1_feed6_1");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_2("fifo1_feed6_2");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_3("fifo1_feed6_3");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_4("fifo1_feed6_4");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_5("fifo1_feed6_5");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_6("fifo1_feed6_6");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_7("fifo1_feed6_7");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_8("fifo1_feed6_8");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed6_9("fifo1_feed6_9");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_0("fifo1_feed7_0");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_1("fifo1_feed7_1");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_2("fifo1_feed7_2");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_3("fifo1_feed7_3");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_4("fifo1_feed7_4");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_5("fifo1_feed7_5");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_6("fifo1_feed7_6");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_7("fifo1_feed7_7");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_8("fifo1_feed7_8");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed7_9("fifo1_feed7_9");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_0("fifo1_feed8_0");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_1("fifo1_feed8_1");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_2("fifo1_feed8_2");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_3("fifo1_feed8_3");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_4("fifo1_feed8_4");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_5("fifo1_feed8_5");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_6("fifo1_feed8_6");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_7("fifo1_feed8_7");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_8("fifo1_feed8_8");
  tapa::stream<U1_Data1PEChannelType, 2> fifo1_feed8_9("fifo1_feed8_9");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_0("fifo2_collect0_0");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_1("fifo2_collect0_1");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_2("fifo2_collect0_2");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_3("fifo2_collect0_3");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_4("fifo2_collect0_4");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_5("fifo2_collect0_5");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_6("fifo2_collect0_6");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_7("fifo2_collect0_7");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_8("fifo2_collect0_8");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect0_9("fifo2_collect0_9");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_0("fifo2_collect1_0");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_1("fifo2_collect1_1");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_2("fifo2_collect1_2");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_3("fifo2_collect1_3");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_4("fifo2_collect1_4");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_5("fifo2_collect1_5");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_6("fifo2_collect1_6");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_7("fifo2_collect1_7");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_8("fifo2_collect1_8");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect1_9("fifo2_collect1_9");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_0("fifo2_collect2_0");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_1("fifo2_collect2_1");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_2("fifo2_collect2_2");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_3("fifo2_collect2_3");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_4("fifo2_collect2_4");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_5("fifo2_collect2_5");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_6("fifo2_collect2_6");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_7("fifo2_collect2_7");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_8("fifo2_collect2_8");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect2_9("fifo2_collect2_9");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_0("fifo2_collect3_0");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_1("fifo2_collect3_1");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_2("fifo2_collect3_2");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_3("fifo2_collect3_3");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_4("fifo2_collect3_4");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_5("fifo2_collect3_5");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_6("fifo2_collect3_6");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_7("fifo2_collect3_7");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_8("fifo2_collect3_8");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect3_9("fifo2_collect3_9");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_0("fifo2_collect4_0");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_1("fifo2_collect4_1");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_2("fifo2_collect4_2");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_3("fifo2_collect4_3");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_4("fifo2_collect4_4");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_5("fifo2_collect4_5");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_6("fifo2_collect4_6");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_7("fifo2_collect4_7");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_8("fifo2_collect4_8");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect4_9("fifo2_collect4_9");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_0("fifo2_collect5_0");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_1("fifo2_collect5_1");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_2("fifo2_collect5_2");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_3("fifo2_collect5_3");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_4("fifo2_collect5_4");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_5("fifo2_collect5_5");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_6("fifo2_collect5_6");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_7("fifo2_collect5_7");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_8("fifo2_collect5_8");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect5_9("fifo2_collect5_9");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_0("fifo2_collect6_0");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_1("fifo2_collect6_1");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_2("fifo2_collect6_2");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_3("fifo2_collect6_3");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_4("fifo2_collect6_4");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_5("fifo2_collect6_5");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_6("fifo2_collect6_6");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_7("fifo2_collect6_7");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_8("fifo2_collect6_8");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect6_9("fifo2_collect6_9");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_0("fifo2_collect7_0");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_1("fifo2_collect7_1");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_2("fifo2_collect7_2");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_3("fifo2_collect7_3");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_4("fifo2_collect7_4");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_5("fifo2_collect7_5");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_6("fifo2_collect7_6");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_7("fifo2_collect7_7");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_8("fifo2_collect7_8");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect7_9("fifo2_collect7_9");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_0("fifo2_collect8_0");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_1("fifo2_collect8_1");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_2("fifo2_collect8_2");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_3("fifo2_collect8_3");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_4("fifo2_collect8_4");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_5("fifo2_collect8_5");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_6("fifo2_collect8_6");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_7("fifo2_collect8_7");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_8("fifo2_collect8_8");
  tapa::stream<U1_Data2PEChannelType, 2> fifo2_collect8_9("fifo2_collect8_9");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer0("fifo0_transfer0");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer1("fifo0_transfer1");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer2("fifo0_transfer2");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer3("fifo0_transfer3");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer4("fifo0_transfer4");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer5("fifo0_transfer5");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer6("fifo0_transfer6");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer7("fifo0_transfer7");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer8("fifo0_transfer8");
  tapa::stream<U1_Data0TransferChannelType, 2> fifo0_transfer9("fifo0_transfer9");
  tapa::stream<U1_Data1TransferChannelType, 2> fifo1_transfer0("fifo1_transfer0");
  tapa::stream<U1_Data1TransferChannelType, 2> fifo1_transfer1("fifo1_transfer1");
  tapa::stream<U1_Data1TransferChannelType, 2> fifo1_transfer2("fifo1_transfer2");
  tapa::stream<U1_Data1TransferChannelType, 2> fifo1_transfer3("fifo1_transfer3");
  tapa::stream<U1_Data1TransferChannelType, 2> fifo1_transfer4("fifo1_transfer4");
  tapa::stream<U1_Data1TransferChannelType, 2> fifo1_transfer5("fifo1_transfer5");
  tapa::stream<U1_Data1TransferChannelType, 2> fifo1_transfer6("fifo1_transfer6");
  tapa::stream<U1_Data1TransferChannelType, 2> fifo1_transfer7("fifo1_transfer7");
  tapa::stream<U1_Data1TransferChannelType, 2> fifo1_transfer8("fifo1_transfer8");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer0("fifo2_transfer0");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer1("fifo2_transfer1");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer2("fifo2_transfer2");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer3("fifo2_transfer3");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer4("fifo2_transfer4");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer5("fifo2_transfer5");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer6("fifo2_transfer6");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer7("fifo2_transfer7");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer8("fifo2_transfer8");
  tapa::stream<U1_Data2TransferChannelType, 2> fifo2_transfer9("fifo2_transfer9");
  tapa::stream<uint, 16> fifo_DataFeed0Head_config_out0("fifo_DataFeed0Head_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Head_config_out1("fifo_DataFeed0Head_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed1Head_config_out0("fifo_DataFeed1Head_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Engine0_config_out0("fifo_DataFeed0Engine0_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Engine0_config_out1("fifo_DataFeed0Engine0_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed0Engine1_config_out0("fifo_DataFeed0Engine1_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Engine1_config_out1("fifo_DataFeed0Engine1_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed0Engine2_config_out0("fifo_DataFeed0Engine2_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Engine2_config_out1("fifo_DataFeed0Engine2_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed0Engine3_config_out0("fifo_DataFeed0Engine3_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Engine3_config_out1("fifo_DataFeed0Engine3_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed0Engine4_config_out0("fifo_DataFeed0Engine4_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Engine4_config_out1("fifo_DataFeed0Engine4_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed0Engine5_config_out0("fifo_DataFeed0Engine5_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Engine5_config_out1("fifo_DataFeed0Engine5_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed0Engine6_config_out0("fifo_DataFeed0Engine6_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Engine6_config_out1("fifo_DataFeed0Engine6_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed0Engine7_config_out0("fifo_DataFeed0Engine7_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed0Engine7_config_out1("fifo_DataFeed0Engine7_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed0Engine8_config_out1("fifo_DataFeed0Engine8_config_out1");
  tapa::stream<uint, 16> fifo_DataFeed1Engine0_config_out0("fifo_DataFeed1Engine0_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed1Engine1_config_out0("fifo_DataFeed1Engine1_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed1Engine2_config_out0("fifo_DataFeed1Engine2_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed1Engine3_config_out0("fifo_DataFeed1Engine3_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed1Engine4_config_out0("fifo_DataFeed1Engine4_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed1Engine5_config_out0("fifo_DataFeed1Engine5_config_out0");
  tapa::stream<uint, 16> fifo_DataFeed1Engine6_config_out0("fifo_DataFeed1Engine6_config_out0");

  tapa::stream<uint, 16> fifo_DataCollect2Engine0_config_out("fifo_DataFeed2Engine0_config_out0");
  tapa::stream<uint, 16> fifo_DataCollect2Engine1_config_out("fifo_DataFeed2Engine1_config_out0");
  tapa::stream<uint, 16> fifo_DataCollect2Engine2_config_out("fifo_DataFeed2Engine2_config_out0");
  tapa::stream<uint, 16> fifo_DataCollect2Engine3_config_out("fifo_DataFeed2Engine3_config_out0");
  tapa::stream<uint, 16> fifo_DataCollect2Engine4_config_out("fifo_DataFeed2Engine4_config_out0");
  tapa::stream<uint, 16> fifo_DataCollect2Engine5_config_out("fifo_DataFeed2Engine5_config_out0");
  tapa::stream<uint, 16> fifo_DataCollect2Engine6_config_out("fifo_DataFeed2Engine6_config_out0");
  tapa::stream<uint, 16> fifo_DataCollect2Engine7_config_out("fifo_DataFeed2Engine7_config_out0");
  tapa::stream<uint, 16> fifo_DataCollect2Engine8_config_out("fifo_DataFeed2Engine8_config_out0");

  tapa::stream<uint, 2> fifo_PE0_0_op0_config_out("fifo_PE0_0_op0_config_out");
  tapa::stream<uint, 2> fifo_PE0_0_op1_config_out("fifo_PE0_0_op1_config_out");
  tapa::stream<uint, 2> fifo_PE0_0_compute_config_out("fifo_PE0_0_compute_config_out");
  tapa::stream<uint, 2> fifo_PE0_0_res_config_out("fifo_PE0_0_res_config_out");
  tapa::stream<uint, 2> fifo_PE0_1_op0_config_out("fifo_PE0_1_op0_config_out");
  tapa::stream<uint, 2> fifo_PE0_1_op1_config_out("fifo_PE0_1_op1_config_out");
  tapa::stream<uint, 2> fifo_PE0_1_compute_config_out("fifo_PE0_1_compute_config_out");
  tapa::stream<uint, 2> fifo_PE0_1_res_config_out("fifo_PE0_1_res_config_out");
  tapa::stream<uint, 2> fifo_PE0_2_op0_config_out("fifo_PE0_2_op0_config_out");
  tapa::stream<uint, 2> fifo_PE0_2_op1_config_out("fifo_PE0_2_op1_config_out");
  tapa::stream<uint, 2> fifo_PE0_2_compute_config_out("fifo_PE0_2_compute_config_out");
  tapa::stream<uint, 2> fifo_PE0_2_res_config_out("fifo_PE0_2_res_config_out");
  tapa::stream<uint, 2> fifo_PE0_3_op0_config_out("fifo_PE0_3_op0_config_out");
  tapa::stream<uint, 2> fifo_PE0_3_op1_config_out("fifo_PE0_3_op1_config_out");
  tapa::stream<uint, 2> fifo_PE0_3_compute_config_out("fifo_PE0_3_compute_config_out");
  tapa::stream<uint, 2> fifo_PE0_3_res_config_out("fifo_PE0_3_res_config_out");
  tapa::stream<uint, 2> fifo_PE0_4_op0_config_out("fifo_PE0_4_op0_config_out");
  tapa::stream<uint, 2> fifo_PE0_4_op1_config_out("fifo_PE0_4_op1_config_out");
  tapa::stream<uint, 2> fifo_PE0_4_compute_config_out("fifo_PE0_4_compute_config_out");
  tapa::stream<uint, 2> fifo_PE0_4_res_config_out("fifo_PE0_4_res_config_out");
  tapa::stream<uint, 2> fifo_PE0_5_op0_config_out("fifo_PE0_5_op0_config_out");
  tapa::stream<uint, 2> fifo_PE0_5_op1_config_out("fifo_PE0_5_op1_config_out");
  tapa::stream<uint, 2> fifo_PE0_5_compute_config_out("fifo_PE0_5_compute_config_out");
  tapa::stream<uint, 2> fifo_PE0_5_res_config_out("fifo_PE0_5_res_config_out");
  tapa::stream<uint, 2> fifo_PE0_6_op0_config_out("fifo_PE0_6_op0_config_out");
  tapa::stream<uint, 2> fifo_PE0_6_op1_config_out("fifo_PE0_6_op1_config_out");
  tapa::stream<uint, 2> fifo_PE0_6_compute_config_out("fifo_PE0_6_compute_config_out");
  tapa::stream<uint, 2> fifo_PE0_6_res_config_out("fifo_PE0_6_res_config_out");
  tapa::stream<uint, 2> fifo_PE0_7_op0_config_out("fifo_PE0_7_op0_config_out");
  tapa::stream<uint, 2> fifo_PE0_7_op1_config_out("fifo_PE0_7_op1_config_out");
  tapa::stream<uint, 2> fifo_PE0_7_compute_config_out("fifo_PE0_7_compute_config_out");
  tapa::stream<uint, 2> fifo_PE0_7_res_config_out("fifo_PE0_7_res_config_out");
  tapa::stream<uint, 2> fifo_PE0_8_op0_config_out("fifo_PE0_8_op0_config_out");
  tapa::stream<uint, 2> fifo_PE0_8_op1_config_out("fifo_PE0_8_op1_config_out");
  tapa::stream<uint, 2> fifo_PE0_8_compute_config_out("fifo_PE0_8_compute_config_out");
  tapa::stream<uint, 2> fifo_PE0_8_res_config_out("fifo_PE0_8_res_config_out");
  tapa::stream<uint, 2> fifo_PE1_0_op0_config_out("fifo_PE1_0_op0_config_out");
  tapa::stream<uint, 2> fifo_PE1_0_op1_config_out("fifo_PE1_0_op1_config_out");
  tapa::stream<uint, 2> fifo_PE1_0_compute_config_out("fifo_PE1_0_compute_config_out");
  tapa::stream<uint, 2> fifo_PE1_0_res_config_out("fifo_PE1_0_res_config_out");
  tapa::stream<uint, 2> fifo_PE1_1_op0_config_out("fifo_PE1_1_op0_config_out");
  tapa::stream<uint, 2> fifo_PE1_1_op1_config_out("fifo_PE1_1_op1_config_out");
  tapa::stream<uint, 2> fifo_PE1_1_compute_config_out("fifo_PE1_1_compute_config_out");
  tapa::stream<uint, 2> fifo_PE1_1_res_config_out("fifo_PE1_1_res_config_out");
  tapa::stream<uint, 2> fifo_PE1_2_op0_config_out("fifo_PE1_2_op0_config_out");
  tapa::stream<uint, 2> fifo_PE1_2_op1_config_out("fifo_PE1_2_op1_config_out");
  tapa::stream<uint, 2> fifo_PE1_2_compute_config_out("fifo_PE1_2_compute_config_out");
  tapa::stream<uint, 2> fifo_PE1_2_res_config_out("fifo_PE1_2_res_config_out");
  tapa::stream<uint, 2> fifo_PE1_3_op0_config_out("fifo_PE1_3_op0_config_out");
  tapa::stream<uint, 2> fifo_PE1_3_op1_config_out("fifo_PE1_3_op1_config_out");
  tapa::stream<uint, 2> fifo_PE1_3_compute_config_out("fifo_PE1_3_compute_config_out");
  tapa::stream<uint, 2> fifo_PE1_3_res_config_out("fifo_PE1_3_res_config_out");
  tapa::stream<uint, 2> fifo_PE1_4_op0_config_out("fifo_PE1_4_op0_config_out");
  tapa::stream<uint, 2> fifo_PE1_4_op1_config_out("fifo_PE1_4_op1_config_out");
  tapa::stream<uint, 2> fifo_PE1_4_compute_config_out("fifo_PE1_4_compute_config_out");
  tapa::stream<uint, 2> fifo_PE1_4_res_config_out("fifo_PE1_4_res_config_out");
  tapa::stream<uint, 2> fifo_PE1_5_op0_config_out("fifo_PE1_5_op0_config_out");
  tapa::stream<uint, 2> fifo_PE1_5_op1_config_out("fifo_PE1_5_op1_config_out");
  tapa::stream<uint, 2> fifo_PE1_5_compute_config_out("fifo_PE1_5_compute_config_out");
  tapa::stream<uint, 2> fifo_PE1_5_res_config_out("fifo_PE1_5_res_config_out");
  tapa::stream<uint, 2> fifo_PE1_6_op0_config_out("fifo_PE1_6_op0_config_out");
  tapa::stream<uint, 2> fifo_PE1_6_op1_config_out("fifo_PE1_6_op1_config_out");
  tapa::stream<uint, 2> fifo_PE1_6_compute_config_out("fifo_PE1_6_compute_config_out");
  tapa::stream<uint, 2> fifo_PE1_6_res_config_out("fifo_PE1_6_res_config_out");
  tapa::stream<uint, 2> fifo_PE1_7_op0_config_out("fifo_PE1_7_op0_config_out");
  tapa::stream<uint, 2> fifo_PE1_7_op1_config_out("fifo_PE1_7_op1_config_out");
  tapa::stream<uint, 2> fifo_PE1_7_compute_config_out("fifo_PE1_7_compute_config_out");
  tapa::stream<uint, 2> fifo_PE1_7_res_config_out("fifo_PE1_7_res_config_out");
  tapa::stream<uint, 2> fifo_PE1_8_op0_config_out("fifo_PE1_8_op0_config_out");
  tapa::stream<uint, 2> fifo_PE1_8_op1_config_out("fifo_PE1_8_op1_config_out");
  tapa::stream<uint, 2> fifo_PE1_8_compute_config_out("fifo_PE1_8_compute_config_out");
  tapa::stream<uint, 2> fifo_PE1_8_res_config_out("fifo_PE1_8_res_config_out");
  tapa::stream<uint, 2> fifo_PE2_0_op0_config_out("fifo_PE2_0_op0_config_out");
  tapa::stream<uint, 2> fifo_PE2_0_op1_config_out("fifo_PE2_0_op1_config_out");
  tapa::stream<uint, 2> fifo_PE2_0_compute_config_out("fifo_PE2_0_compute_config_out");
  tapa::stream<uint, 2> fifo_PE2_0_res_config_out("fifo_PE2_0_res_config_out");
  tapa::stream<uint, 2> fifo_PE2_1_op0_config_out("fifo_PE2_1_op0_config_out");
  tapa::stream<uint, 2> fifo_PE2_1_op1_config_out("fifo_PE2_1_op1_config_out");
  tapa::stream<uint, 2> fifo_PE2_1_compute_config_out("fifo_PE2_1_compute_config_out");
  tapa::stream<uint, 2> fifo_PE2_1_res_config_out("fifo_PE2_1_res_config_out");
  tapa::stream<uint, 2> fifo_PE2_2_op0_config_out("fifo_PE2_2_op0_config_out");
  tapa::stream<uint, 2> fifo_PE2_2_op1_config_out("fifo_PE2_2_op1_config_out");
  tapa::stream<uint, 2> fifo_PE2_2_compute_config_out("fifo_PE2_2_compute_config_out");
  tapa::stream<uint, 2> fifo_PE2_2_res_config_out("fifo_PE2_2_res_config_out");
  tapa::stream<uint, 2> fifo_PE2_3_op0_config_out("fifo_PE2_3_op0_config_out");
  tapa::stream<uint, 2> fifo_PE2_3_op1_config_out("fifo_PE2_3_op1_config_out");
  tapa::stream<uint, 2> fifo_PE2_3_compute_config_out("fifo_PE2_3_compute_config_out");
  tapa::stream<uint, 2> fifo_PE2_3_res_config_out("fifo_PE2_3_res_config_out");
  tapa::stream<uint, 2> fifo_PE2_4_op0_config_out("fifo_PE2_4_op0_config_out");
  tapa::stream<uint, 2> fifo_PE2_4_op1_config_out("fifo_PE2_4_op1_config_out");
  tapa::stream<uint, 2> fifo_PE2_4_compute_config_out("fifo_PE2_4_compute_config_out");
  tapa::stream<uint, 2> fifo_PE2_4_res_config_out("fifo_PE2_4_res_config_out");
  tapa::stream<uint, 2> fifo_PE2_5_op0_config_out("fifo_PE2_5_op0_config_out");
  tapa::stream<uint, 2> fifo_PE2_5_op1_config_out("fifo_PE2_5_op1_config_out");
  tapa::stream<uint, 2> fifo_PE2_5_compute_config_out("fifo_PE2_5_compute_config_out");
  tapa::stream<uint, 2> fifo_PE2_5_res_config_out("fifo_PE2_5_res_config_out");
  tapa::stream<uint, 2> fifo_PE2_6_op0_config_out("fifo_PE2_6_op0_config_out");
  tapa::stream<uint, 2> fifo_PE2_6_op1_config_out("fifo_PE2_6_op1_config_out");
  tapa::stream<uint, 2> fifo_PE2_6_compute_config_out("fifo_PE2_6_compute_config_out");
  tapa::stream<uint, 2> fifo_PE2_6_res_config_out("fifo_PE2_6_res_config_out");
  tapa::stream<uint, 2> fifo_PE2_7_op0_config_out("fifo_PE2_7_op0_config_out");
  tapa::stream<uint, 2> fifo_PE2_7_op1_config_out("fifo_PE2_7_op1_config_out");
  tapa::stream<uint, 2> fifo_PE2_7_compute_config_out("fifo_PE2_7_compute_config_out");
  tapa::stream<uint, 2> fifo_PE2_7_res_config_out("fifo_PE2_7_res_config_out");
  tapa::stream<uint, 2> fifo_PE2_8_op0_config_out("fifo_PE2_8_op0_config_out");
  tapa::stream<uint, 2> fifo_PE2_8_op1_config_out("fifo_PE2_8_op1_config_out");
  tapa::stream<uint, 2> fifo_PE2_8_compute_config_out("fifo_PE2_8_compute_config_out");
  tapa::stream<uint, 2> fifo_PE2_8_res_config_out("fifo_PE2_8_res_config_out");
  tapa::stream<uint, 2> fifo_PE3_0_op0_config_out("fifo_PE3_0_op0_config_out");
  tapa::stream<uint, 2> fifo_PE3_0_op1_config_out("fifo_PE3_0_op1_config_out");
  tapa::stream<uint, 2> fifo_PE3_0_compute_config_out("fifo_PE3_0_compute_config_out");
  tapa::stream<uint, 2> fifo_PE3_0_res_config_out("fifo_PE3_0_res_config_out");
  tapa::stream<uint, 2> fifo_PE3_1_op0_config_out("fifo_PE3_1_op0_config_out");
  tapa::stream<uint, 2> fifo_PE3_1_op1_config_out("fifo_PE3_1_op1_config_out");
  tapa::stream<uint, 2> fifo_PE3_1_compute_config_out("fifo_PE3_1_compute_config_out");
  tapa::stream<uint, 2> fifo_PE3_1_res_config_out("fifo_PE3_1_res_config_out");
  tapa::stream<uint, 2> fifo_PE3_2_op0_config_out("fifo_PE3_2_op0_config_out");
  tapa::stream<uint, 2> fifo_PE3_2_op1_config_out("fifo_PE3_2_op1_config_out");
  tapa::stream<uint, 2> fifo_PE3_2_compute_config_out("fifo_PE3_2_compute_config_out");
  tapa::stream<uint, 2> fifo_PE3_2_res_config_out("fifo_PE3_2_res_config_out");
  tapa::stream<uint, 2> fifo_PE3_3_op0_config_out("fifo_PE3_3_op0_config_out");
  tapa::stream<uint, 2> fifo_PE3_3_op1_config_out("fifo_PE3_3_op1_config_out");
  tapa::stream<uint, 2> fifo_PE3_3_compute_config_out("fifo_PE3_3_compute_config_out");
  tapa::stream<uint, 2> fifo_PE3_3_res_config_out("fifo_PE3_3_res_config_out");
  tapa::stream<uint, 2> fifo_PE3_4_op0_config_out("fifo_PE3_4_op0_config_out");
  tapa::stream<uint, 2> fifo_PE3_4_op1_config_out("fifo_PE3_4_op1_config_out");
  tapa::stream<uint, 2> fifo_PE3_4_compute_config_out("fifo_PE3_4_compute_config_out");
  tapa::stream<uint, 2> fifo_PE3_4_res_config_out("fifo_PE3_4_res_config_out");
  tapa::stream<uint, 2> fifo_PE3_5_op0_config_out("fifo_PE3_5_op0_config_out");
  tapa::stream<uint, 2> fifo_PE3_5_op1_config_out("fifo_PE3_5_op1_config_out");
  tapa::stream<uint, 2> fifo_PE3_5_compute_config_out("fifo_PE3_5_compute_config_out");
  tapa::stream<uint, 2> fifo_PE3_5_res_config_out("fifo_PE3_5_res_config_out");
  tapa::stream<uint, 2> fifo_PE3_6_op0_config_out("fifo_PE3_6_op0_config_out");
  tapa::stream<uint, 2> fifo_PE3_6_op1_config_out("fifo_PE3_6_op1_config_out");
  tapa::stream<uint, 2> fifo_PE3_6_compute_config_out("fifo_PE3_6_compute_config_out");
  tapa::stream<uint, 2> fifo_PE3_6_res_config_out("fifo_PE3_6_res_config_out");
  tapa::stream<uint, 2> fifo_PE3_7_op0_config_out("fifo_PE3_7_op0_config_out");
  tapa::stream<uint, 2> fifo_PE3_7_op1_config_out("fifo_PE3_7_op1_config_out");
  tapa::stream<uint, 2> fifo_PE3_7_compute_config_out("fifo_PE3_7_compute_config_out");
  tapa::stream<uint, 2> fifo_PE3_7_res_config_out("fifo_PE3_7_res_config_out");
  tapa::stream<uint, 2> fifo_PE3_8_op0_config_out("fifo_PE3_8_op0_config_out");
  tapa::stream<uint, 2> fifo_PE3_8_op1_config_out("fifo_PE3_8_op1_config_out");
  tapa::stream<uint, 2> fifo_PE3_8_compute_config_out("fifo_PE3_8_compute_config_out");
  tapa::stream<uint, 2> fifo_PE3_8_res_config_out("fifo_PE3_8_res_config_out");
  tapa::stream<uint, 2> fifo_PE4_0_op0_config_out("fifo_PE4_0_op0_config_out");
  tapa::stream<uint, 2> fifo_PE4_0_op1_config_out("fifo_PE4_0_op1_config_out");
  tapa::stream<uint, 2> fifo_PE4_0_compute_config_out("fifo_PE4_0_compute_config_out");
  tapa::stream<uint, 2> fifo_PE4_0_res_config_out("fifo_PE4_0_res_config_out");
  tapa::stream<uint, 2> fifo_PE4_1_op0_config_out("fifo_PE4_1_op0_config_out");
  tapa::stream<uint, 2> fifo_PE4_1_op1_config_out("fifo_PE4_1_op1_config_out");
  tapa::stream<uint, 2> fifo_PE4_1_compute_config_out("fifo_PE4_1_compute_config_out");
  tapa::stream<uint, 2> fifo_PE4_1_res_config_out("fifo_PE4_1_res_config_out");
  tapa::stream<uint, 2> fifo_PE4_2_op0_config_out("fifo_PE4_2_op0_config_out");
  tapa::stream<uint, 2> fifo_PE4_2_op1_config_out("fifo_PE4_2_op1_config_out");
  tapa::stream<uint, 2> fifo_PE4_2_compute_config_out("fifo_PE4_2_compute_config_out");
  tapa::stream<uint, 2> fifo_PE4_2_res_config_out("fifo_PE4_2_res_config_out");
  tapa::stream<uint, 2> fifo_PE4_3_op0_config_out("fifo_PE4_3_op0_config_out");
  tapa::stream<uint, 2> fifo_PE4_3_op1_config_out("fifo_PE4_3_op1_config_out");
  tapa::stream<uint, 2> fifo_PE4_3_compute_config_out("fifo_PE4_3_compute_config_out");
  tapa::stream<uint, 2> fifo_PE4_3_res_config_out("fifo_PE4_3_res_config_out");
  tapa::stream<uint, 2> fifo_PE4_4_op0_config_out("fifo_PE4_4_op0_config_out");
  tapa::stream<uint, 2> fifo_PE4_4_op1_config_out("fifo_PE4_4_op1_config_out");
  tapa::stream<uint, 2> fifo_PE4_4_compute_config_out("fifo_PE4_4_compute_config_out");
  tapa::stream<uint, 2> fifo_PE4_4_res_config_out("fifo_PE4_4_res_config_out");
  tapa::stream<uint, 2> fifo_PE4_5_op0_config_out("fifo_PE4_5_op0_config_out");
  tapa::stream<uint, 2> fifo_PE4_5_op1_config_out("fifo_PE4_5_op1_config_out");
  tapa::stream<uint, 2> fifo_PE4_5_compute_config_out("fifo_PE4_5_compute_config_out");
  tapa::stream<uint, 2> fifo_PE4_5_res_config_out("fifo_PE4_5_res_config_out");
  tapa::stream<uint, 2> fifo_PE4_6_op0_config_out("fifo_PE4_6_op0_config_out");
  tapa::stream<uint, 2> fifo_PE4_6_op1_config_out("fifo_PE4_6_op1_config_out");
  tapa::stream<uint, 2> fifo_PE4_6_compute_config_out("fifo_PE4_6_compute_config_out");
  tapa::stream<uint, 2> fifo_PE4_6_res_config_out("fifo_PE4_6_res_config_out");
  tapa::stream<uint, 2> fifo_PE4_7_op0_config_out("fifo_PE4_7_op0_config_out");
  tapa::stream<uint, 2> fifo_PE4_7_op1_config_out("fifo_PE4_7_op1_config_out");
  tapa::stream<uint, 2> fifo_PE4_7_compute_config_out("fifo_PE4_7_compute_config_out");
  tapa::stream<uint, 2> fifo_PE4_7_res_config_out("fifo_PE4_7_res_config_out");
  tapa::stream<uint, 2> fifo_PE4_8_op0_config_out("fifo_PE4_8_op0_config_out");
  tapa::stream<uint, 2> fifo_PE4_8_op1_config_out("fifo_PE4_8_op1_config_out");
  tapa::stream<uint, 2> fifo_PE4_8_compute_config_out("fifo_PE4_8_compute_config_out");
  tapa::stream<uint, 2> fifo_PE4_8_res_config_out("fifo_PE4_8_res_config_out");
  tapa::stream<uint, 2> fifo_PE5_0_op0_config_out("fifo_PE5_0_op0_config_out");
  tapa::stream<uint, 2> fifo_PE5_0_op1_config_out("fifo_PE5_0_op1_config_out");
  tapa::stream<uint, 2> fifo_PE5_0_compute_config_out("fifo_PE5_0_compute_config_out");
  tapa::stream<uint, 2> fifo_PE5_0_res_config_out("fifo_PE5_0_res_config_out");
  tapa::stream<uint, 2> fifo_PE5_1_op0_config_out("fifo_PE5_1_op0_config_out");
  tapa::stream<uint, 2> fifo_PE5_1_op1_config_out("fifo_PE5_1_op1_config_out");
  tapa::stream<uint, 2> fifo_PE5_1_compute_config_out("fifo_PE5_1_compute_config_out");
  tapa::stream<uint, 2> fifo_PE5_1_res_config_out("fifo_PE5_1_res_config_out");
  tapa::stream<uint, 2> fifo_PE5_2_op0_config_out("fifo_PE5_2_op0_config_out");
  tapa::stream<uint, 2> fifo_PE5_2_op1_config_out("fifo_PE5_2_op1_config_out");
  tapa::stream<uint, 2> fifo_PE5_2_compute_config_out("fifo_PE5_2_compute_config_out");
  tapa::stream<uint, 2> fifo_PE5_2_res_config_out("fifo_PE5_2_res_config_out");
  tapa::stream<uint, 2> fifo_PE5_3_op0_config_out("fifo_PE5_3_op0_config_out");
  tapa::stream<uint, 2> fifo_PE5_3_op1_config_out("fifo_PE5_3_op1_config_out");
  tapa::stream<uint, 2> fifo_PE5_3_compute_config_out("fifo_PE5_3_compute_config_out");
  tapa::stream<uint, 2> fifo_PE5_3_res_config_out("fifo_PE5_3_res_config_out");
  tapa::stream<uint, 2> fifo_PE5_4_op0_config_out("fifo_PE5_4_op0_config_out");
  tapa::stream<uint, 2> fifo_PE5_4_op1_config_out("fifo_PE5_4_op1_config_out");
  tapa::stream<uint, 2> fifo_PE5_4_compute_config_out("fifo_PE5_4_compute_config_out");
  tapa::stream<uint, 2> fifo_PE5_4_res_config_out("fifo_PE5_4_res_config_out");
  tapa::stream<uint, 2> fifo_PE5_5_op0_config_out("fifo_PE5_5_op0_config_out");
  tapa::stream<uint, 2> fifo_PE5_5_op1_config_out("fifo_PE5_5_op1_config_out");
  tapa::stream<uint, 2> fifo_PE5_5_compute_config_out("fifo_PE5_5_compute_config_out");
  tapa::stream<uint, 2> fifo_PE5_5_res_config_out("fifo_PE5_5_res_config_out");
  tapa::stream<uint, 2> fifo_PE5_6_op0_config_out("fifo_PE5_6_op0_config_out");
  tapa::stream<uint, 2> fifo_PE5_6_op1_config_out("fifo_PE5_6_op1_config_out");
  tapa::stream<uint, 2> fifo_PE5_6_compute_config_out("fifo_PE5_6_compute_config_out");
  tapa::stream<uint, 2> fifo_PE5_6_res_config_out("fifo_PE5_6_res_config_out");
  tapa::stream<uint, 2> fifo_PE5_7_op0_config_out("fifo_PE5_7_op0_config_out");
  tapa::stream<uint, 2> fifo_PE5_7_op1_config_out("fifo_PE5_7_op1_config_out");
  tapa::stream<uint, 2> fifo_PE5_7_compute_config_out("fifo_PE5_7_compute_config_out");
  tapa::stream<uint, 2> fifo_PE5_7_res_config_out("fifo_PE5_7_res_config_out");
  tapa::stream<uint, 2> fifo_PE5_8_op0_config_out("fifo_PE5_8_op0_config_out");
  tapa::stream<uint, 2> fifo_PE5_8_op1_config_out("fifo_PE5_8_op1_config_out");
  tapa::stream<uint, 2> fifo_PE5_8_compute_config_out("fifo_PE5_8_compute_config_out");
  tapa::stream<uint, 2> fifo_PE5_8_res_config_out("fifo_PE5_8_res_config_out");
  tapa::stream<uint, 2> fifo_PE6_0_op0_config_out("fifo_PE6_0_op0_config_out");
  tapa::stream<uint, 2> fifo_PE6_0_op1_config_out("fifo_PE6_0_op1_config_out");
  tapa::stream<uint, 2> fifo_PE6_0_compute_config_out("fifo_PE6_0_compute_config_out");
  tapa::stream<uint, 2> fifo_PE6_0_res_config_out("fifo_PE6_0_res_config_out");
  tapa::stream<uint, 2> fifo_PE6_1_op0_config_out("fifo_PE6_1_op0_config_out");
  tapa::stream<uint, 2> fifo_PE6_1_op1_config_out("fifo_PE6_1_op1_config_out");
  tapa::stream<uint, 2> fifo_PE6_1_compute_config_out("fifo_PE6_1_compute_config_out");
  tapa::stream<uint, 2> fifo_PE6_1_res_config_out("fifo_PE6_1_res_config_out");
  tapa::stream<uint, 2> fifo_PE6_2_op0_config_out("fifo_PE6_2_op0_config_out");
  tapa::stream<uint, 2> fifo_PE6_2_op1_config_out("fifo_PE6_2_op1_config_out");
  tapa::stream<uint, 2> fifo_PE6_2_compute_config_out("fifo_PE6_2_compute_config_out");
  tapa::stream<uint, 2> fifo_PE6_2_res_config_out("fifo_PE6_2_res_config_out");
  tapa::stream<uint, 2> fifo_PE6_3_op0_config_out("fifo_PE6_3_op0_config_out");
  tapa::stream<uint, 2> fifo_PE6_3_op1_config_out("fifo_PE6_3_op1_config_out");
  tapa::stream<uint, 2> fifo_PE6_3_compute_config_out("fifo_PE6_3_compute_config_out");
  tapa::stream<uint, 2> fifo_PE6_3_res_config_out("fifo_PE6_3_res_config_out");
  tapa::stream<uint, 2> fifo_PE6_4_op0_config_out("fifo_PE6_4_op0_config_out");
  tapa::stream<uint, 2> fifo_PE6_4_op1_config_out("fifo_PE6_4_op1_config_out");
  tapa::stream<uint, 2> fifo_PE6_4_compute_config_out("fifo_PE6_4_compute_config_out");
  tapa::stream<uint, 2> fifo_PE6_4_res_config_out("fifo_PE6_4_res_config_out");
  tapa::stream<uint, 2> fifo_PE6_5_op0_config_out("fifo_PE6_5_op0_config_out");
  tapa::stream<uint, 2> fifo_PE6_5_op1_config_out("fifo_PE6_5_op1_config_out");
  tapa::stream<uint, 2> fifo_PE6_5_compute_config_out("fifo_PE6_5_compute_config_out");
  tapa::stream<uint, 2> fifo_PE6_5_res_config_out("fifo_PE6_5_res_config_out");
  tapa::stream<uint, 2> fifo_PE6_6_op0_config_out("fifo_PE6_6_op0_config_out");
  tapa::stream<uint, 2> fifo_PE6_6_op1_config_out("fifo_PE6_6_op1_config_out");
  tapa::stream<uint, 2> fifo_PE6_6_compute_config_out("fifo_PE6_6_compute_config_out");
  tapa::stream<uint, 2> fifo_PE6_6_res_config_out("fifo_PE6_6_res_config_out");
  tapa::stream<uint, 2> fifo_PE6_7_op0_config_out("fifo_PE6_7_op0_config_out");
  tapa::stream<uint, 2> fifo_PE6_7_op1_config_out("fifo_PE6_7_op1_config_out");
  tapa::stream<uint, 2> fifo_PE6_7_compute_config_out("fifo_PE6_7_compute_config_out");
  tapa::stream<uint, 2> fifo_PE6_7_res_config_out("fifo_PE6_7_res_config_out");
  tapa::stream<uint, 2> fifo_PE6_8_op0_config_out("fifo_PE6_8_op0_config_out");
  tapa::stream<uint, 2> fifo_PE6_8_op1_config_out("fifo_PE6_8_op1_config_out");
  tapa::stream<uint, 2> fifo_PE6_8_compute_config_out("fifo_PE6_8_compute_config_out");
  tapa::stream<uint, 2> fifo_PE6_8_res_config_out("fifo_PE6_8_res_config_out");
  tapa::stream<uint, 2> fifo_PE7_0_op0_config_out("fifo_PE7_0_op0_config_out");
  tapa::stream<uint, 2> fifo_PE7_0_op1_config_out("fifo_PE7_0_op1_config_out");
  tapa::stream<uint, 2> fifo_PE7_0_compute_config_out("fifo_PE7_0_compute_config_out");
  tapa::stream<uint, 2> fifo_PE7_0_res_config_out("fifo_PE7_0_res_config_out");
  tapa::stream<uint, 2> fifo_PE7_1_op0_config_out("fifo_PE7_1_op0_config_out");
  tapa::stream<uint, 2> fifo_PE7_1_op1_config_out("fifo_PE7_1_op1_config_out");
  tapa::stream<uint, 2> fifo_PE7_1_compute_config_out("fifo_PE7_1_compute_config_out");
  tapa::stream<uint, 2> fifo_PE7_1_res_config_out("fifo_PE7_1_res_config_out");
  tapa::stream<uint, 2> fifo_PE7_2_op0_config_out("fifo_PE7_2_op0_config_out");
  tapa::stream<uint, 2> fifo_PE7_2_op1_config_out("fifo_PE7_2_op1_config_out");
  tapa::stream<uint, 2> fifo_PE7_2_compute_config_out("fifo_PE7_2_compute_config_out");
  tapa::stream<uint, 2> fifo_PE7_2_res_config_out("fifo_PE7_2_res_config_out");
  tapa::stream<uint, 2> fifo_PE7_3_op0_config_out("fifo_PE7_3_op0_config_out");
  tapa::stream<uint, 2> fifo_PE7_3_op1_config_out("fifo_PE7_3_op1_config_out");
  tapa::stream<uint, 2> fifo_PE7_3_compute_config_out("fifo_PE7_3_compute_config_out");
  tapa::stream<uint, 2> fifo_PE7_3_res_config_out("fifo_PE7_3_res_config_out");
  tapa::stream<uint, 2> fifo_PE7_4_op0_config_out("fifo_PE7_4_op0_config_out");
  tapa::stream<uint, 2> fifo_PE7_4_op1_config_out("fifo_PE7_4_op1_config_out");
  tapa::stream<uint, 2> fifo_PE7_4_compute_config_out("fifo_PE7_4_compute_config_out");
  tapa::stream<uint, 2> fifo_PE7_4_res_config_out("fifo_PE7_4_res_config_out");
  tapa::stream<uint, 2> fifo_PE7_5_op0_config_out("fifo_PE7_5_op0_config_out");
  tapa::stream<uint, 2> fifo_PE7_5_op1_config_out("fifo_PE7_5_op1_config_out");
  tapa::stream<uint, 2> fifo_PE7_5_compute_config_out("fifo_PE7_5_compute_config_out");
  tapa::stream<uint, 2> fifo_PE7_5_res_config_out("fifo_PE7_5_res_config_out");
  tapa::stream<uint, 2> fifo_PE7_6_op0_config_out("fifo_PE7_6_op0_config_out");
  tapa::stream<uint, 2> fifo_PE7_6_op1_config_out("fifo_PE7_6_op1_config_out");
  tapa::stream<uint, 2> fifo_PE7_6_compute_config_out("fifo_PE7_6_compute_config_out");
  tapa::stream<uint, 2> fifo_PE7_6_res_config_out("fifo_PE7_6_res_config_out");
  tapa::stream<uint, 2> fifo_PE7_7_op0_config_out("fifo_PE7_7_op0_config_out");
  tapa::stream<uint, 2> fifo_PE7_7_op1_config_out("fifo_PE7_7_op1_config_out");
  tapa::stream<uint, 2> fifo_PE7_7_compute_config_out("fifo_PE7_7_compute_config_out");
  tapa::stream<uint, 2> fifo_PE7_7_res_config_out("fifo_PE7_7_res_config_out");
  tapa::stream<uint, 2> fifo_PE7_8_op0_config_out("fifo_PE7_8_op0_config_out");
  tapa::stream<uint, 2> fifo_PE7_8_op1_config_out("fifo_PE7_8_op1_config_out");
  tapa::stream<uint, 2> fifo_PE7_8_compute_config_out("fifo_PE7_8_compute_config_out");
  tapa::stream<uint, 2> fifo_PE7_8_res_config_out("fifo_PE7_8_res_config_out");

  tapa::stream<U1_Data0PEChannelType, 2> PE0_0_fifo0_local("PE0_0_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE0_0_fifo1_local("PE0_0_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE0_0_fifo2_local("PE0_0_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE0_1_fifo0_local("PE0_1_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE0_1_fifo1_local("PE0_1_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE0_1_fifo2_local("PE0_1_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE0_2_fifo0_local("PE0_2_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE0_2_fifo1_local("PE0_2_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE0_2_fifo2_local("PE0_2_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE0_3_fifo0_local("PE0_3_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE0_3_fifo1_local("PE0_3_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE0_3_fifo2_local("PE0_3_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE0_4_fifo0_local("PE0_4_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE0_4_fifo1_local("PE0_4_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE0_4_fifo2_local("PE0_4_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE0_5_fifo0_local("PE0_5_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE0_5_fifo1_local("PE0_5_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE0_5_fifo2_local("PE0_5_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE0_6_fifo0_local("PE0_6_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE0_6_fifo1_local("PE0_6_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE0_6_fifo2_local("PE0_6_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE0_7_fifo0_local("PE0_7_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE0_7_fifo1_local("PE0_7_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE0_7_fifo2_local("PE0_7_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE0_8_fifo0_local("PE0_8_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE0_8_fifo1_local("PE0_8_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE0_8_fifo2_local("PE0_8_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE1_0_fifo0_local("PE1_0_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE1_0_fifo1_local("PE1_0_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE1_0_fifo2_local("PE1_0_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE1_1_fifo0_local("PE1_1_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE1_1_fifo1_local("PE1_1_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE1_1_fifo2_local("PE1_1_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE1_2_fifo0_local("PE1_2_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE1_2_fifo1_local("PE1_2_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE1_2_fifo2_local("PE1_2_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE1_3_fifo0_local("PE1_3_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE1_3_fifo1_local("PE1_3_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE1_3_fifo2_local("PE1_3_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE1_4_fifo0_local("PE1_4_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE1_4_fifo1_local("PE1_4_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE1_4_fifo2_local("PE1_4_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE1_5_fifo0_local("PE1_5_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE1_5_fifo1_local("PE1_5_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE1_5_fifo2_local("PE1_5_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE1_6_fifo0_local("PE1_6_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE1_6_fifo1_local("PE1_6_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE1_6_fifo2_local("PE1_6_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE1_7_fifo0_local("PE1_7_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE1_7_fifo1_local("PE1_7_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE1_7_fifo2_local("PE1_7_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE1_8_fifo0_local("PE1_8_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE1_8_fifo1_local("PE1_8_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE1_8_fifo2_local("PE1_8_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE2_0_fifo0_local("PE2_0_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE2_0_fifo1_local("PE2_0_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE2_0_fifo2_local("PE2_0_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE2_1_fifo0_local("PE2_1_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE2_1_fifo1_local("PE2_1_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE2_1_fifo2_local("PE2_1_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE2_2_fifo0_local("PE2_2_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE2_2_fifo1_local("PE2_2_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE2_2_fifo2_local("PE2_2_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE2_3_fifo0_local("PE2_3_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE2_3_fifo1_local("PE2_3_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE2_3_fifo2_local("PE2_3_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE2_4_fifo0_local("PE2_4_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE2_4_fifo1_local("PE2_4_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE2_4_fifo2_local("PE2_4_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE2_5_fifo0_local("PE2_5_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE2_5_fifo1_local("PE2_5_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE2_5_fifo2_local("PE2_5_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE2_6_fifo0_local("PE2_6_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE2_6_fifo1_local("PE2_6_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE2_6_fifo2_local("PE2_6_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE2_7_fifo0_local("PE2_7_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE2_7_fifo1_local("PE2_7_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE2_7_fifo2_local("PE2_7_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE2_8_fifo0_local("PE2_8_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE2_8_fifo1_local("PE2_8_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE2_8_fifo2_local("PE2_8_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE3_0_fifo0_local("PE3_0_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE3_0_fifo1_local("PE3_0_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE3_0_fifo2_local("PE3_0_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE3_1_fifo0_local("PE3_1_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE3_1_fifo1_local("PE3_1_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE3_1_fifo2_local("PE3_1_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE3_2_fifo0_local("PE3_2_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE3_2_fifo1_local("PE3_2_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE3_2_fifo2_local("PE3_2_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE3_3_fifo0_local("PE3_3_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE3_3_fifo1_local("PE3_3_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE3_3_fifo2_local("PE3_3_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE3_4_fifo0_local("PE3_4_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE3_4_fifo1_local("PE3_4_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE3_4_fifo2_local("PE3_4_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE3_5_fifo0_local("PE3_5_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE3_5_fifo1_local("PE3_5_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE3_5_fifo2_local("PE3_5_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE3_6_fifo0_local("PE3_6_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE3_6_fifo1_local("PE3_6_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE3_6_fifo2_local("PE3_6_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE3_7_fifo0_local("PE3_7_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE3_7_fifo1_local("PE3_7_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE3_7_fifo2_local("PE3_7_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE3_8_fifo0_local("PE3_8_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE3_8_fifo1_local("PE3_8_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE3_8_fifo2_local("PE3_8_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE4_0_fifo0_local("PE4_0_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE4_0_fifo1_local("PE4_0_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE4_0_fifo2_local("PE4_0_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE4_1_fifo0_local("PE4_1_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE4_1_fifo1_local("PE4_1_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE4_1_fifo2_local("PE4_1_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE4_2_fifo0_local("PE4_2_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE4_2_fifo1_local("PE4_2_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE4_2_fifo2_local("PE4_2_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE4_3_fifo0_local("PE4_3_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE4_3_fifo1_local("PE4_3_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE4_3_fifo2_local("PE4_3_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE4_4_fifo0_local("PE4_4_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE4_4_fifo1_local("PE4_4_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE4_4_fifo2_local("PE4_4_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE4_5_fifo0_local("PE4_5_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE4_5_fifo1_local("PE4_5_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE4_5_fifo2_local("PE4_5_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE4_6_fifo0_local("PE4_6_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE4_6_fifo1_local("PE4_6_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE4_6_fifo2_local("PE4_6_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE4_7_fifo0_local("PE4_7_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE4_7_fifo1_local("PE4_7_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE4_7_fifo2_local("PE4_7_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE4_8_fifo0_local("PE4_8_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE4_8_fifo1_local("PE4_8_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE4_8_fifo2_local("PE4_8_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE5_0_fifo0_local("PE5_0_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE5_0_fifo1_local("PE5_0_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE5_0_fifo2_local("PE5_0_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE5_1_fifo0_local("PE5_1_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE5_1_fifo1_local("PE5_1_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE5_1_fifo2_local("PE5_1_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE5_2_fifo0_local("PE5_2_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE5_2_fifo1_local("PE5_2_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE5_2_fifo2_local("PE5_2_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE5_3_fifo0_local("PE5_3_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE5_3_fifo1_local("PE5_3_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE5_3_fifo2_local("PE5_3_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE5_4_fifo0_local("PE5_4_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE5_4_fifo1_local("PE5_4_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE5_4_fifo2_local("PE5_4_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE5_5_fifo0_local("PE5_5_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE5_5_fifo1_local("PE5_5_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE5_5_fifo2_local("PE5_5_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE5_6_fifo0_local("PE5_6_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE5_6_fifo1_local("PE5_6_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE5_6_fifo2_local("PE5_6_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE5_7_fifo0_local("PE5_7_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE5_7_fifo1_local("PE5_7_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE5_7_fifo2_local("PE5_7_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE5_8_fifo0_local("PE5_8_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE5_8_fifo1_local("PE5_8_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE5_8_fifo2_local("PE5_8_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE6_0_fifo0_local("PE6_0_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE6_0_fifo1_local("PE6_0_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE6_0_fifo2_local("PE6_0_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE6_1_fifo0_local("PE6_1_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE6_1_fifo1_local("PE6_1_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE6_1_fifo2_local("PE6_1_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE6_2_fifo0_local("PE6_2_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE6_2_fifo1_local("PE6_2_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE6_2_fifo2_local("PE6_2_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE6_3_fifo0_local("PE6_3_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE6_3_fifo1_local("PE6_3_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE6_3_fifo2_local("PE6_3_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE6_4_fifo0_local("PE6_4_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE6_4_fifo1_local("PE6_4_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE6_4_fifo2_local("PE6_4_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE6_5_fifo0_local("PE6_5_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE6_5_fifo1_local("PE6_5_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE6_5_fifo2_local("PE6_5_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE6_6_fifo0_local("PE6_6_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE6_6_fifo1_local("PE6_6_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE6_6_fifo2_local("PE6_6_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE6_7_fifo0_local("PE6_7_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE6_7_fifo1_local("PE6_7_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE6_7_fifo2_local("PE6_7_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE6_8_fifo0_local("PE6_8_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE6_8_fifo1_local("PE6_8_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE6_8_fifo2_local("PE6_8_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE7_0_fifo0_local("PE7_0_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE7_0_fifo1_local("PE7_0_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE7_0_fifo2_local("PE7_0_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE7_1_fifo0_local("PE7_1_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE7_1_fifo1_local("PE7_1_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE7_1_fifo2_local("PE7_1_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE7_2_fifo0_local("PE7_2_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE7_2_fifo1_local("PE7_2_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE7_2_fifo2_local("PE7_2_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE7_3_fifo0_local("PE7_3_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE7_3_fifo1_local("PE7_3_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE7_3_fifo2_local("PE7_3_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE7_4_fifo0_local("PE7_4_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE7_4_fifo1_local("PE7_4_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE7_4_fifo2_local("PE7_4_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE7_5_fifo0_local("PE7_5_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE7_5_fifo1_local("PE7_5_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE7_5_fifo2_local("PE7_5_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE7_6_fifo0_local("PE7_6_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE7_6_fifo1_local("PE7_6_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE7_6_fifo2_local("PE7_6_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE7_7_fifo0_local("PE7_7_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE7_7_fifo1_local("PE7_7_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE7_7_fifo2_local("PE7_7_fifo2_local");
  tapa::stream<U1_Data0PEChannelType, 2> PE7_8_fifo0_local("PE7_8_fifo0_local");
  tapa::stream<U1_Data1PEChannelType, 2> PE7_8_fifo1_local("PE7_8_fifo1_local");
  tapa::stream<U1_Data2PEChannelType, 2> PE7_8_fifo2_local("PE7_8_fifo2_local");

  tapa::stream<CinLoadData0Type, 128> fifo_data_bypass("fifo_data_bypass");
  tapa::stream<uint, 2> fifo_config_bypass("fifo_config_bypass");
  //data fifos
  tapa::stream<CinLoadData0Type, 128> cin_load_to_SA_0("cin_load_to_SA_0");
  tapa::stream<CinLoadData0Type, 128> weight_load_to_SA_0("weight_load_to_SA_0");
  tapa::stream<CinLoadData0Type, 128> bias_load_to_act_and_bn_0("bias_load_to_act_and_bn_0");
  tapa::stream<CinLoadData0Type, 128> bias_load_to_act_and_bn_1("bias_load_to_act_and_bn_1");
  tapa::stream<CinLoadData0Type, 128> cin_load_prev_to_pool_0("cin_load_prev_to_pool_0");
  tapa::stream<CinLoadData0Type, 128> SA_to_upsample_0("SA_to_upsample_0");
  tapa::stream<CinLoadData0Type, 128> pool_to_concat_0("pool_to_concat_0");
  tapa::stream<CinLoadData0Type, 128> upsample_to_concat_0("upsample_to_concat_0");
  tapa::stream<CinLoadData0Type, 128> concat_to_add_0("concat_to_add_0");
  tapa::stream<CinLoadData0Type, 128> concat_to_add_1("concat_to_add_1");
  tapa::stream<CinLoadData0Type, 128> add_to_act_and_bn_0("add_to_act_and_bn_0");
  tapa::stream<CinLoadData0Type, 128> act_and_bn_to_cout_write_0("act_and_bn_to_cout_write_0");
  //instruction fifos
  tapa::stream<ConfigInst,16> config_cin_load_to_weight_load("config_cin_load_to_weight_load");
  tapa::stream<ConfigInst,16> config_weight_load_to_bias_load("config_weight_load_to_bias_load");
  tapa::stream<ConfigInst,16> config_bias_load_to_cin_load_prev("config_bias_load_to_cin_load_prev");
  tapa::stream<ConfigInst,16> config_cin_load_prev_to_SA("config_cin_load_prev_to_SA");
  tapa::stream<ConfigInst,16> config_SA_to_pool("config_SA_to_pool");
  tapa::stream<ConfigInst,16> config_pool_to_upsample("config_pool_to_upsample");
  tapa::stream<ConfigInst,16> config_upsample_to_concat("config_upsample_to_concat");
  tapa::stream<ConfigInst,16> config_concat_to_add("config_concat_to_add");
  tapa::stream<ConfigInst,16> config_add_to_act_and_bn("config_add_to_act_and_bn");
  tapa::stream<ConfigInst,16> config_act_and_bn_to_cout_write("config_act_and_bn_to_cout_write");
  //synchronization fifos
  tapa::stream<int> cin_to_cout_sync("cin_to_cout_sync");
  tapa::stream<int> cout_to_cin_sync("cout_to_cin_sync");
  tapa::task()
  .invoke(cin_load, 
  		start_inst, end_inst,
  		dram_b0, 
  		layer_config,
  		cin_load_to_SA_0, 
  		config_cin_load_to_weight_load
  , cout_to_cin_sync, cin_to_cout_sync
  )
  .invoke(weight_load, 
  		start_inst, end_inst,
  		dram_weights, 
  		config_cin_load_to_weight_load,
  		weight_load_to_SA_0, 
  		config_weight_load_to_bias_load
  )
  .invoke(bias_load, 
  		start_inst, end_inst,
  		dram_biases, 
  		config_weight_load_to_bias_load,
  		bias_load_to_act_and_bn_0, 
  		bias_load_to_act_and_bn_1, 
  		config_bias_load_to_cin_load_prev
  )
  .invoke(cin_load_prev, 
  		start_inst, end_inst,
  		dram_b0, 
  		config_bias_load_to_cin_load_prev,
  		cin_load_prev_to_pool_0, 
  		config_cin_load_prev_to_SA
  )
  .invoke(U1_DataFeed0Head,
    start_inst, end_inst,
    cin_load_to_SA_0,
    fifo0_transfer0,
    config_cin_load_prev_to_SA,
    config_SA_to_pool,
    fifo_DataFeed0Head_config_out0, fifo_DataFeed0Head_config_out1,
    fifo_data_bypass,
    fifo_config_bypass
  )
  .invoke(U1_DataFeed0Engine0_wrapper,
    fifo0_transfer0,
    fifo0_transfer1,
    fifo0_feed0_0,
    0,
    fifo_DataFeed0Head_config_out0,
    fifo_DataFeed0Engine0_config_out0,
    fifo_DataFeed0Engine0_config_out1
  )
  .invoke(U1_DataFeed0Engine0_wrapper,
    fifo0_transfer1,
    fifo0_transfer2,
    fifo0_feed0_1,
    1,
    fifo_DataFeed0Engine0_config_out0,
    fifo_DataFeed0Engine1_config_out0,
    fifo_DataFeed0Engine1_config_out1
  )
  .invoke(U1_DataFeed0Engine0_wrapper,
    fifo0_transfer2,
    fifo0_transfer3,
    fifo0_feed0_2,
    2,
    fifo_DataFeed0Engine1_config_out0,
    fifo_DataFeed0Engine2_config_out0,
    fifo_DataFeed0Engine2_config_out1
  )
  .invoke(U1_DataFeed0Engine0_wrapper,
    fifo0_transfer3,
    fifo0_transfer4,
    fifo0_feed0_3,
    3,
    fifo_DataFeed0Engine2_config_out0,
    fifo_DataFeed0Engine3_config_out0,
    fifo_DataFeed0Engine3_config_out1
  )
  .invoke(U1_DataFeed0Engine0_wrapper,
    fifo0_transfer4,
    fifo0_transfer5,
    fifo0_feed0_4,
    4,
    fifo_DataFeed0Engine3_config_out0,
    fifo_DataFeed0Engine4_config_out0,
    fifo_DataFeed0Engine4_config_out1
  )
  .invoke(U1_DataFeed0Engine0_wrapper,
    fifo0_transfer5,
    fifo0_transfer6,
    fifo0_feed0_5,
    5,
    fifo_DataFeed0Engine4_config_out0,
    fifo_DataFeed0Engine5_config_out0,
    fifo_DataFeed0Engine5_config_out1
  )
  .invoke(U1_DataFeed0Engine0_wrapper,
    fifo0_transfer6,
    fifo0_transfer7,
    fifo0_feed0_6,
    6,
    fifo_DataFeed0Engine5_config_out0,
    fifo_DataFeed0Engine6_config_out0,
    fifo_DataFeed0Engine6_config_out1
  )
  .invoke(U1_DataFeed0Engine0_wrapper,
    fifo0_transfer7,
    fifo0_transfer8,
    fifo0_feed0_7,
    7,
    fifo_DataFeed0Engine6_config_out0,
    fifo_DataFeed0Engine7_config_out0,
    fifo_DataFeed0Engine7_config_out1
  )
  .invoke(U1_DataFeed0EngineLast,
    fifo0_transfer8,
    fifo0_feed0_8,
    8,
    fifo_DataFeed0Engine7_config_out0,
    fifo_DataFeed0Engine8_config_out1
  )
  .invoke(U1_DataFeed1Head,
    weight_load_to_SA_0,
    fifo1_transfer0,
    fifo_DataFeed0Head_config_out1, fifo_DataFeed1Head_config_out0
  )
  .invoke(U1_DataFeed1Engine0_wrapper,
    fifo1_transfer0,
    fifo1_transfer1,
    fifo1_feed0_0,
    0,
    fifo_DataFeed1Head_config_out0,
    fifo_DataFeed1Engine0_config_out0
  )
  .invoke(U1_DataFeed1Engine0_wrapper,
    fifo1_transfer1,
    fifo1_transfer2,
    fifo1_feed1_0,
    1,
    fifo_DataFeed1Engine0_config_out0,
    fifo_DataFeed1Engine1_config_out0
  )
  .invoke(U1_DataFeed1Engine0_wrapper,
    fifo1_transfer2,
    fifo1_transfer3,
    fifo1_feed2_0,
    2,
    fifo_DataFeed1Engine1_config_out0,
    fifo_DataFeed1Engine2_config_out0
  )
  .invoke(U1_DataFeed1Engine0_wrapper,
    fifo1_transfer3,
    fifo1_transfer4,
    fifo1_feed3_0,
    3,
    fifo_DataFeed1Engine2_config_out0,
    fifo_DataFeed1Engine3_config_out0
  )
  .invoke(U1_DataFeed1Engine0_wrapper,
    fifo1_transfer4,
    fifo1_transfer5,
    fifo1_feed4_0,
    4,
    fifo_DataFeed1Engine3_config_out0,
    fifo_DataFeed1Engine4_config_out0
  )
  .invoke(U1_DataFeed1Engine0_wrapper,
    fifo1_transfer5,
    fifo1_transfer6,
    fifo1_feed5_0,
    5,
    fifo_DataFeed1Engine4_config_out0,
    fifo_DataFeed1Engine5_config_out0
  )
  .invoke(U1_DataFeed1Engine0_wrapper,
    fifo1_transfer6,
    fifo1_transfer7,
    fifo1_feed6_0,
    6,
    fifo_DataFeed1Engine5_config_out0,
    fifo_DataFeed1Engine6_config_out0
  )
  .invoke(U1_DataFeed1EngineLast,
    fifo1_transfer7,
    fifo1_feed7_0,
    7,
    fifo_DataFeed1Engine6_config_out0
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed0_0,
    fifo0_feed1_0,
    PE0_0_fifo0_local,
    fifo_DataFeed0Engine0_config_out1,
    fifo_PE0_0_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed0_0,
    fifo1_feed0_1,
    PE0_0_fifo1_local,
    fifo_PE0_0_op0_config_out,
    fifo_PE0_0_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE0_0_fifo0_local,
    PE0_0_fifo1_local,
    PE0_0_fifo2_local,
    fifo_PE0_0_op1_config_out,
    fifo_PE0_0_compute_config_out
  )
  .invoke(U1_res_transfer_first_wrapper,
    PE0_0_fifo2_local,
    fifo2_collect0_0,
    0,
    0,
    fifo_PE0_0_compute_config_out,
    fifo_PE0_0_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed0_1,
    fifo0_feed1_1,
    PE0_1_fifo0_local,
    fifo_DataFeed0Engine1_config_out1,
    fifo_PE0_1_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed0_1,
    fifo1_feed0_2,
    PE0_1_fifo1_local,
    fifo_PE0_1_op0_config_out,
    fifo_PE0_1_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE0_1_fifo0_local,
    PE0_1_fifo1_local,
    PE0_1_fifo2_local,
    fifo_PE0_1_op1_config_out,
    fifo_PE0_1_compute_config_out
  )
  .invoke(U1_res_transfer_first_wrapper,
    PE0_1_fifo2_local,
    fifo2_collect0_1,
    0,
    1,
    fifo_PE0_1_compute_config_out,
    fifo_PE0_1_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed0_2,
    fifo0_feed1_2,
    PE0_2_fifo0_local,
    fifo_DataFeed0Engine2_config_out1,
    fifo_PE0_2_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed0_2,
    fifo1_feed0_3,
    PE0_2_fifo1_local,
    fifo_PE0_2_op0_config_out,
    fifo_PE0_2_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE0_2_fifo0_local,
    PE0_2_fifo1_local,
    PE0_2_fifo2_local,
    fifo_PE0_2_op1_config_out,
    fifo_PE0_2_compute_config_out
  )
  .invoke(U1_res_transfer_first_wrapper,
    PE0_2_fifo2_local,
    fifo2_collect0_2,
    0,
    2,
    fifo_PE0_2_compute_config_out,
    fifo_PE0_2_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed0_3,
    fifo0_feed1_3,
    PE0_3_fifo0_local,
    fifo_DataFeed0Engine3_config_out1,
    fifo_PE0_3_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed0_3,
    fifo1_feed0_4,
    PE0_3_fifo1_local,
    fifo_PE0_3_op0_config_out,
    fifo_PE0_3_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE0_3_fifo0_local,
    PE0_3_fifo1_local,
    PE0_3_fifo2_local,
    fifo_PE0_3_op1_config_out,
    fifo_PE0_3_compute_config_out
  )
  .invoke(U1_res_transfer_first_wrapper,
    PE0_3_fifo2_local,
    fifo2_collect0_3,
    0,
    3,
    fifo_PE0_3_compute_config_out,
    fifo_PE0_3_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed0_4,
    fifo0_feed1_4,
    PE0_4_fifo0_local,
    fifo_DataFeed0Engine4_config_out1,
    fifo_PE0_4_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed0_4,
    fifo1_feed0_5,
    PE0_4_fifo1_local,
    fifo_PE0_4_op0_config_out,
    fifo_PE0_4_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE0_4_fifo0_local,
    PE0_4_fifo1_local,
    PE0_4_fifo2_local,
    fifo_PE0_4_op1_config_out,
    fifo_PE0_4_compute_config_out
  )
  .invoke(U1_res_transfer_first_wrapper,
    PE0_4_fifo2_local,
    fifo2_collect0_4,
    0,
    4,
    fifo_PE0_4_compute_config_out,
    fifo_PE0_4_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed0_5,
    fifo0_feed1_5,
    PE0_5_fifo0_local,
    fifo_DataFeed0Engine5_config_out1,
    fifo_PE0_5_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed0_5,
    fifo1_feed0_6,
    PE0_5_fifo1_local,
    fifo_PE0_5_op0_config_out,
    fifo_PE0_5_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE0_5_fifo0_local,
    PE0_5_fifo1_local,
    PE0_5_fifo2_local,
    fifo_PE0_5_op1_config_out,
    fifo_PE0_5_compute_config_out
  )
  .invoke(U1_res_transfer_first_wrapper,
    PE0_5_fifo2_local,
    fifo2_collect0_5,
    0,
    5,
    fifo_PE0_5_compute_config_out,
    fifo_PE0_5_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed0_6,
    fifo0_feed1_6,
    PE0_6_fifo0_local,
    fifo_DataFeed0Engine6_config_out1,
    fifo_PE0_6_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed0_6,
    fifo1_feed0_7,
    PE0_6_fifo1_local,
    fifo_PE0_6_op0_config_out,
    fifo_PE0_6_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE0_6_fifo0_local,
    PE0_6_fifo1_local,
    PE0_6_fifo2_local,
    fifo_PE0_6_op1_config_out,
    fifo_PE0_6_compute_config_out
  )
  .invoke(U1_res_transfer_first_wrapper,
    PE0_6_fifo2_local,
    fifo2_collect0_6,
    0,
    6,
    fifo_PE0_6_compute_config_out,
    fifo_PE0_6_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed0_7,
    fifo0_feed1_7,
    PE0_7_fifo0_local,
    fifo_DataFeed0Engine7_config_out1,
    fifo_PE0_7_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed0_7,
    fifo1_feed0_8,
    PE0_7_fifo1_local,
    fifo_PE0_7_op0_config_out,
    fifo_PE0_7_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE0_7_fifo0_local,
    PE0_7_fifo1_local,
    PE0_7_fifo2_local,
    fifo_PE0_7_op1_config_out,
    fifo_PE0_7_compute_config_out
  )
  .invoke(U1_res_transfer_first_wrapper,
    PE0_7_fifo2_local,
    fifo2_collect0_7,
    0,
    7,
    fifo_PE0_7_compute_config_out,
    fifo_PE0_7_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed0_8,
    fifo0_feed1_8,
    PE0_8_fifo0_local,
    fifo_DataFeed0Engine8_config_out1,
    fifo_PE0_8_op0_config_out
  )
  .invoke(U1_op1_transfer_last_wrapper,
    fifo1_feed0_8,
    PE0_8_fifo1_local,
    fifo_PE0_8_op0_config_out,
    fifo_PE0_8_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE0_8_fifo0_local,
    PE0_8_fifo1_local,
    PE0_8_fifo2_local,
    fifo_PE0_8_op1_config_out,
    fifo_PE0_8_compute_config_out
  )
  .invoke(U1_res_transfer_first_wrapper,
    PE0_8_fifo2_local,
    fifo2_collect0_8,
    0,
    8,
    fifo_PE0_8_compute_config_out,
    fifo_PE0_8_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed1_0,
    fifo0_feed2_0,
    PE1_0_fifo0_local,
    fifo_PE0_0_res_config_out,
    fifo_PE1_0_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed1_0,
    fifo1_feed1_1,
    PE1_0_fifo1_local,
    fifo_PE1_0_op0_config_out,
    fifo_PE1_0_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE1_0_fifo0_local,
    PE1_0_fifo1_local,
    PE1_0_fifo2_local,
    fifo_PE1_0_op1_config_out,
    fifo_PE1_0_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE1_0_fifo2_local,
    fifo2_collect0_0,
    fifo2_collect1_0,
    1,
    0,
    fifo_PE1_0_compute_config_out,
    fifo_PE1_0_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed1_1,
    fifo0_feed2_1,
    PE1_1_fifo0_local,
    fifo_PE0_1_res_config_out,
    fifo_PE1_1_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed1_1,
    fifo1_feed1_2,
    PE1_1_fifo1_local,
    fifo_PE1_1_op0_config_out,
    fifo_PE1_1_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE1_1_fifo0_local,
    PE1_1_fifo1_local,
    PE1_1_fifo2_local,
    fifo_PE1_1_op1_config_out,
    fifo_PE1_1_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE1_1_fifo2_local,
    fifo2_collect0_1,
    fifo2_collect1_1,
    1,
    1,
    fifo_PE1_1_compute_config_out,
    fifo_PE1_1_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed1_2,
    fifo0_feed2_2,
    PE1_2_fifo0_local,
    fifo_PE0_2_res_config_out,
    fifo_PE1_2_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed1_2,
    fifo1_feed1_3,
    PE1_2_fifo1_local,
    fifo_PE1_2_op0_config_out,
    fifo_PE1_2_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE1_2_fifo0_local,
    PE1_2_fifo1_local,
    PE1_2_fifo2_local,
    fifo_PE1_2_op1_config_out,
    fifo_PE1_2_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE1_2_fifo2_local,
    fifo2_collect0_2,
    fifo2_collect1_2,
    1,
    2,
    fifo_PE1_2_compute_config_out,
    fifo_PE1_2_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed1_3,
    fifo0_feed2_3,
    PE1_3_fifo0_local,
    fifo_PE0_3_res_config_out,
    fifo_PE1_3_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed1_3,
    fifo1_feed1_4,
    PE1_3_fifo1_local,
    fifo_PE1_3_op0_config_out,
    fifo_PE1_3_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE1_3_fifo0_local,
    PE1_3_fifo1_local,
    PE1_3_fifo2_local,
    fifo_PE1_3_op1_config_out,
    fifo_PE1_3_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE1_3_fifo2_local,
    fifo2_collect0_3,
    fifo2_collect1_3,
    1,
    3,
    fifo_PE1_3_compute_config_out,
    fifo_PE1_3_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed1_4,
    fifo0_feed2_4,
    PE1_4_fifo0_local,
    fifo_PE0_4_res_config_out,
    fifo_PE1_4_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed1_4,
    fifo1_feed1_5,
    PE1_4_fifo1_local,
    fifo_PE1_4_op0_config_out,
    fifo_PE1_4_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE1_4_fifo0_local,
    PE1_4_fifo1_local,
    PE1_4_fifo2_local,
    fifo_PE1_4_op1_config_out,
    fifo_PE1_4_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE1_4_fifo2_local,
    fifo2_collect0_4,
    fifo2_collect1_4,
    1,
    4,
    fifo_PE1_4_compute_config_out,
    fifo_PE1_4_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed1_5,
    fifo0_feed2_5,
    PE1_5_fifo0_local,
    fifo_PE0_5_res_config_out,
    fifo_PE1_5_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed1_5,
    fifo1_feed1_6,
    PE1_5_fifo1_local,
    fifo_PE1_5_op0_config_out,
    fifo_PE1_5_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE1_5_fifo0_local,
    PE1_5_fifo1_local,
    PE1_5_fifo2_local,
    fifo_PE1_5_op1_config_out,
    fifo_PE1_5_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE1_5_fifo2_local,
    fifo2_collect0_5,
    fifo2_collect1_5,
    1,
    5,
    fifo_PE1_5_compute_config_out,
    fifo_PE1_5_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed1_6,
    fifo0_feed2_6,
    PE1_6_fifo0_local,
    fifo_PE0_6_res_config_out,
    fifo_PE1_6_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed1_6,
    fifo1_feed1_7,
    PE1_6_fifo1_local,
    fifo_PE1_6_op0_config_out,
    fifo_PE1_6_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE1_6_fifo0_local,
    PE1_6_fifo1_local,
    PE1_6_fifo2_local,
    fifo_PE1_6_op1_config_out,
    fifo_PE1_6_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE1_6_fifo2_local,
    fifo2_collect0_6,
    fifo2_collect1_6,
    1,
    6,
    fifo_PE1_6_compute_config_out,
    fifo_PE1_6_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed1_7,
    fifo0_feed2_7,
    PE1_7_fifo0_local,
    fifo_PE0_7_res_config_out,
    fifo_PE1_7_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed1_7,
    fifo1_feed1_8,
    PE1_7_fifo1_local,
    fifo_PE1_7_op0_config_out,
    fifo_PE1_7_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE1_7_fifo0_local,
    PE1_7_fifo1_local,
    PE1_7_fifo2_local,
    fifo_PE1_7_op1_config_out,
    fifo_PE1_7_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE1_7_fifo2_local,
    fifo2_collect0_7,
    fifo2_collect1_7,
    1,
    7,
    fifo_PE1_7_compute_config_out,
    fifo_PE1_7_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed1_8,
    fifo0_feed2_8,
    PE1_8_fifo0_local,
    fifo_PE0_8_res_config_out,
    fifo_PE1_8_op0_config_out
  )
  .invoke(U1_op1_transfer_last_wrapper,
    fifo1_feed1_8,
    PE1_8_fifo1_local,
    fifo_PE1_8_op0_config_out,
    fifo_PE1_8_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE1_8_fifo0_local,
    PE1_8_fifo1_local,
    PE1_8_fifo2_local,
    fifo_PE1_8_op1_config_out,
    fifo_PE1_8_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE1_8_fifo2_local,
    fifo2_collect0_8,
    fifo2_collect1_8,
    1,
    8,
    fifo_PE1_8_compute_config_out,
    fifo_PE1_8_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed2_0,
    fifo0_feed3_0,
    PE2_0_fifo0_local,
    fifo_PE1_0_res_config_out,
    fifo_PE2_0_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed2_0,
    fifo1_feed2_1,
    PE2_0_fifo1_local,
    fifo_PE2_0_op0_config_out,
    fifo_PE2_0_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE2_0_fifo0_local,
    PE2_0_fifo1_local,
    PE2_0_fifo2_local,
    fifo_PE2_0_op1_config_out,
    fifo_PE2_0_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE2_0_fifo2_local,
    fifo2_collect1_0,
    fifo2_collect2_0,
    2,
    0,
    fifo_PE2_0_compute_config_out,
    fifo_PE2_0_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed2_1,
    fifo0_feed3_1,
    PE2_1_fifo0_local,
    fifo_PE1_1_res_config_out,
    fifo_PE2_1_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed2_1,
    fifo1_feed2_2,
    PE2_1_fifo1_local,
    fifo_PE2_1_op0_config_out,
    fifo_PE2_1_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE2_1_fifo0_local,
    PE2_1_fifo1_local,
    PE2_1_fifo2_local,
    fifo_PE2_1_op1_config_out,
    fifo_PE2_1_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE2_1_fifo2_local,
    fifo2_collect1_1,
    fifo2_collect2_1,
    2,
    1,
    fifo_PE2_1_compute_config_out,
    fifo_PE2_1_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed2_2,
    fifo0_feed3_2,
    PE2_2_fifo0_local,
    fifo_PE1_2_res_config_out,
    fifo_PE2_2_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed2_2,
    fifo1_feed2_3,
    PE2_2_fifo1_local,
    fifo_PE2_2_op0_config_out,
    fifo_PE2_2_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE2_2_fifo0_local,
    PE2_2_fifo1_local,
    PE2_2_fifo2_local,
    fifo_PE2_2_op1_config_out,
    fifo_PE2_2_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE2_2_fifo2_local,
    fifo2_collect1_2,
    fifo2_collect2_2,
    2,
    2,
    fifo_PE2_2_compute_config_out,
    fifo_PE2_2_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed2_3,
    fifo0_feed3_3,
    PE2_3_fifo0_local,
    fifo_PE1_3_res_config_out,
    fifo_PE2_3_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed2_3,
    fifo1_feed2_4,
    PE2_3_fifo1_local,
    fifo_PE2_3_op0_config_out,
    fifo_PE2_3_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE2_3_fifo0_local,
    PE2_3_fifo1_local,
    PE2_3_fifo2_local,
    fifo_PE2_3_op1_config_out,
    fifo_PE2_3_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE2_3_fifo2_local,
    fifo2_collect1_3,
    fifo2_collect2_3,
    2,
    3,
    fifo_PE2_3_compute_config_out,
    fifo_PE2_3_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed2_4,
    fifo0_feed3_4,
    PE2_4_fifo0_local,
    fifo_PE1_4_res_config_out,
    fifo_PE2_4_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed2_4,
    fifo1_feed2_5,
    PE2_4_fifo1_local,
    fifo_PE2_4_op0_config_out,
    fifo_PE2_4_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE2_4_fifo0_local,
    PE2_4_fifo1_local,
    PE2_4_fifo2_local,
    fifo_PE2_4_op1_config_out,
    fifo_PE2_4_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE2_4_fifo2_local,
    fifo2_collect1_4,
    fifo2_collect2_4,
    2,
    4,
    fifo_PE2_4_compute_config_out,
    fifo_PE2_4_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed2_5,
    fifo0_feed3_5,
    PE2_5_fifo0_local,
    fifo_PE1_5_res_config_out,
    fifo_PE2_5_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed2_5,
    fifo1_feed2_6,
    PE2_5_fifo1_local,
    fifo_PE2_5_op0_config_out,
    fifo_PE2_5_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE2_5_fifo0_local,
    PE2_5_fifo1_local,
    PE2_5_fifo2_local,
    fifo_PE2_5_op1_config_out,
    fifo_PE2_5_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE2_5_fifo2_local,
    fifo2_collect1_5,
    fifo2_collect2_5,
    2,
    5,
    fifo_PE2_5_compute_config_out,
    fifo_PE2_5_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed2_6,
    fifo0_feed3_6,
    PE2_6_fifo0_local,
    fifo_PE1_6_res_config_out,
    fifo_PE2_6_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed2_6,
    fifo1_feed2_7,
    PE2_6_fifo1_local,
    fifo_PE2_6_op0_config_out,
    fifo_PE2_6_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE2_6_fifo0_local,
    PE2_6_fifo1_local,
    PE2_6_fifo2_local,
    fifo_PE2_6_op1_config_out,
    fifo_PE2_6_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE2_6_fifo2_local,
    fifo2_collect1_6,
    fifo2_collect2_6,
    2,
    6,
    fifo_PE2_6_compute_config_out,
    fifo_PE2_6_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed2_7,
    fifo0_feed3_7,
    PE2_7_fifo0_local,
    fifo_PE1_7_res_config_out,
    fifo_PE2_7_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed2_7,
    fifo1_feed2_8,
    PE2_7_fifo1_local,
    fifo_PE2_7_op0_config_out,
    fifo_PE2_7_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE2_7_fifo0_local,
    PE2_7_fifo1_local,
    PE2_7_fifo2_local,
    fifo_PE2_7_op1_config_out,
    fifo_PE2_7_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE2_7_fifo2_local,
    fifo2_collect1_7,
    fifo2_collect2_7,
    2,
    7,
    fifo_PE2_7_compute_config_out,
    fifo_PE2_7_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed2_8,
    fifo0_feed3_8,
    PE2_8_fifo0_local,
    fifo_PE1_8_res_config_out,
    fifo_PE2_8_op0_config_out
  )
  .invoke(U1_op1_transfer_last_wrapper,
    fifo1_feed2_8,
    PE2_8_fifo1_local,
    fifo_PE2_8_op0_config_out,
    fifo_PE2_8_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE2_8_fifo0_local,
    PE2_8_fifo1_local,
    PE2_8_fifo2_local,
    fifo_PE2_8_op1_config_out,
    fifo_PE2_8_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE2_8_fifo2_local,
    fifo2_collect1_8,
    fifo2_collect2_8,
    2,
    8,
    fifo_PE2_8_compute_config_out,
    fifo_PE2_8_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed3_0,
    fifo0_feed4_0,
    PE3_0_fifo0_local,
    fifo_PE2_0_res_config_out,
    fifo_PE3_0_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed3_0,
    fifo1_feed3_1,
    PE3_0_fifo1_local,
    fifo_PE3_0_op0_config_out,
    fifo_PE3_0_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE3_0_fifo0_local,
    PE3_0_fifo1_local,
    PE3_0_fifo2_local,
    fifo_PE3_0_op1_config_out,
    fifo_PE3_0_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE3_0_fifo2_local,
    fifo2_collect2_0,
    fifo2_collect3_0,
    3,
    0,
    fifo_PE3_0_compute_config_out,
    fifo_PE3_0_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed3_1,
    fifo0_feed4_1,
    PE3_1_fifo0_local,
    fifo_PE2_1_res_config_out,
    fifo_PE3_1_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed3_1,
    fifo1_feed3_2,
    PE3_1_fifo1_local,
    fifo_PE3_1_op0_config_out,
    fifo_PE3_1_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE3_1_fifo0_local,
    PE3_1_fifo1_local,
    PE3_1_fifo2_local,
    fifo_PE3_1_op1_config_out,
    fifo_PE3_1_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE3_1_fifo2_local,
    fifo2_collect2_1,
    fifo2_collect3_1,
    3,
    1,
    fifo_PE3_1_compute_config_out,
    fifo_PE3_1_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed3_2,
    fifo0_feed4_2,
    PE3_2_fifo0_local,
    fifo_PE2_2_res_config_out,
    fifo_PE3_2_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed3_2,
    fifo1_feed3_3,
    PE3_2_fifo1_local,
    fifo_PE3_2_op0_config_out,
    fifo_PE3_2_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE3_2_fifo0_local,
    PE3_2_fifo1_local,
    PE3_2_fifo2_local,
    fifo_PE3_2_op1_config_out,
    fifo_PE3_2_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE3_2_fifo2_local,
    fifo2_collect2_2,
    fifo2_collect3_2,
    3,
    2,
    fifo_PE3_2_compute_config_out,
    fifo_PE3_2_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed3_3,
    fifo0_feed4_3,
    PE3_3_fifo0_local,
    fifo_PE2_3_res_config_out,
    fifo_PE3_3_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed3_3,
    fifo1_feed3_4,
    PE3_3_fifo1_local,
    fifo_PE3_3_op0_config_out,
    fifo_PE3_3_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE3_3_fifo0_local,
    PE3_3_fifo1_local,
    PE3_3_fifo2_local,
    fifo_PE3_3_op1_config_out,
    fifo_PE3_3_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE3_3_fifo2_local,
    fifo2_collect2_3,
    fifo2_collect3_3,
    3,
    3,
    fifo_PE3_3_compute_config_out,
    fifo_PE3_3_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed3_4,
    fifo0_feed4_4,
    PE3_4_fifo0_local,
    fifo_PE2_4_res_config_out,
    fifo_PE3_4_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed3_4,
    fifo1_feed3_5,
    PE3_4_fifo1_local,
    fifo_PE3_4_op0_config_out,
    fifo_PE3_4_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE3_4_fifo0_local,
    PE3_4_fifo1_local,
    PE3_4_fifo2_local,
    fifo_PE3_4_op1_config_out,
    fifo_PE3_4_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE3_4_fifo2_local,
    fifo2_collect2_4,
    fifo2_collect3_4,
    3,
    4,
    fifo_PE3_4_compute_config_out,
    fifo_PE3_4_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed3_5,
    fifo0_feed4_5,
    PE3_5_fifo0_local,
    fifo_PE2_5_res_config_out,
    fifo_PE3_5_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed3_5,
    fifo1_feed3_6,
    PE3_5_fifo1_local,
    fifo_PE3_5_op0_config_out,
    fifo_PE3_5_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE3_5_fifo0_local,
    PE3_5_fifo1_local,
    PE3_5_fifo2_local,
    fifo_PE3_5_op1_config_out,
    fifo_PE3_5_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE3_5_fifo2_local,
    fifo2_collect2_5,
    fifo2_collect3_5,
    3,
    5,
    fifo_PE3_5_compute_config_out,
    fifo_PE3_5_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed3_6,
    fifo0_feed4_6,
    PE3_6_fifo0_local,
    fifo_PE2_6_res_config_out,
    fifo_PE3_6_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed3_6,
    fifo1_feed3_7,
    PE3_6_fifo1_local,
    fifo_PE3_6_op0_config_out,
    fifo_PE3_6_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE3_6_fifo0_local,
    PE3_6_fifo1_local,
    PE3_6_fifo2_local,
    fifo_PE3_6_op1_config_out,
    fifo_PE3_6_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE3_6_fifo2_local,
    fifo2_collect2_6,
    fifo2_collect3_6,
    3,
    6,
    fifo_PE3_6_compute_config_out,
    fifo_PE3_6_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed3_7,
    fifo0_feed4_7,
    PE3_7_fifo0_local,
    fifo_PE2_7_res_config_out,
    fifo_PE3_7_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed3_7,
    fifo1_feed3_8,
    PE3_7_fifo1_local,
    fifo_PE3_7_op0_config_out,
    fifo_PE3_7_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE3_7_fifo0_local,
    PE3_7_fifo1_local,
    PE3_7_fifo2_local,
    fifo_PE3_7_op1_config_out,
    fifo_PE3_7_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE3_7_fifo2_local,
    fifo2_collect2_7,
    fifo2_collect3_7,
    3,
    7,
    fifo_PE3_7_compute_config_out,
    fifo_PE3_7_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed3_8,
    fifo0_feed4_8,
    PE3_8_fifo0_local,
    fifo_PE2_8_res_config_out,
    fifo_PE3_8_op0_config_out
  )
  .invoke(U1_op1_transfer_last_wrapper,
    fifo1_feed3_8,
    PE3_8_fifo1_local,
    fifo_PE3_8_op0_config_out,
    fifo_PE3_8_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE3_8_fifo0_local,
    PE3_8_fifo1_local,
    PE3_8_fifo2_local,
    fifo_PE3_8_op1_config_out,
    fifo_PE3_8_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE3_8_fifo2_local,
    fifo2_collect2_8,
    fifo2_collect3_8,
    3,
    8,
    fifo_PE3_8_compute_config_out,
    fifo_PE3_8_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed4_0,
    fifo0_feed5_0,
    PE4_0_fifo0_local,
    fifo_PE3_0_res_config_out,
    fifo_PE4_0_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed4_0,
    fifo1_feed4_1,
    PE4_0_fifo1_local,
    fifo_PE4_0_op0_config_out,
    fifo_PE4_0_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE4_0_fifo0_local,
    PE4_0_fifo1_local,
    PE4_0_fifo2_local,
    fifo_PE4_0_op1_config_out,
    fifo_PE4_0_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE4_0_fifo2_local,
    fifo2_collect3_0,
    fifo2_collect4_0,
    4,
    0,
    fifo_PE4_0_compute_config_out,
    fifo_PE4_0_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed4_1,
    fifo0_feed5_1,
    PE4_1_fifo0_local,
    fifo_PE3_1_res_config_out,
    fifo_PE4_1_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed4_1,
    fifo1_feed4_2,
    PE4_1_fifo1_local,
    fifo_PE4_1_op0_config_out,
    fifo_PE4_1_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE4_1_fifo0_local,
    PE4_1_fifo1_local,
    PE4_1_fifo2_local,
    fifo_PE4_1_op1_config_out,
    fifo_PE4_1_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE4_1_fifo2_local,
    fifo2_collect3_1,
    fifo2_collect4_1,
    4,
    1,
    fifo_PE4_1_compute_config_out,
    fifo_PE4_1_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed4_2,
    fifo0_feed5_2,
    PE4_2_fifo0_local,
    fifo_PE3_2_res_config_out,
    fifo_PE4_2_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed4_2,
    fifo1_feed4_3,
    PE4_2_fifo1_local,
    fifo_PE4_2_op0_config_out,
    fifo_PE4_2_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE4_2_fifo0_local,
    PE4_2_fifo1_local,
    PE4_2_fifo2_local,
    fifo_PE4_2_op1_config_out,
    fifo_PE4_2_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE4_2_fifo2_local,
    fifo2_collect3_2,
    fifo2_collect4_2,
    4,
    2,
    fifo_PE4_2_compute_config_out,
    fifo_PE4_2_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed4_3,
    fifo0_feed5_3,
    PE4_3_fifo0_local,
    fifo_PE3_3_res_config_out,
    fifo_PE4_3_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed4_3,
    fifo1_feed4_4,
    PE4_3_fifo1_local,
    fifo_PE4_3_op0_config_out,
    fifo_PE4_3_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE4_3_fifo0_local,
    PE4_3_fifo1_local,
    PE4_3_fifo2_local,
    fifo_PE4_3_op1_config_out,
    fifo_PE4_3_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE4_3_fifo2_local,
    fifo2_collect3_3,
    fifo2_collect4_3,
    4,
    3,
    fifo_PE4_3_compute_config_out,
    fifo_PE4_3_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed4_4,
    fifo0_feed5_4,
    PE4_4_fifo0_local,
    fifo_PE3_4_res_config_out,
    fifo_PE4_4_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed4_4,
    fifo1_feed4_5,
    PE4_4_fifo1_local,
    fifo_PE4_4_op0_config_out,
    fifo_PE4_4_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE4_4_fifo0_local,
    PE4_4_fifo1_local,
    PE4_4_fifo2_local,
    fifo_PE4_4_op1_config_out,
    fifo_PE4_4_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE4_4_fifo2_local,
    fifo2_collect3_4,
    fifo2_collect4_4,
    4,
    4,
    fifo_PE4_4_compute_config_out,
    fifo_PE4_4_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed4_5,
    fifo0_feed5_5,
    PE4_5_fifo0_local,
    fifo_PE3_5_res_config_out,
    fifo_PE4_5_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed4_5,
    fifo1_feed4_6,
    PE4_5_fifo1_local,
    fifo_PE4_5_op0_config_out,
    fifo_PE4_5_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE4_5_fifo0_local,
    PE4_5_fifo1_local,
    PE4_5_fifo2_local,
    fifo_PE4_5_op1_config_out,
    fifo_PE4_5_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE4_5_fifo2_local,
    fifo2_collect3_5,
    fifo2_collect4_5,
    4,
    5,
    fifo_PE4_5_compute_config_out,
    fifo_PE4_5_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed4_6,
    fifo0_feed5_6,
    PE4_6_fifo0_local,
    fifo_PE3_6_res_config_out,
    fifo_PE4_6_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed4_6,
    fifo1_feed4_7,
    PE4_6_fifo1_local,
    fifo_PE4_6_op0_config_out,
    fifo_PE4_6_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE4_6_fifo0_local,
    PE4_6_fifo1_local,
    PE4_6_fifo2_local,
    fifo_PE4_6_op1_config_out,
    fifo_PE4_6_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE4_6_fifo2_local,
    fifo2_collect3_6,
    fifo2_collect4_6,
    4,
    6,
    fifo_PE4_6_compute_config_out,
    fifo_PE4_6_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed4_7,
    fifo0_feed5_7,
    PE4_7_fifo0_local,
    fifo_PE3_7_res_config_out,
    fifo_PE4_7_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed4_7,
    fifo1_feed4_8,
    PE4_7_fifo1_local,
    fifo_PE4_7_op0_config_out,
    fifo_PE4_7_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE4_7_fifo0_local,
    PE4_7_fifo1_local,
    PE4_7_fifo2_local,
    fifo_PE4_7_op1_config_out,
    fifo_PE4_7_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE4_7_fifo2_local,
    fifo2_collect3_7,
    fifo2_collect4_7,
    4,
    7,
    fifo_PE4_7_compute_config_out,
    fifo_PE4_7_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed4_8,
    fifo0_feed5_8,
    PE4_8_fifo0_local,
    fifo_PE3_8_res_config_out,
    fifo_PE4_8_op0_config_out
  )
  .invoke(U1_op1_transfer_last_wrapper,
    fifo1_feed4_8,
    PE4_8_fifo1_local,
    fifo_PE4_8_op0_config_out,
    fifo_PE4_8_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE4_8_fifo0_local,
    PE4_8_fifo1_local,
    PE4_8_fifo2_local,
    fifo_PE4_8_op1_config_out,
    fifo_PE4_8_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE4_8_fifo2_local,
    fifo2_collect3_8,
    fifo2_collect4_8,
    4,
    8,
    fifo_PE4_8_compute_config_out,
    fifo_PE4_8_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed5_0,
    fifo0_feed6_0,
    PE5_0_fifo0_local,
    fifo_PE4_0_res_config_out,
    fifo_PE5_0_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed5_0,
    fifo1_feed5_1,
    PE5_0_fifo1_local,
    fifo_PE5_0_op0_config_out,
    fifo_PE5_0_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE5_0_fifo0_local,
    PE5_0_fifo1_local,
    PE5_0_fifo2_local,
    fifo_PE5_0_op1_config_out,
    fifo_PE5_0_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE5_0_fifo2_local,
    fifo2_collect4_0,
    fifo2_collect5_0,
    5,
    0,
    fifo_PE5_0_compute_config_out,
    fifo_PE5_0_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed5_1,
    fifo0_feed6_1,
    PE5_1_fifo0_local,
    fifo_PE4_1_res_config_out,
    fifo_PE5_1_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed5_1,
    fifo1_feed5_2,
    PE5_1_fifo1_local,
    fifo_PE5_1_op0_config_out,
    fifo_PE5_1_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE5_1_fifo0_local,
    PE5_1_fifo1_local,
    PE5_1_fifo2_local,
    fifo_PE5_1_op1_config_out,
    fifo_PE5_1_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE5_1_fifo2_local,
    fifo2_collect4_1,
    fifo2_collect5_1,
    5,
    1,
    fifo_PE5_1_compute_config_out,
    fifo_PE5_1_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed5_2,
    fifo0_feed6_2,
    PE5_2_fifo0_local,
    fifo_PE4_2_res_config_out,
    fifo_PE5_2_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed5_2,
    fifo1_feed5_3,
    PE5_2_fifo1_local,
    fifo_PE5_2_op0_config_out,
    fifo_PE5_2_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE5_2_fifo0_local,
    PE5_2_fifo1_local,
    PE5_2_fifo2_local,
    fifo_PE5_2_op1_config_out,
    fifo_PE5_2_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE5_2_fifo2_local,
    fifo2_collect4_2,
    fifo2_collect5_2,
    5,
    2,
    fifo_PE5_2_compute_config_out,
    fifo_PE5_2_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed5_3,
    fifo0_feed6_3,
    PE5_3_fifo0_local,
    fifo_PE4_3_res_config_out,
    fifo_PE5_3_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed5_3,
    fifo1_feed5_4,
    PE5_3_fifo1_local,
    fifo_PE5_3_op0_config_out,
    fifo_PE5_3_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE5_3_fifo0_local,
    PE5_3_fifo1_local,
    PE5_3_fifo2_local,
    fifo_PE5_3_op1_config_out,
    fifo_PE5_3_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE5_3_fifo2_local,
    fifo2_collect4_3,
    fifo2_collect5_3,
    5,
    3,
    fifo_PE5_3_compute_config_out,
    fifo_PE5_3_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed5_4,
    fifo0_feed6_4,
    PE5_4_fifo0_local,
    fifo_PE4_4_res_config_out,
    fifo_PE5_4_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed5_4,
    fifo1_feed5_5,
    PE5_4_fifo1_local,
    fifo_PE5_4_op0_config_out,
    fifo_PE5_4_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE5_4_fifo0_local,
    PE5_4_fifo1_local,
    PE5_4_fifo2_local,
    fifo_PE5_4_op1_config_out,
    fifo_PE5_4_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE5_4_fifo2_local,
    fifo2_collect4_4,
    fifo2_collect5_4,
    5,
    4,
    fifo_PE5_4_compute_config_out,
    fifo_PE5_4_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed5_5,
    fifo0_feed6_5,
    PE5_5_fifo0_local,
    fifo_PE4_5_res_config_out,
    fifo_PE5_5_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed5_5,
    fifo1_feed5_6,
    PE5_5_fifo1_local,
    fifo_PE5_5_op0_config_out,
    fifo_PE5_5_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE5_5_fifo0_local,
    PE5_5_fifo1_local,
    PE5_5_fifo2_local,
    fifo_PE5_5_op1_config_out,
    fifo_PE5_5_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE5_5_fifo2_local,
    fifo2_collect4_5,
    fifo2_collect5_5,
    5,
    5,
    fifo_PE5_5_compute_config_out,
    fifo_PE5_5_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed5_6,
    fifo0_feed6_6,
    PE5_6_fifo0_local,
    fifo_PE4_6_res_config_out,
    fifo_PE5_6_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed5_6,
    fifo1_feed5_7,
    PE5_6_fifo1_local,
    fifo_PE5_6_op0_config_out,
    fifo_PE5_6_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE5_6_fifo0_local,
    PE5_6_fifo1_local,
    PE5_6_fifo2_local,
    fifo_PE5_6_op1_config_out,
    fifo_PE5_6_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE5_6_fifo2_local,
    fifo2_collect4_6,
    fifo2_collect5_6,
    5,
    6,
    fifo_PE5_6_compute_config_out,
    fifo_PE5_6_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed5_7,
    fifo0_feed6_7,
    PE5_7_fifo0_local,
    fifo_PE4_7_res_config_out,
    fifo_PE5_7_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed5_7,
    fifo1_feed5_8,
    PE5_7_fifo1_local,
    fifo_PE5_7_op0_config_out,
    fifo_PE5_7_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE5_7_fifo0_local,
    PE5_7_fifo1_local,
    PE5_7_fifo2_local,
    fifo_PE5_7_op1_config_out,
    fifo_PE5_7_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE5_7_fifo2_local,
    fifo2_collect4_7,
    fifo2_collect5_7,
    5,
    7,
    fifo_PE5_7_compute_config_out,
    fifo_PE5_7_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed5_8,
    fifo0_feed6_8,
    PE5_8_fifo0_local,
    fifo_PE4_8_res_config_out,
    fifo_PE5_8_op0_config_out
  )
  .invoke(U1_op1_transfer_last_wrapper,
    fifo1_feed5_8,
    PE5_8_fifo1_local,
    fifo_PE5_8_op0_config_out,
    fifo_PE5_8_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE5_8_fifo0_local,
    PE5_8_fifo1_local,
    PE5_8_fifo2_local,
    fifo_PE5_8_op1_config_out,
    fifo_PE5_8_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE5_8_fifo2_local,
    fifo2_collect4_8,
    fifo2_collect5_8,
    5,
    8,
    fifo_PE5_8_compute_config_out,
    fifo_PE5_8_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed6_0,
    fifo0_feed7_0,
    PE6_0_fifo0_local,
    fifo_PE5_0_res_config_out,
    fifo_PE6_0_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed6_0,
    fifo1_feed6_1,
    PE6_0_fifo1_local,
    fifo_PE6_0_op0_config_out,
    fifo_PE6_0_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE6_0_fifo0_local,
    PE6_0_fifo1_local,
    PE6_0_fifo2_local,
    fifo_PE6_0_op1_config_out,
    fifo_PE6_0_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE6_0_fifo2_local,
    fifo2_collect5_0,
    fifo2_collect6_0,
    6,
    0,
    fifo_PE6_0_compute_config_out,
    fifo_PE6_0_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed6_1,
    fifo0_feed7_1,
    PE6_1_fifo0_local,
    fifo_PE5_1_res_config_out,
    fifo_PE6_1_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed6_1,
    fifo1_feed6_2,
    PE6_1_fifo1_local,
    fifo_PE6_1_op0_config_out,
    fifo_PE6_1_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE6_1_fifo0_local,
    PE6_1_fifo1_local,
    PE6_1_fifo2_local,
    fifo_PE6_1_op1_config_out,
    fifo_PE6_1_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE6_1_fifo2_local,
    fifo2_collect5_1,
    fifo2_collect6_1,
    6,
    1,
    fifo_PE6_1_compute_config_out,
    fifo_PE6_1_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed6_2,
    fifo0_feed7_2,
    PE6_2_fifo0_local,
    fifo_PE5_2_res_config_out,
    fifo_PE6_2_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed6_2,
    fifo1_feed6_3,
    PE6_2_fifo1_local,
    fifo_PE6_2_op0_config_out,
    fifo_PE6_2_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE6_2_fifo0_local,
    PE6_2_fifo1_local,
    PE6_2_fifo2_local,
    fifo_PE6_2_op1_config_out,
    fifo_PE6_2_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE6_2_fifo2_local,
    fifo2_collect5_2,
    fifo2_collect6_2,
    6,
    2,
    fifo_PE6_2_compute_config_out,
    fifo_PE6_2_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed6_3,
    fifo0_feed7_3,
    PE6_3_fifo0_local,
    fifo_PE5_3_res_config_out,
    fifo_PE6_3_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed6_3,
    fifo1_feed6_4,
    PE6_3_fifo1_local,
    fifo_PE6_3_op0_config_out,
    fifo_PE6_3_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE6_3_fifo0_local,
    PE6_3_fifo1_local,
    PE6_3_fifo2_local,
    fifo_PE6_3_op1_config_out,
    fifo_PE6_3_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE6_3_fifo2_local,
    fifo2_collect5_3,
    fifo2_collect6_3,
    6,
    3,
    fifo_PE6_3_compute_config_out,
    fifo_PE6_3_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed6_4,
    fifo0_feed7_4,
    PE6_4_fifo0_local,
    fifo_PE5_4_res_config_out,
    fifo_PE6_4_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed6_4,
    fifo1_feed6_5,
    PE6_4_fifo1_local,
    fifo_PE6_4_op0_config_out,
    fifo_PE6_4_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE6_4_fifo0_local,
    PE6_4_fifo1_local,
    PE6_4_fifo2_local,
    fifo_PE6_4_op1_config_out,
    fifo_PE6_4_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE6_4_fifo2_local,
    fifo2_collect5_4,
    fifo2_collect6_4,
    6,
    4,
    fifo_PE6_4_compute_config_out,
    fifo_PE6_4_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed6_5,
    fifo0_feed7_5,
    PE6_5_fifo0_local,
    fifo_PE5_5_res_config_out,
    fifo_PE6_5_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed6_5,
    fifo1_feed6_6,
    PE6_5_fifo1_local,
    fifo_PE6_5_op0_config_out,
    fifo_PE6_5_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE6_5_fifo0_local,
    PE6_5_fifo1_local,
    PE6_5_fifo2_local,
    fifo_PE6_5_op1_config_out,
    fifo_PE6_5_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE6_5_fifo2_local,
    fifo2_collect5_5,
    fifo2_collect6_5,
    6,
    5,
    fifo_PE6_5_compute_config_out,
    fifo_PE6_5_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed6_6,
    fifo0_feed7_6,
    PE6_6_fifo0_local,
    fifo_PE5_6_res_config_out,
    fifo_PE6_6_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed6_6,
    fifo1_feed6_7,
    PE6_6_fifo1_local,
    fifo_PE6_6_op0_config_out,
    fifo_PE6_6_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE6_6_fifo0_local,
    PE6_6_fifo1_local,
    PE6_6_fifo2_local,
    fifo_PE6_6_op1_config_out,
    fifo_PE6_6_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE6_6_fifo2_local,
    fifo2_collect5_6,
    fifo2_collect6_6,
    6,
    6,
    fifo_PE6_6_compute_config_out,
    fifo_PE6_6_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed6_7,
    fifo0_feed7_7,
    PE6_7_fifo0_local,
    fifo_PE5_7_res_config_out,
    fifo_PE6_7_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed6_7,
    fifo1_feed6_8,
    PE6_7_fifo1_local,
    fifo_PE6_7_op0_config_out,
    fifo_PE6_7_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE6_7_fifo0_local,
    PE6_7_fifo1_local,
    PE6_7_fifo2_local,
    fifo_PE6_7_op1_config_out,
    fifo_PE6_7_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE6_7_fifo2_local,
    fifo2_collect5_7,
    fifo2_collect6_7,
    6,
    7,
    fifo_PE6_7_compute_config_out,
    fifo_PE6_7_res_config_out
  )
  .invoke(U1_op0_transfer_wrapper,
    fifo0_feed6_8,
    fifo0_feed7_8,
    PE6_8_fifo0_local,
    fifo_PE5_8_res_config_out,
    fifo_PE6_8_op0_config_out
  )
  .invoke(U1_op1_transfer_last_wrapper,
    fifo1_feed6_8,
    PE6_8_fifo1_local,
    fifo_PE6_8_op0_config_out,
    fifo_PE6_8_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE6_8_fifo0_local,
    PE6_8_fifo1_local,
    PE6_8_fifo2_local,
    fifo_PE6_8_op1_config_out,
    fifo_PE6_8_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE6_8_fifo2_local,
    fifo2_collect5_8,
    fifo2_collect6_8,
    6,
    8,
    fifo_PE6_8_compute_config_out,
    fifo_PE6_8_res_config_out
  )
  .invoke(U1_op0_transfer_last_wrapper,
    fifo0_feed7_0,
    PE7_0_fifo0_local,
    fifo_PE6_0_res_config_out,
    fifo_PE7_0_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed7_0,
    fifo1_feed7_1,
    PE7_0_fifo1_local,
    fifo_PE7_0_op0_config_out,
    fifo_PE7_0_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE7_0_fifo0_local,
    PE7_0_fifo1_local,
    PE7_0_fifo2_local,
    fifo_PE7_0_op1_config_out,
    fifo_PE7_0_compute_config_out
  )
  .invoke(U1_res_transfer_last,
    PE7_0_fifo2_local,
    fifo2_collect6_0,
    fifo2_collect7_0,
    7,
    0,
    fifo_PE7_0_compute_config_out
  )
  .invoke(U1_op0_transfer_last_wrapper,
    fifo0_feed7_1,
    PE7_1_fifo0_local,
    fifo_PE6_1_res_config_out,
    fifo_PE7_1_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed7_1,
    fifo1_feed7_2,
    PE7_1_fifo1_local,
    fifo_PE7_1_op0_config_out,
    fifo_PE7_1_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE7_1_fifo0_local,
    PE7_1_fifo1_local,
    PE7_1_fifo2_local,
    fifo_PE7_1_op1_config_out,
    fifo_PE7_1_compute_config_out
  )
  .invoke(U1_res_transfer_last,
    PE7_1_fifo2_local,
    fifo2_collect6_1,
    fifo2_collect7_1,
    7,
    1,
    fifo_PE7_1_compute_config_out
  )
  .invoke(U1_op0_transfer_last_wrapper,
    fifo0_feed7_2,
    PE7_2_fifo0_local,
    fifo_PE6_2_res_config_out,
    fifo_PE7_2_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed7_2,
    fifo1_feed7_3,
    PE7_2_fifo1_local,
    fifo_PE7_2_op0_config_out,
    fifo_PE7_2_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE7_2_fifo0_local,
    PE7_2_fifo1_local,
    PE7_2_fifo2_local,
    fifo_PE7_2_op1_config_out,
    fifo_PE7_2_compute_config_out
  )
  .invoke(U1_res_transfer_last,
    PE7_2_fifo2_local,
    fifo2_collect6_2,
    fifo2_collect7_2,
    7,
    2,
    fifo_PE7_2_compute_config_out
  )
  .invoke(U1_op0_transfer_last_wrapper,
    fifo0_feed7_3,
    PE7_3_fifo0_local,
    fifo_PE6_3_res_config_out,
    fifo_PE7_3_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed7_3,
    fifo1_feed7_4,
    PE7_3_fifo1_local,
    fifo_PE7_3_op0_config_out,
    fifo_PE7_3_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE7_3_fifo0_local,
    PE7_3_fifo1_local,
    PE7_3_fifo2_local,
    fifo_PE7_3_op1_config_out,
    fifo_PE7_3_compute_config_out
  )
  .invoke(U1_res_transfer_last,
    PE7_3_fifo2_local,
    fifo2_collect6_3,
    fifo2_collect7_3,
    7,
    3,
    fifo_PE7_3_compute_config_out
  )
  .invoke(U1_op0_transfer_last_wrapper,
    fifo0_feed7_4,
    PE7_4_fifo0_local,
    fifo_PE6_4_res_config_out,
    fifo_PE7_4_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed7_4,
    fifo1_feed7_5,
    PE7_4_fifo1_local,
    fifo_PE7_4_op0_config_out,
    fifo_PE7_4_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE7_4_fifo0_local,
    PE7_4_fifo1_local,
    PE7_4_fifo2_local,
    fifo_PE7_4_op1_config_out,
    fifo_PE7_4_compute_config_out
  )
  .invoke(U1_res_transfer_last,
    PE7_4_fifo2_local,
    fifo2_collect6_4,
    fifo2_collect7_4,
    7,
    4,
    fifo_PE7_4_compute_config_out
  )
  .invoke(U1_op0_transfer_last_wrapper,
    fifo0_feed7_5,
    PE7_5_fifo0_local,
    fifo_PE6_5_res_config_out,
    fifo_PE7_5_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed7_5,
    fifo1_feed7_6,
    PE7_5_fifo1_local,
    fifo_PE7_5_op0_config_out,
    fifo_PE7_5_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE7_5_fifo0_local,
    PE7_5_fifo1_local,
    PE7_5_fifo2_local,
    fifo_PE7_5_op1_config_out,
    fifo_PE7_5_compute_config_out
  )
  .invoke(U1_res_transfer_last,
    PE7_5_fifo2_local,
    fifo2_collect6_5,
    fifo2_collect7_5,
    7,
    5,
    fifo_PE7_5_compute_config_out
  )
  .invoke(U1_op0_transfer_last_wrapper,
    fifo0_feed7_6,
    PE7_6_fifo0_local,
    fifo_PE6_6_res_config_out,
    fifo_PE7_6_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed7_6,
    fifo1_feed7_7,
    PE7_6_fifo1_local,
    fifo_PE7_6_op0_config_out,
    fifo_PE7_6_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE7_6_fifo0_local,
    PE7_6_fifo1_local,
    PE7_6_fifo2_local,
    fifo_PE7_6_op1_config_out,
    fifo_PE7_6_compute_config_out
  )
  .invoke(U1_res_transfer_last,
    PE7_6_fifo2_local,
    fifo2_collect6_6,
    fifo2_collect7_6,
    7,
    6,
    fifo_PE7_6_compute_config_out
  )
  .invoke(U1_op0_transfer_last_wrapper,
    fifo0_feed7_7,
    PE7_7_fifo0_local,
    fifo_PE6_7_res_config_out,
    fifo_PE7_7_op0_config_out
  )
  .invoke(U1_op1_transfer_wrapper,
    fifo1_feed7_7,
    fifo1_feed7_8,
    PE7_7_fifo1_local,
    fifo_PE7_7_op0_config_out,
    fifo_PE7_7_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE7_7_fifo0_local,
    PE7_7_fifo1_local,
    PE7_7_fifo2_local,
    fifo_PE7_7_op1_config_out,
    fifo_PE7_7_compute_config_out
  )
  .invoke(U1_res_transfer_last,
    PE7_7_fifo2_local,
    fifo2_collect6_7,
    fifo2_collect7_7,
    7,
    7,
    fifo_PE7_7_compute_config_out
  )
  .invoke(U1_op0_transfer_last_wrapper,
    fifo0_feed7_8,
    PE7_8_fifo0_local,
    fifo_PE6_8_res_config_out,
    fifo_PE7_8_op0_config_out
  )
  .invoke(U1_op1_transfer_last_wrapper,
    fifo1_feed7_8,
    PE7_8_fifo1_local,
    fifo_PE7_8_op0_config_out,
    fifo_PE7_8_op1_config_out
  )
  .invoke(U1_compute_wrapper,
    PE7_8_fifo0_local,
    PE7_8_fifo1_local,
    PE7_8_fifo2_local,
    fifo_PE7_8_op1_config_out,
    fifo_PE7_8_compute_config_out
  )
  .invoke(U1_res_transfer_wrapper,
    PE7_8_fifo2_local,
    fifo2_collect6_8,
    fifo2_collect7_8,
    7,
    8,
    fifo_PE7_8_compute_config_out,
    fifo_PE7_8_res_config_out
  )
  .invoke(U1_DataCollect2EngineLast,
    fifo2_transfer0,
    fifo2_collect7_8,
    8,
    fifo_PE7_8_res_config_out,
    fifo_DataCollect2Engine8_config_out
  )
  .invoke(U1_DataCollect2Engine0_wrapper,
    fifo2_transfer0,
    fifo2_transfer1,
    fifo2_collect7_7,
    7,
    fifo_DataCollect2Engine8_config_out,
    fifo_DataCollect2Engine7_config_out
  )
  .invoke(U1_DataCollect2Engine0_wrapper,
    fifo2_transfer1,
    fifo2_transfer2,
    fifo2_collect7_6,
    6,
    fifo_DataCollect2Engine7_config_out,
    fifo_DataCollect2Engine6_config_out
  )
  .invoke(U1_DataCollect2Engine0_wrapper,
    fifo2_transfer2,
    fifo2_transfer3,
    fifo2_collect7_5,
    5,
    fifo_DataCollect2Engine6_config_out,
    fifo_DataCollect2Engine5_config_out
  )
  .invoke(U1_DataCollect2Engine0_wrapper,
    fifo2_transfer3,
    fifo2_transfer4,
    fifo2_collect7_4,
    4,
    fifo_DataCollect2Engine5_config_out,
    fifo_DataCollect2Engine4_config_out
  )
  .invoke(U1_DataCollect2Engine0_wrapper,
    fifo2_transfer4,
    fifo2_transfer5,
    fifo2_collect7_3,
    3,
    fifo_DataCollect2Engine4_config_out,
    fifo_DataCollect2Engine3_config_out
  )
  .invoke(U1_DataCollect2Engine0_wrapper,
    fifo2_transfer5,
    fifo2_transfer6,
    fifo2_collect7_2,
    2,
    fifo_DataCollect2Engine3_config_out,
    fifo_DataCollect2Engine2_config_out
  )
  .invoke(U1_DataCollect2Engine0_wrapper,
    fifo2_transfer6,
    fifo2_transfer7,
    fifo2_collect7_1,
    1,
    fifo_DataCollect2Engine2_config_out,
    fifo_DataCollect2Engine1_config_out
  )
  .invoke(U1_DataCollect2Engine0_wrapper,
    fifo2_transfer7,
    fifo2_transfer8,
    fifo2_collect7_0,
    0,
    fifo_DataCollect2Engine1_config_out,
    fifo_DataCollect2Engine0_config_out
  )
  .invoke(U1_DataCollect2Head,
    fifo_data_bypass,
    fifo_config_bypass,
    SA_to_upsample_0,
    fifo2_transfer8,
    fifo_DataCollect2Engine0_config_out
  )
  .invoke(pool, 
  		start_inst, end_inst,
  		cin_load_prev_to_pool_0, 
  		config_SA_to_pool,
  		pool_to_concat_0, 
  		config_pool_to_upsample
  )
  .invoke(upsample, 
  		start_inst, end_inst,
  		SA_to_upsample_0, 
  		config_pool_to_upsample,
  		upsample_to_concat_0, 
  		config_upsample_to_concat
  )
  .invoke(concat, 
  		start_inst, end_inst,
  		pool_to_concat_0, 
  		upsample_to_concat_0, 
  		config_upsample_to_concat,
  		concat_to_add_0, 
  		concat_to_add_1, 
  		config_concat_to_add
  )
  .invoke(add, 
  		start_inst, end_inst,
  		concat_to_add_0, 
  		concat_to_add_1, 
  		config_concat_to_add,
  		add_to_act_and_bn_0, 
  		config_add_to_act_and_bn
  )
  .invoke(act_and_bn, 
  		start_inst, end_inst,
  		bias_load_to_act_and_bn_0, 
  		bias_load_to_act_and_bn_1, 
  		add_to_act_and_bn_0, 
  		config_add_to_act_and_bn,
  		act_and_bn_to_cout_write_0, 
  		config_act_and_bn_to_cout_write
  )
  .invoke(cout_write, 
  		start_inst, end_inst,
  		act_and_bn_to_cout_write_0, 
  		config_act_and_bn_to_cout_write,
  		dram_b0, 
  		cin_to_cout_sync, cout_to_cin_sync
  );
}
