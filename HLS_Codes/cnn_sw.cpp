#include "pose.h"

#define LAYER2_IN_NUM 24
#define LAYER2_OUT_NUM 48
#define LAYER2_IN_H 16
#define LAYER2_IN_W 16
#define LAYER2_OUT_H 16
#define LAYER2_OUT_W 16
#define LAYER2_K 3

#define LAYER3_IN_NUM 48
#define LAYER3_OUT_NUM 96
#define LAYER3_IN_H 16
#define LAYER3_IN_W 16
#define LAYER3_OUT_H 8
#define LAYER3_OUT_W 8
#define LAYER3_K 3

#define LAYER4_IN_NUM 96
#define LAYER4_OUT_NUM 96
#define LAYER4_IN_H 8
#define LAYER4_IN_W 8
#define LAYER4_OUT_H 8
#define LAYER4_OUT_W 8
#define LAYER4_K 3

#define LAYER5_IN_NUM 96
#define LAYER5_OUT_NUM 192
#define LAYER5_IN_H 8
#define LAYER5_IN_W 8
#define LAYER5_OUT_H 4
#define LAYER5_OUT_W 4
#define LAYER5_K 3




// Extract hardware outputs
void openpose_postprocess(
  data_t0* cin_hw,
  data_t0  LAYER_out[STAGE2L_OUT_H][STAGE2L_OUT_W][STAGE2R_OUT_NUM + STAGE2L_OUT_NUM]
//  data_t0 LAYERL_out[STAGE2L_OUT_NUM][STAGE2L_OUT_H][STAGE2L_OUT_W],  
//  data_t0 LAYERR_out[STAGE2R_OUT_NUM][STAGE2R_OUT_H][STAGE2R_OUT_W]
){
  // Cout layout: [OUT_NUM / OUT_NUM_T][OUT_H + K - 1][OUT_W + K - 1][OUT_NUM_T]
  for (int o1 = 0; o1 < STAGE2L_OUT_NUM_HW / STAGE2L_OUT_NUM_T; o1++)
    for (int h = 0; h < STAGE2L_OUT_H; h++)
      for (int w = 0; w < STAGE2L_OUT_W; w++)
        for (int o2 = 0; o2 < STAGE2L_OUT_NUM_T; o2++){
          int o = o1 * STAGE2L_OUT_NUM_T + o2;
          if (o < STAGE2L_OUT_NUM){
            LAYER_out[h][w][o + STAGE2R_OUT_NUM] = cin_hw[STAGE2L_OFFSET + o1 * STAGE2L_OUT_H_HW * STAGE2L_OUT_W_HW * STAGE2L_OUT_NUM_T + (h + int(STAGE2L_K / 2)) * STAGE2L_OUT_W_HW * STAGE2L_OUT_NUM_T + (w + int(STAGE2L_K / 2)) * STAGE2L_OUT_NUM_T + o2];
          }
        }
  for (int o1 = 0; o1 < STAGE2R_OUT_NUM_HW / STAGE2R_OUT_NUM_T; o1++)
    for (int h = 0; h < STAGE2R_OUT_H; h++)
      for (int w = 0; w < STAGE2R_OUT_W; w++)
        for (int o2 = 0; o2 < STAGE2R_OUT_NUM_T; o2++){
          int o = o1 * STAGE2R_OUT_NUM_T + o2;
          if (o < STAGE2R_OUT_NUM){
            LAYER_out[h][w][o] = cin_hw[STAGE2R_OFFSET + o1 * STAGE2R_OUT_H_HW * STAGE2R_OUT_W_HW * STAGE2R_OUT_NUM_T + (h + int(STAGE2R_K / 2)) * STAGE2R_OUT_W_HW * STAGE2R_OUT_NUM_T + (w + int(STAGE2R_K / 2)) * STAGE2R_OUT_NUM_T + o2];
          }
        }
}


// Loads inputs, weights, and bias data for a layer in mobilenet
void mobilenet_preprocess(
  data_t0* cin_hw,
  data_t1* weight_hw,
  data_t2* bias_hw,
  data_t0  LAYER_out[out_h][out_w][out_num]
//  data_t0  LAYERL_out[STAGE2L_OUT_NUM][STAGE2L_OUT_H][STAGE2L_OUT_W],  
//  data_t0  LAYERR_out[STAGE2R_OUT_NUM][STAGE2R_OUT_H][STAGE2R_OUT_W]
){
  char* prj_path_c = getenv("PRJ_PATH");
  //char* prj_path_c = "/curr/atefehSZ/research/mobilenetv2-simple/openposeFPGA_mobilenet";
  // Prepare the software buffers
  cout << std::fixed << "Preparing data..." << endl;
  
  
  // first layer
  const int input_in_num = 3;
  const int input_h = 384;
  const int input_w = 384;
  const int in_num_t = 8;
  const int in_h_t = 12;
  const int in_w_t = 48;
  const int in_num_hw = 8;
  const int in_h_hw = 386;
  const int in_w_hw = 386;
  const int layer1_out_num_hw = 96;
  const int weight_size = WEIGHT_SIZE;
  const int bias_size = BIAS_SIZE;
  const int out_num = 57;
  //const int out_num = 96;
  const int out_h = 48;
  const int out_w = 48;
  const int in_filter = 3;
  
  // Load the inputs for the network
  static data_t0 LAYER1_cin[input_in_num][input_h][input_w];
  cout << "Loading input..." << endl; 
  string file_path = string(prj_path_c) + "/data_layer/input.dat";  
  //string file_path = string(prj_path_c) + "/data/conv0_input.dat"; 
  ifstream input_file(file_path.c_str());
  if (input_file.is_open()){

    int idx = 0;
    for (int i = 0; i < input_in_num; i++)
      for (int h = 0; h < input_h; h++)
        for (int w = 0; w < input_w; w++)
        {
          input_file >> LAYER1_cin[i][h][w];
          idx++;
        }

    input_file.close();
  } else {
    cout << "Input open failed!" << endl;
    exit(-1);
  }
  //delete[] bin_input;

  // Initialize the hardware input buffer
  // Cin layout: [IN_NUM / IN_NUM_T][IN_H + K - 1][IN_W + K - 1][IN_NUM_T]
  for (int i1 = 0; i1 < in_num_hw / in_num_t; i1++)
    for (int h = 0; h < input_h; h++)
      for (int w = 0; w < input_w; w++)
        for (int i2 = 0; i2 < in_num_t; i2++){
          int i = i1 * in_num_t + i2;
          if (i < input_in_num){
            cin_hw[i1 * in_h_hw * in_w_hw * in_num_t + (h + int(in_filter / 2)) * in_w_hw * in_num_t + (w + int(in_filter/ 2)) * in_num_t + i2] = LAYER1_cin[i][h][w]; // filter size = 3
          }
        }

  // Load weights
  cout << "Loading weight..." << endl;
  file_path = string(prj_path_c) + "/data/weight_reorg.dat"; 
  ifstream weight_file(file_path.c_str()); 
  //ifstream weight_file(file_path.c_str(), ios::binary | ios::in);
  //bin_input = new char[sizeof(data_t1) * WEIGHT_SIZE];
  if (weight_file.is_open()){

    for (int w = 0; w < weight_size; w++){
      weight_file >> weight_hw[w];
    }

    weight_file.close();
  } else {
    cout << "Weight open failed!" << endl;
    exit(-1);
  }
  
  //delete[] bin_input;

  // Load bias
  cout << "Loading bias..." << endl;
  file_path = string(prj_path_c) +  "/data/bias_reorg.dat";
  ifstream bias_file(file_path.c_str());
  //bin_input = new char[sizeof(data_t2) * BIAS_SIZE];  

  if (bias_file.is_open()){
    //bias_file.read(bin_input, sizeof(data_t2) * BIAS_SIZE);
    //data_t2* convt_input = (data_t2*)bin_input;

    for (int w = 0; w < bias_size; w++){
      bias_file >> bias_hw[w];    
    }
    bias_file.close();
  } else {
    cout << "Bias open failed!" << endl;
    exit(-1);
  }

  //delete[] bin_input;

  // Load outputs
  cout << "Loading output..." << endl;
  //file_path = string(prj_path_c) + "/data_layer/expand_relu.dat";
  //file_path = string(prj_path_c) + "/data/stage6_l2_5.dat";
  file_path = string(prj_path_c) + "/data_layer/output.dat";
  //file_path = string(prj_path_c) + "/data_layer/conv0_relu.dat";
  ifstream output_file(file_path.c_str()); 

  if (output_file.is_open()){
    //output_file.read(bin_input, sizeof(data_t0) * (STAGE2L_OUT_H * STAGE2L_OUT_W * STAGE2L_OUT_NUM + STAGE2R_OUT_H * STAGE2R_OUT_W * STAGE2R_OUT_NUM));
    //data_t0* convt_input = (data_t0*)bin_input;

    int idx = 0;
    for (int o = 0; o < STAGE2R_OUT_NUM; o++)
      for (int h = 0; h < out_h; h++)
        for (int w = 0; w < out_w; w++)
        {
          output_file >> LAYER_out[h][w][o];
          idx++;
        }
    output_file.close();
  } else {
    cout << "Output open failed!" << endl;
    exit(-1);
  }
  
  
  file_path = string(prj_path_c) + "/data/stage6_l1_5.dat";
  ifstream output_file2(file_path.c_str()); 

  if (output_file2.is_open()){
    int idx = 0;
    for (int o = 0; o < STAGE2L_OUT_NUM; o++)
      for (int h = 0; h < out_h; h++)
        for (int w = 0; w < out_w; w++)
        {
          output_file2 >> LAYER_out[h][w][o + STAGE2R_OUT_NUM];
          idx++;
        }
    output_file2.close();
  } else {
    cout << "Output open failed!" << endl;
    exit(-1);
  }

  //delete[] bin_input;
}



// Extract hardware outputs for one layer
void mobilenet_postprocess(
  data_t0* cin_hw,
  data_t0  LAYER_out[out_h][out_w][out_num],
  uint offset,
  uint out_h_hw,
  uint out_h,
  uint out_w_hw,
  uint out_w,
  uint out_num_t,
  uint out_num_hw,
  uint out_num,
  uint k
){
  // Cout layout: [OUT_NUM / OUT_NUM_T][OUT_H + K - 1][OUT_W + K - 1][OUT_NUM_T]
  for (int o1 = 0; o1 < out_num_hw / out_num_t; o1++)
    for (int h = 0; h < out_h; h++)
      for (int w = 0; w < out_w; w++)
        for (int o2 = 0; o2 < out_num_t; o2++){
          int o = o1 * out_num_t + o2;
          if (o < out_num){
            LAYER_out[h][w][o] = cin_hw[offset + o1 * out_h_hw * out_w_hw * out_num_t + (h + int(k / 2)) * out_w_hw * out_num_t + (w + int(k / 2)) * out_num_t + o2];
          }
        }
        
        //num_iter / LAYER_OUT_NUM_T * LAYER_OUT_H_HW * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + h * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + in_w_iter * LAYER_OUT_NUM_T + cout_offset
  /*for (int o1 = 0; o1 < STAGE2R_OUT_NUM_HW / STAGE2R_OUT_NUM_T; o1++)
    for (int h = 0; h < STAGE2R_OUT_H; h++)
      for (int w = 0; w < STAGE2R_OUT_W; w++)
        for (int o2 = 0; o2 < STAGE2R_OUT_NUM_T; o2++){
          int o = o1 * STAGE2R_OUT_NUM_T + o2;
          if (o < STAGE2R_OUT_NUM){
            LAYER_out[h][w][o] = cin_hw[STAGE2R_OFFSET + o1 * STAGE2R_OUT_H_HW * STAGE2R_OUT_W_HW * STAGE2R_OUT_NUM_T + (h + int(STAGE2R_K / 2)) * STAGE2R_OUT_W_HW * STAGE2R_OUT_NUM_T + (w + int(STAGE2R_K / 2)) * STAGE2R_OUT_NUM_T + o2];
          }
        }*/
}
