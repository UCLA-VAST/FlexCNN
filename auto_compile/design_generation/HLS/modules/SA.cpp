/**
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
}