
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

    bool bias_en = (CONV_EN == 1 && BIAS_EN == 1);
		bool norm_conv_en = (CONV_EN == 1 && BATCH_NORM_EN == 1);  
    uint beta_conv_offset = 0;
		uint gamma_conv_offset = 0;
    uint bias_offset = BIAS_OFFSET;

    if (bias_en){
      uint global_bias_offset = bias_offset + in_num_iter;
      for (int i = 0; i < MAX_OUT_NUM_T / BUS_PACK_FACTOR2; i++){
        #pragma HLS pipeline
        gamma_conv_burst_buf_out[i] = 0;
      }
      memcpy((void*)beta_conv_burst_buf_out, (void*)&global_bias[global_bias_offset / BUS_PACK_FACTOR2], sizeof(data_t2) * LAYER_IN_NUM_T);
		}else if(norm_conv_en){
      uint global_bias_offset = bias_offset + in_num_iter;
      memcpy((void*)gamma_conv_burst_buf_out, (void*)&global_bias[global_bias_offset / BUS_PACK_FACTOR2], sizeof(data_t2) * LAYER_IN_NUM_T);
      memcpy((void*)gamma_conv_burst_buf_out, (void*)&global_bias[global_bias_offset / BUS_PACK_FACTOR2], sizeof(data_t2) * LAYER_IN_NUM_T);
    }

    if (bias_en || norm_conv_en) {
		  weight_load_bias_write(beta_conv_burst_buf_out, fifo_beta_conv_out, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter, 1);
      weight_load_bias_write(gamma_conv_burst_buf_out, fifo_gamma_conv_out, inst0, inst1, inst2, inst3, in_num_iter, out_num_iter, 1);
    }

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