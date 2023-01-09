void upsample(
  	hls::stream<ReluData0Type>  &fifo_cin,
		hls::stream<ConfigInst>     &fifo_config_in,
		hls::stream<PoolData0Type>  &fifo_cout,
		hls::stream<ConfigInst>     &fifo_config_out
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
}