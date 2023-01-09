/*
 * Function name: cin_load_ddr_read
 * Function description: This function loads cin results from off-chip DRAM.
 *                       Two modes are enabled. If the whole feature maps of the layer could fit
 *                       in the on-chip buffer, they will be loaded as a whole. Otherwise, each time,
 *                       LAYER_IN_NUM_T * (LAYER_IN_W_T + FILTER_S - 1) of data are loaded.
 */
void cin_load_ddr_read(
		bus_t0     		*global_cin,
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
		hls::stream<CinLoadData0Type>   &fifo_cin,
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
		bus_t0                         *global_cin,
		bus_t3                         config[CONFIG_PARAMS],
		hls::stream<CinLoadData0Type>  &fifo_cin,
		hls::stream<ConfigInst>        &fifo_config_out
){
	#pragma HLS INLINE off 

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
}
/*
 * Function name: weight_load_conv_weight_write
 * Function description: this function writes conv weights to conv module.
 * It has the same functionality as weight_load_depth_conv_weight_write
 */
void weight_load_conv_weight_write(
		bus_t1 weight_burst_buf2[],
		hls::stream<WeightLoadData1Type> &fifo_conv_weight,
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
		bus_t1                           *global_weight,
		hls::stream<ConfigInst>          &fifo_config_in,
		hls::stream<WeightLoadData1Type> &fifo_conv_weight,
		hls::stream<ConfigInst>          &fifo_config_out
){
	#pragma HLS INLINE off 
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
}
/*
 * Function name: weight_load_bias_write
 * Function description: This function writes bias to relu module.
 */
void weight_load_bias_write(
		bus_t2 bias_burst_buf[],
		hls::stream<WeightLoadData2Type> &fifo_bias,
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
		bus_t2                           *global_bias,
		hls::stream<ConfigInst>          &fifo_config_in,
		// hls::stream<ConvData0Type>       &fifo_gamma_conv_in,
		// hls::stream<ConvData0Type>       &fifo_beta_conv_in,
    hls::stream<ConvData0Type>       &fifo_gamma_conv_out,
    hls::stream<ConvData0Type>       &fifo_beta_conv_out,
		hls::stream<ConfigInst>          &fifo_config_out
){
	#pragma HLS INLINE off 
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
}/**
 * Function name: conv
 * Function description: This function performs normal convoluation for any given filter size
 *                       Currently, it is configured to work with the Systolic array
 *                       Note that the code for Systolic array is not in this file, to see how to add it refer to the README
 *                       If you want to work with a simple compute engine for conv, uncomment the "kernel" module in this file
 */
void SA(
		hls::stream<WeightLoadData1Type> &fifo_weight,
    hls::stream<DepthConvData0Type>  &fifo_cin,
		hls::stream<ConfigInst>          &fifo_config_in,
		hls::stream<ConvData0Type>       &fifo_cout,
		hls::stream<ConfigInst>          &fifo_config_out
){
	#pragma HLS INLINE off 
	uint in_num_iter = 0;
	uint out_num_iter = 0;
	uint in_h_iter = 0;
	uint in_w_iter = 0;
	uint layer_iter = 0;

		// Dummpy first read
		// The previous module (currently relu6) should write the instructions twice
	ConfigInst inst0 = fifo_config_in.read();
	ConfigInst inst1 = fifo_config_in.read();
	ConfigInst inst2 = fifo_config_in.read();
	ConfigInst inst3 = fifo_config_in.read();
	ConfigInst inst4 = fifo_config_in.read();
	ConfigInst inst5 = fifo_config_in.read();

	ap_uint<32> LAYER_BATCH = inst3(32*5+31, 32*5);
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

	ap_uint<1>  CONV_1ST_EN    = LAYER_EN[0];
	ap_uint<1>  DEPTH_CONV_EN  = LAYER_EN[1];
	ap_uint<1>  CONV_EN        = LAYER_EN[2];
	ap_uint<1>  RELU_EN        = LAYER_EN[3];
	ap_uint<1>  RELU6_EN       = LAYER_EN[4];
	ap_uint<1>  POOL_EN        = LAYER_EN[5];
	ap_uint<1>  UP_SAMPLE_EN   = LAYER_EN[6];  	// reserved
	ap_uint<1>  BIAS_EN        = LAYER_EN[7];
	ap_uint<1>  BATCH_NORM_EN  = LAYER_EN[10];

	uint FILTER_S = (CONV_EN == 1)? (uint)FILTER_S2: 1;
	bool separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
	bool conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
	bool max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);

	bool test = (CONV_EN == 0);
	switch(CONV_EN){
	case 0:
			// bypass
		for (int layer_iter = 0; layer_iter < LAYER_BATCH; layer_iter++){
				// Read instructions
	#ifdef DEBUG_config_conv
			cout << LAYER_IN_NUM_HW << " " << LAYER_OUT_NUM_HW << " " << LAYER_IN_H_HW << " " << LAYER_IN_W_HW << " " << LAYER_OUT_H_HW << " " << LAYER_OUT_W_HW << endl;
			cout << LAYER_IN_NUM << " " << LAYER_OUT_NUM << " " << LAYER_IN_H << " " << LAYER_IN_W << " " << LAYER_OUT_H << " " << LAYER_OUT_W << endl;
			cout << CIN_OFFSET << " " << WEIGHT_OFFSET << " " << BIAS_OFFSET << " " << COUT_OFFSET << " " << FILTER_S1 << " " << FILTER_S2 << " " << STRIDE << endl;
			cout << LAYER_EN << " " << PREV_CIN_OFFSET << " " << LAYER_IN_NUM_T << " " << LAYER_OUT_NUM_T << " " << LAYER_IN_H_T << " " << LAYER_IN_W_T << endl;
	#endif
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
				// Refer to cin_load module to understand the meaning of the instructions
				// inst0
			LAYER_IN_NUM_HW  = inst0(32*0+31, 32*0);
			LAYER_OUT_NUM_HW = inst0(32*1+31, 32*1);
			LAYER_IN_H_HW    = inst0(32*2+31, 32*2);
			LAYER_IN_W_HW    = inst0(32*3+31, 32*3);
			LAYER_OUT_H_HW   = inst0(32*4+31, 32*4);
			LAYER_OUT_W_HW   = inst0(32*5+31, 32*5);
				// inst1
			LAYER_IN_NUM     = inst1(32*0+31, 32*0);
			LAYER_OUT_NUM    = inst1(32*1+31, 32*1);
			LAYER_IN_H       = inst1(32*2+31, 32*2);
			LAYER_IN_W       = inst1(32*3+31, 32*3);
			LAYER_OUT_H      = inst1(32*4+31, 32*4);
			LAYER_OUT_W      = inst1(32*5+31, 32*5);
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

			CONV_1ST_EN      = LAYER_EN[0];
			DEPTH_CONV_EN    = LAYER_EN[1];
			CONV_EN          = LAYER_EN[2];
			RELU_EN          = LAYER_EN[3];
			RELU6_EN         = LAYER_EN[4];
			POOL_EN          = LAYER_EN[5];
			UP_SAMPLE_EN     = LAYER_EN[6]; 	// reserved

				// Set up some configuration signals
			FILTER_S = (CONV_EN == 1)? (uint)FILTER_S2: 1;
			separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
			conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
			max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);

			int in_h_iter = 0;
			int in_w_iter = 0;
			int out_num_iter = 0;
			int in_num_iter = 0;
			bool done1 = 0;
			while(!done1){
				if ((max_pool && out_num_iter == 0) || (UP_SAMPLE_EN && out_num_iter == 0)){
					int o = 0;
					int h = 0;
					int w = 0;
					bool done2 = 0;
					while(!done2){
	#pragma HLS PIPELINE II=1
						DepthConvData0Type tmp = fifo_cin.read();
						fifo_cout.write(tmp);
						
							// Repeat until the whole tile is read
						w++;
						if (w == LAYER_IN_W_T + FILTER_S - 1){
							w = 0;
							h++;
							if (h == LAYER_IN_H_T + FILTER_S - 1){
								h = 0;
								o++;
								if (o == LAYER_IN_NUM_T / CONV_LANE){
									o = 0;
									done2 = 1;
								}
							}
						}
					}
				}
	#ifdef DEBUG_config_conv
				cout << in_num_iter << " " << out_num_iter << " " << in_w_iter << " " << in_h_iter << endl;
	#endif
					// Repeat until all the tiles are read
					// Must repeat the computation until LAYER_OUT_NUM output feature maps are generated
				in_num_iter += LAYER_IN_NUM_T;
				if (in_num_iter >= LAYER_IN_NUM){
					in_num_iter = 0;
					out_num_iter += LAYER_OUT_NUM_T;
					if (out_num_iter >= LAYER_OUT_NUM){
						out_num_iter = 0;
						in_w_iter += LAYER_IN_W_T;
						if (in_w_iter >= LAYER_IN_W){
							in_w_iter = 0;
							in_h_iter += LAYER_IN_H_T;
							if (in_h_iter >= LAYER_IN_H){
								in_h_iter = 0;
								done1 = 1;
							}
						}
					}
				}

			}
		}
		break;
		// compute
	case 1:
	#ifdef DEBUG_kernel
		cout << "before kernel" << endl;
	#endif
			// int count = 0;
			// while(!fifo_cin.empty()){
			// 	DepthConvData0Type item = fifo_cin.read();
			// 				// float num[8];
			// 				// 	// printf("output: ");
			// 				// for(int i=0; i<8; i++){
			// 				// 	num[i] = Reinterpret<float>((ap_uint<32>)item.data((i+1)*32-1, 32*i));
			// 				// 	printf("%10f\t", num[i]);
			// 				// }
			// 				// printf("\n");
			// 	count++;
			// }
			// cout<<"cins: "<<count<<endl;
			// count = 0;
			// while(!fifo_weight.empty()){
			// 	DepthConvData0Type item = fifo_weight.read();
			// 				// float num[8];
			// 				// 	// printf("output: ");
			// 				// for(int i=0; i<8; i++){
			// 				// 	num[i] = Reinterpret<float>((ap_uint<32>)item.data((i+1)*32-1, 32*i));
			// 				// 	printf("%10f\t", num[i]);
			// 				// }
			// 				// printf("\n");
			// 	count++;
			// }
			// cout<<"weights: "<<count<<endl;
			// return;
			// Calls systolic array
			// Refer to README to see how to add the systolic array
			// You can replace it with your own implementations
			// If you want to check with a simple implementation, uncomment the "kernel" module in this file
    
		kernel(fifo_cin, fifo_weight, fifo_cout, fifo_config_in, fifo_config_out);
	#ifdef DEBUG_kernel
		
	#endif
		break;
	}
}void act_and_bn(
		hls::stream<ConvData0Type>        &fifo_gamma_conv,
		hls::stream<ConvData0Type>        &fifo_beta_conv,
		hls::stream<ConvData0Type>        &fifo_cin,
		hls::stream<ConfigInst>           &fifo_config_in,
		hls::stream<ReluData0Type>        &fifo_cout,
		hls::stream<ConfigInst>           &fifo_config_out
){
	#pragma HLS INLINE off
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
					ConvData0Type beta = fifo_beta_conv.read();
					ConvData0Type gamma = fifo_gamma_conv.read();
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

				int w_bound = (LAYER_IN_W_T / STRIDE)*LAYER_TCONV_STRIDE;
				int h_bound = (LAYER_IN_H_T / STRIDE)*LAYER_TCONV_STRIDE;

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
            // if (BATCH_NORM_EN){
            //   tmp = tmp * (data_t0) 0.9995003746640602;
            // }
            if (RELU_EN){
              // cout<<tmp<<" + "<<beta_buf[o][lane]<<" = "<<tmp + beta_buf[o][lane]<<endl;
              tmp = tmp + beta_buf[o][lane];
              tmp = max(tmp, (data_t0) 0);
            }
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
	#if RELU_LANE == 32
							cout_buf[31], cout_buf[30], cout_buf[29], cout_buf[28], cout_buf[27], cout_buf[26], cout_buf[25], cout_buf[24],
							cout_buf[23], cout_buf[22], cout_buf[21], cout_buf[20], cout_buf[19], cout_buf[18], cout_buf[17], cout_buf[16],
							cout_buf[15], cout_buf[14], cout_buf[13], cout_buf[12], cout_buf[11], cout_buf[10], cout_buf[9], cout_buf[8],
							cout_buf[7], cout_buf[6], cout_buf[5], cout_buf[4], cout_buf[3], cout_buf[2], cout_buf[1], cout_buf[0]
	#elif RELU_LANE == 16
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
}/**
 * Function name: pool
 * Function description: This functions performs max-pooling operation.
 */
void pool(
		hls::stream<ReluData0Type>  &fifo_cin,
		hls::stream<ConfigInst>     &fifo_config_in,
		hls::stream<PoolData0Type>  &fifo_cout,
		hls::stream<ConfigInst>     &fifo_config_out
){
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
    ap_uint<32> LAYER_NUM_T = LAYER_OUT_NUM_T;
    STRIDE = POOL_EN? (ap_uint<32>) 2 :  (ap_uint<32>) 1;
			// Set up some configuration signals
		bool en = POOL_EN;
		bool separable_conv = (DEPTH_CONV_EN == 1) && (CONV_EN == 1);
		bool conv2d = (DEPTH_CONV_EN == 0) && (CONV_EN == 1);
		bool max_pool = (DEPTH_CONV_EN == 0) && (CONV_EN == 0);
    // if(LOAD_PREV_CIN == 1){
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
          // if(!fifo_cin.empty()){
            PoolData0Type tmp = fifo_cin.read();
            fifo_cout.write(tmp);
          // }
					
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
		// }
		
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
        if (num_iter >= LAYER_OUT_NUM_HW){
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
	}
}
/**
 * Function name: cout_write_fifo_read
 * Function description: This function reads cout data.
 */
void cout_write_fifo_read(
		bus_t0 *cout_burst_buf,
		hls::stream<PoolData0Type>  &fifo_cout,
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
		bus_t0 *global_cout,
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
		hls::stream<PoolData0Type>  &fifo_cout,
		hls::stream<ConfigInst>     &fifo_config_in,
		bus_t0                      *global_cout
){

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
}
/**
You can add any new module that you want here.
You just need to follow the coding structure of the other modules.
You can uncomment the commented modules if your application needs them. All the modules listed in this file (commneted or uncommented) has a working implementation.
For convolution, you can either choose to use a naive implementation (which is slow), or add the systolic array kernel.
**/
void engine(
    bus_t0 *global_cin,
    bus_t0 *global_prev_cin,
    bus_t1 *global_weight,
    bus_t2 *global_bias,
    bus_t0 *global_cout,
    bus_t3 config[CONFIG_PARAMS],
    uint layer_id
){
#pragma HLS DATAFLOW
  //data fifos
  hls::stream<CinLoadData0Type> cin_load_to_SA_0("cin_load_to_SA_0");
  #pragma HLS STREAM variable=cin_load_to_SA_0 depth=128
  hls::stream<CinLoadData0Type> weight_load_to_SA_0("weight_load_to_SA_0");
  #pragma HLS STREAM variable=weight_load_to_SA_0 depth=128
  hls::stream<CinLoadData0Type> bias_load_to_act_and_bn_0("bias_load_to_act_and_bn_0");
  #pragma HLS STREAM variable=bias_load_to_act_and_bn_0 depth=128
  hls::stream<CinLoadData0Type> bias_load_to_act_and_bn_1("bias_load_to_act_and_bn_1");
  #pragma HLS STREAM variable=bias_load_to_act_and_bn_1 depth=128
  hls::stream<CinLoadData0Type> SA_to_act_and_bn_0("SA_to_act_and_bn_0");
  #pragma HLS STREAM variable=SA_to_act_and_bn_0 depth=128
  hls::stream<CinLoadData0Type> act_and_bn_to_pool_0("act_and_bn_to_pool_0");
  #pragma HLS STREAM variable=act_and_bn_to_pool_0 depth=128
  hls::stream<CinLoadData0Type> pool_to_cout_write_0("pool_to_cout_write_0");
  #pragma HLS STREAM variable=pool_to_cout_write_0 depth=128
  //instruction fifos
  hls::stream<ConfigInst> config_cin_load_to_weight_load("config_cin_load_to_weight_load");
  #pragma HLS STREAM variable=config_cin_load_to_weight_load depth=16
  hls::stream<ConfigInst> config_weight_load_to_bias_load("config_weight_load_to_bias_load");
  #pragma HLS STREAM variable=config_weight_load_to_bias_load depth=16
  hls::stream<ConfigInst> config_bias_load_to_SA("config_bias_load_to_SA");
  #pragma HLS STREAM variable=config_bias_load_to_SA depth=16
  hls::stream<ConfigInst> config_SA_to_act_and_bn("config_SA_to_act_and_bn");
  #pragma HLS STREAM variable=config_SA_to_act_and_bn depth=16
  hls::stream<ConfigInst> config_act_and_bn_to_pool("config_act_and_bn_to_pool");
  #pragma HLS STREAM variable=config_act_and_bn_to_pool depth=16
  hls::stream<ConfigInst> config_pool_to_cout_write("config_pool_to_cout_write");
  #pragma HLS STREAM variable=config_pool_to_cout_write depth=16
  layer_id += 1;
  cin_load(
  		global_cin, 
  		config,
  		cin_load_to_SA_0, 
  		config_cin_load_to_weight_load
  );
  #ifdef DEBUG_engine
  	cout << "passed cin_load" << endl;
  #endif
  #ifdef DEBUG_cin_load
  if(layer_id==TARGET_INST){
  	string prj_path = string(getenv("PRJ_PATH"));
  	string layer_id_str = to_string(layer_id);
  	string file_path = prj_path + "/data/test/cin_load_0_"+layer_id_str+".dat";
  	int count = 0;
  	float sum = 0;
  	FILE *f;
  	f = fopen(file_path.c_str(), "w");
  	while(!cin_load_to_SA_0.empty()){
  		ReluData0Type item = cin_load_to_SA_0.read();
  		data_t2 num[8];
  		for(int i=0; i<SIMD_LANE; i++){
  			num[i] = Reinterpret<data_t2>((ap_uint<32>)item((i+1)*32-1, 32*i));
  			fprintf(f, "%10f	", num[i]);
  			count++;
  			sum += num[i];
  		}
  		fprintf(f, "\n");
  	}
  		fprintf(f, "sum: %f\n", sum);
  		fprintf(f, "count: %d\n", count);
  	cout<<"sum: "<<sum<<endl;
  	cout<<"count: "<<count<<endl;
  	fclose(f);
  	exit(0);
  }
  #endif
  weight_load(
  		global_weight, 
  		config_cin_load_to_weight_load,
  		weight_load_to_SA_0, 
  		config_weight_load_to_bias_load
  );
  #ifdef DEBUG_engine
  	cout << "passed weight_load" << endl;
  #endif
  #ifdef DEBUG_weight_load
  if(layer_id==TARGET_INST){
  	string prj_path = string(getenv("PRJ_PATH"));
  	string layer_id_str = to_string(layer_id);
  	string file_path = prj_path + "/data/test/weight_load_0_"+layer_id_str+".dat";
  	int count = 0;
  	float sum = 0;
  	FILE *f;
  	f = fopen(file_path.c_str(), "w");
  	while(!weight_load_to_SA_0.empty()){
  		ReluData0Type item = weight_load_to_SA_0.read();
  		data_t2 num[8];
  		for(int i=0; i<SIMD_LANE; i++){
  			num[i] = Reinterpret<data_t2>((ap_uint<32>)item((i+1)*32-1, 32*i));
  			fprintf(f, "%10f	", num[i]);
  			count++;
  			sum += num[i];
  		}
  		fprintf(f, "\n");
  	}
  		fprintf(f, "sum: %f\n", sum);
  		fprintf(f, "count: %d\n", count);
  	cout<<"sum: "<<sum<<endl;
  	cout<<"count: "<<count<<endl;
  	fclose(f);
  	exit(0);
  }
  #endif
  bias_load(
  		global_bias, 
  		config_weight_load_to_bias_load,
  		bias_load_to_act_and_bn_0, 
  		bias_load_to_act_and_bn_1, 
  		config_bias_load_to_SA
  );
  #ifdef DEBUG_engine
  	cout << "passed bias_load" << endl;
  #endif
  #ifdef DEBUG_bias_load
  if(layer_id==TARGET_INST){
  	string prj_path = string(getenv("PRJ_PATH"));
  	string layer_id_str = to_string(layer_id);
  	string file_path = prj_path + "/data/test/bias_load_0_"+layer_id_str+".dat";
  	int count = 0;
  	float sum = 0;
  	FILE *f;
  	f = fopen(file_path.c_str(), "w");
  	while(!bias_load_to_act_and_bn_0.empty()){
  		ReluData0Type item = bias_load_to_act_and_bn_0.read();
  		data_t2 num[8];
  		for(int i=0; i<SIMD_LANE; i++){
  			num[i] = Reinterpret<data_t2>((ap_uint<32>)item((i+1)*32-1, 32*i));
  			fprintf(f, "%10f	", num[i]);
  			count++;
  			sum += num[i];
  		}
  		fprintf(f, "\n");
  	}
  		fprintf(f, "sum: %f\n", sum);
  		fprintf(f, "count: %d\n", count);
  	cout<<"sum: "<<sum<<endl;
  	cout<<"count: "<<count<<endl;
  	file_path = prj_path + "/data/test/bias_load_1_"+layer_id_str+".dat";
  	count = 0;
  	sum = 0;
  	f = fopen(file_path.c_str(), "w");
  	while(!bias_load_to_act_and_bn_1.empty()){
  		ReluData0Type item = bias_load_to_act_and_bn_1.read();
  		data_t2 num[8];
  		for(int i=0; i<SIMD_LANE; i++){
  			num[i] = Reinterpret<data_t2>((ap_uint<32>)item((i+1)*32-1, 32*i));
  			fprintf(f, "%10f	", num[i]);
  			count++;
  			sum += num[i];
  		}
  		fprintf(f, "\n");
  	}
  		fprintf(f, "sum: %f\n", sum);
  		fprintf(f, "count: %d\n", count);
  	cout<<"sum: "<<sum<<endl;
  	cout<<"count: "<<count<<endl;
  	fclose(f);
  	exit(0);
  }
  #endif
  SA(
  		cin_load_to_SA_0, 
  		weight_load_to_SA_0, 
  		config_bias_load_to_SA,
  		SA_to_act_and_bn_0, 
  		config_SA_to_act_and_bn
  );
  #ifdef DEBUG_engine
  	cout << "passed SA" << endl;
  #endif
  #ifdef DEBUG_SA
  if(layer_id==TARGET_INST){
  	string prj_path = string(getenv("PRJ_PATH"));
  	string layer_id_str = to_string(layer_id);
  	string file_path = prj_path + "/data/test/SA_0_"+layer_id_str+".dat";
  	int count = 0;
  	float sum = 0;
  	FILE *f;
  	f = fopen(file_path.c_str(), "w");
  	while(!SA_to_act_and_bn_0.empty()){
  		ReluData0Type item = SA_to_act_and_bn_0.read();
  		data_t2 num[8];
  		for(int i=0; i<SIMD_LANE; i++){
  			num[i] = Reinterpret<data_t2>((ap_uint<32>)item((i+1)*32-1, 32*i));
  			fprintf(f, "%10f	", num[i]);
  			count++;
  			sum += num[i];
  		}
  		fprintf(f, "\n");
  	}
  		fprintf(f, "sum: %f\n", sum);
  		fprintf(f, "count: %d\n", count);
  	cout<<"sum: "<<sum<<endl;
  	cout<<"count: "<<count<<endl;
  	fclose(f);
  	exit(0);
  }
  #endif
  act_and_bn(
  		bias_load_to_act_and_bn_0, 
  		bias_load_to_act_and_bn_1, 
  		SA_to_act_and_bn_0, 
  		config_SA_to_act_and_bn,
  		act_and_bn_to_pool_0, 
  		config_act_and_bn_to_pool
  );
  #ifdef DEBUG_engine
  	cout << "passed act_and_bn" << endl;
  #endif
  #ifdef DEBUG_act_and_bn
  if(layer_id==TARGET_INST){
  	string prj_path = string(getenv("PRJ_PATH"));
  	string layer_id_str = to_string(layer_id);
  	string file_path = prj_path + "/data/test/act_and_bn_0_"+layer_id_str+".dat";
  	int count = 0;
  	float sum = 0;
  	FILE *f;
  	f = fopen(file_path.c_str(), "w");
  	while(!act_and_bn_to_pool_0.empty()){
  		ReluData0Type item = act_and_bn_to_pool_0.read();
  		data_t2 num[8];
  		for(int i=0; i<SIMD_LANE; i++){
  			num[i] = Reinterpret<data_t2>((ap_uint<32>)item((i+1)*32-1, 32*i));
  			fprintf(f, "%10f	", num[i]);
  			count++;
  			sum += num[i];
  		}
  		fprintf(f, "\n");
  	}
  		fprintf(f, "sum: %f\n", sum);
  		fprintf(f, "count: %d\n", count);
  	cout<<"sum: "<<sum<<endl;
  	cout<<"count: "<<count<<endl;
  	fclose(f);
  	exit(0);
  }
  #endif
  pool(
  		act_and_bn_to_pool_0, 
  		config_act_and_bn_to_pool,
  		pool_to_cout_write_0, 
  		config_pool_to_cout_write
  );
  #ifdef DEBUG_engine
  	cout << "passed pool" << endl;
  #endif
  #ifdef DEBUG_pool
  if(layer_id==TARGET_INST){
  	string prj_path = string(getenv("PRJ_PATH"));
  	string layer_id_str = to_string(layer_id);
  	string file_path = prj_path + "/data/test/pool_0_"+layer_id_str+".dat";
  	int count = 0;
  	float sum = 0;
  	FILE *f;
  	f = fopen(file_path.c_str(), "w");
  	while(!pool_to_cout_write_0.empty()){
  		ReluData0Type item = pool_to_cout_write_0.read();
  		data_t2 num[8];
  		for(int i=0; i<SIMD_LANE; i++){
  			num[i] = Reinterpret<data_t2>((ap_uint<32>)item((i+1)*32-1, 32*i));
  			fprintf(f, "%10f	", num[i]);
  			count++;
  			sum += num[i];
  		}
  		fprintf(f, "\n");
  	}
  		fprintf(f, "sum: %f\n", sum);
  		fprintf(f, "count: %d\n", count);
  	cout<<"sum: "<<sum<<endl;
  	cout<<"count: "<<count<<endl;
  	fclose(f);
  	exit(0);
  }
  #endif
  cout_write(
  		pool_to_cout_write_0, 
  		config_pool_to_cout_write,
  		global_cout 
  );
  #ifdef DEBUG_engine
  	cout << "passed cout_write" << endl;
  #endif
  #ifdef DEBUG_cout_write
  if(layer_id==TARGET_INST){
  	string prj_path = string(getenv("PRJ_PATH"));
  	string layer_id_str = to_string(layer_id);
  	fclose(f);
  	exit(0);
  }
  #endif
}
extern "C" {
	void top_kernel(
			bus_t0 *global_cin,
			bus_t0 *global_prev_cin,
			bus_t0 *global_cout,
			bus_t1 *global_weight,
			bus_t2 *global_bias,
			bus_t3 *layer_config,
			uint start_layer,
			uint end_layer
	){
	#pragma HLS INTERFACE m_axi port=global_cin offset=slave bundle=gmem1 depth=0
	#pragma HLS INTERFACE m_axi port=global_prev_cin offset=slave bundle=gmem3 depth=0
	#pragma HLS INTERFACE m_axi port=global_cout offset=slave bundle=gmem1 depth=826274
	#pragma HLS INTERFACE m_axi port=global_weight offset=slave bundle=gmem2 depth=34234
	#pragma HLS INTERFACE m_axi port=global_bias offset=slave bundle=gmem4 depth=1026
	#pragma HLS INTERFACE m_axi port=layer_config offset=slave bundle=gcontrol depth=2815

	#pragma HLS INTERFACE s_axilite port=global_cin bundle=control
	#pragma HLS INTERFACE s_axilite port=global_prev_cin bundle=control
	#pragma HLS INTERFACE s_axilite port=global_weight bundle=control
	#pragma HLS INTERFACE s_axilite port=global_bias bundle=control
	#pragma HLS INTERFACE s_axilite port=global_cout bundle=control
	#pragma HLS INTERFACE s_axilite port=layer_config bundle=control
	#pragma HLS INTERFACE s_axilite port=return bundle=control
	#define DEBUG_layer
			// Copy the first instruction
		unsigned int init_inst[5]; 	// [LAYERS]
		memcpy((void*)init_inst, (void*)(&layer_config[0]), sizeof(unsigned int) * 5);
		int layers = init_inst[0];

		int layer_num = layers;
		bus_t3 config[CONFIG_PARAMS * MAX_LAYER_BATCH];
		int cur_layer_batch = 1;
		int nxt_layer_batch = 1;
		int layer_id = start_layer;
		while(layer_id < end_layer){
			cur_layer_batch = nxt_layer_batch;

			memcpy((void*)config, (void*)(&layer_config[5 + CONFIG_PARAMS * layer_id]), sizeof(unsigned int) * CONFIG_PARAMS * cur_layer_batch);
			nxt_layer_batch = config[CONFIG_PARAMS * (cur_layer_batch - 1) + 29 - 1];
			config[29 - 1] = cur_layer_batch;
				// call engine module for each of the layers
			engine(global_cin, global_prev_cin, global_weight, global_bias, global_cout, config, layer_id);
			#ifdef DEBUG_layer
				cout << "Passed " << layer_id + 1 << endl;
			#endif
				//layer_id += cur_layer_batch;
			layer_id += 1;
		}
	}
}
