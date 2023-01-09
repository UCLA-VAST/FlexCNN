
/**
 * Function name: add
 * Function description: This functions add the input of previous layer to result of this layer.
 *                       It helps supporting building blocks such as residual bottleneck layer in MobileNetV2
 */
void add(
		hls::stream<ConvData0Type>        &fifo_cin,
		hls::stream<ConvData0Type>        &fifo_conv,
		hls::stream<ConfigInst>           &fifo_config_in,
		hls::stream<ConvData0Type>        &fifo_cout,
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
            ConvData0Type conv_tmp = 0;
            conv_tmp = fifo_conv.read();
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
}