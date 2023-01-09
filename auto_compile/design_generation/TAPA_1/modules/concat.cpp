/**
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