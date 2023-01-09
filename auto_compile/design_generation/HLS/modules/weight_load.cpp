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