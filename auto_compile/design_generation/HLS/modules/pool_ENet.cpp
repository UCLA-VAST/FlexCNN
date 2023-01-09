/**
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
}