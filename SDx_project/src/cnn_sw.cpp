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

void extract_layer(
  data_t0* cin_hw,
  uint*    configs,
  uint     layer_id
){
  uint config[CONFIG_PARAMS];
  for (int p = 0; p < CONFIG_PARAMS; p++){
    config[p] = configs[4 + layer_id * CONFIG_PARAMS + p];
  }
  
  // inst0
  ap_uint<32> LAYER_IN_NUM_HW  = config[0];
  ap_uint<32> LAYER_OUT_NUM_HW = config[1];
  ap_uint<32> LAYER_IN_H_HW    = config[2];
  ap_uint<32> LAYER_IN_W_HW    = config[3];
  ap_uint<32> LAYER_OUT_H_HW   = config[4];
  ap_uint<32> LAYER_OUT_W_HW   = config[5];

  // inst1
  ap_uint<32> LAYER_IN_NUM  = config[6];
  ap_uint<32> LAYER_OUT_NUM = config[7];
  ap_uint<32> LAYER_IN_H    = config[8];
  ap_uint<32> LAYER_IN_W    = config[9];
  ap_uint<32> LAYER_OUT_H   = config[10];
  ap_uint<32> LAYER_OUT_W   = config[11];
 
  // inst2
  ap_uint<32> CIN_OFFSET    = config[12];
  ap_uint<32> WEIGHT_OFFSET = config[13];
  ap_uint<32> BIAS_OFFSET   = config[14];
  ap_uint<32> COUT_OFFSET   = config[15];
  ap_uint<16> FILTER_S1     = config[16];
  ap_uint<16> FILTER_S2     = config[17];
  ap_uint<32> STRIDE        = config[18];

  // inst3
  ap_uint<32> LAYER_EN        = config[19];
  ap_uint<32> LAYER_IN_NUM_T  = config[20];
  ap_uint<32> LAYER_OUT_NUM_T = config[21]; 
  ap_uint<32> LAYER_IN_H_T    = config[22]; 
  ap_uint<32> LAYER_IN_W_T    = config[23]; 

  ap_uint<1>  DEPTH_CONV_EN = LAYER_EN[0];
  ap_uint<1>  CONV_EN       = LAYER_EN[1];
  ap_uint<1>  RELU_EN       = LAYER_EN[2];
  ap_uint<1>  POOL_EN       = LAYER_EN[3];
  ap_uint<1>  UP_SAMPLE_EN  = LAYER_EN[4];  // reserved
  ap_uint<1>  LD_SEL        = LAYER_EN[5];  // reserved
  ap_uint<1>  WR_SEL        = LAYER_EN[6];  // reserved

  data_t0* layer_cout = new data_t0[LAYER_OUT_NUM * LAYER_OUT_H * LAYER_OUT_W];
  uint cout_offset = COUT_OFFSET;

  // extract input
  ofstream input_file("debug_input.dat");
  if (input_file.is_open()){
    for (int o1 = 0; o1 < LAYER_IN_NUM_HW / LAYER_IN_NUM_T; o1++)
      for (int h = 0; h < LAYER_IN_H_T + FILTER_S1 - 1; h++)
        for (int w = 0; w < LAYER_IN_W_T + FILTER_S1 - 1; w++)
          for (int o2 = 0; o2 < LAYER_IN_NUM_T; o2++){
            uint o = o1 * LAYER_IN_NUM_T + o2;
            uint global_cin_idx = o1 * LAYER_IN_H_HW * LAYER_IN_W_HW * LAYER_IN_NUM_T + h * LAYER_IN_W_HW * LAYER_IN_NUM_T + w * LAYER_IN_NUM_T + o2 + CIN_OFFSET;
            if (o == 0){
              input_file << cin_hw[global_cin_idx] << endl;
            }
          }
    input_file.close();
  }

  // write out to files
  cout << "extract offset: " << cout_offset << endl;
  cout << cout_offset + 1 << ": " << cin_hw[cout_offset + 1] << endl;
  cout << cout_offset + 32 << ": " << cin_hw[cout_offset + 32] << endl;
  cout << LAYER_OUT_NUM_HW << " " << LAYER_OUT_NUM_T << " " << endl;
  ofstream output_file("debug.dat");
  if (output_file.is_open()){
    for (int o1 = 0; o1 < LAYER_OUT_NUM_HW / LAYER_OUT_NUM_T; o1++)
      for (int h = 0; h < LAYER_OUT_H; h++)
        for (int w = 0; w < LAYER_OUT_W; w++)
          for (int o2 = 0; o2 < LAYER_OUT_NUM_T; o2++){
            uint o = o1 * LAYER_OUT_NUM_T + o2;
            uint global_cout_idx = o1 * LAYER_OUT_H_HW * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + 
              h * LAYER_OUT_W_HW * LAYER_OUT_NUM_T + w * LAYER_OUT_NUM_T + o2 + cout_offset;
            uint local_cout_idx = o * LAYER_OUT_H * LAYER_OUT_W + h * LAYER_OUT_W + w;
#ifdef DEBUG            
            if (h == 0 && w == 1 && o == 0){
              cout << global_cout_idx << " " << local_cout_idx << endl;
            }
#endif              
            if (o < LAYER_OUT_NUM){
              layer_cout[local_cout_idx] = cin_hw[global_cout_idx];
            }
          }

    cout << "extract_layer: " << LAYER_OUT_H << " " << LAYER_OUT_W << " " << LAYER_OUT_NUM << endl;

    for (int h = 0; h < LAYER_OUT_H; h++)
      for (int w = 0; w < LAYER_OUT_W; w++)
        for (int o = 0; o < LAYER_OUT_NUM; o++){
          uint local_cout_idx = o * LAYER_OUT_H * LAYER_OUT_W + h * LAYER_OUT_W + w;
          output_file << layer_cout[local_cout_idx] << endl;
        }
  } else {
    cout << "Output open failed!" << endl;
    exit(-1);
  }

  delete[] layer_cout;
}

void compute_layer(
){

  // Layer 1
  char* prj_path_c = getenv("PRJ_PATH");  

  // Prepare the software buffers
  cout << std::fixed << "Preparing data..." << endl;
  
  // Load the inputs for the network
  static data_t0 LAYER1_cin[LAYER1_IN_NUM][LAYER1_IN_H][LAYER1_IN_W];
  cout << "Loading input..." << endl; 
  string file_path = string(prj_path_c) + "/data/input.dat";  
  ifstream input_file(file_path.c_str());

  if (input_file.is_open()){
    for (int h = 0; h < LAYER1_IN_H; h++)
      for (int w = 0; w < LAYER1_IN_W; w++)
        for (int i = 0; i < LAYER1_IN_NUM; i++)
          input_file >> LAYER1_cin[i][h][w];
    input_file.close();
  } else {
    cout << "Input open failed!" << endl;
    exit(-1);
  }
  
  // Load weights
  static data_t1 LAYER1_weight[K_T][K_T][LAYER1_IN_NUM][LAYER1_OUT_NUM];
  static data_t1 LAYER2_weight1[K_T][K_T][LAYER2_IN_NUM];
  static data_t1 LAYER2_weight2[1][1][LAYER2_IN_NUM][LAYER2_OUT_NUM];

  static data_t1 LAYER3_weight1[K_T][K_T][LAYER3_IN_NUM];
  static data_t1 LAYER3_weight2[1][1][LAYER3_IN_NUM][LAYER3_OUT_NUM];
  static data_t1 LAYER4_weight1[K_T][K_T][LAYER4_IN_NUM];
  static data_t1 LAYER4_weight2[1][1][LAYER4_IN_NUM][LAYER4_OUT_NUM];
  static data_t1 LAYER5_weight1[K_T][K_T][LAYER5_IN_NUM];
  static data_t1 LAYER5_weight2[1][1][LAYER5_IN_NUM][LAYER5_OUT_NUM];

  cout << "Loading weight..." << endl;
  file_path = string(prj_path_c) + "/data/weight.dat";  
  ifstream weight_file(file_path.c_str());

  if (weight_file.is_open()){
    for (int p = 0; p < LAYER1_K; p++)
      for (int q = 0; q < LAYER1_K; q++)
        for (int i = 0; i < LAYER1_IN_NUM; i++)
          for (int o = 0; o < LAYER1_OUT_NUM; o++){
            weight_file >> LAYER1_weight[p][q][i][o];
          }
    for (int p = 0; p < LAYER2_K; p++)
      for (int q = 0; q < LAYER2_K; q++)
        for (int i = 0; i < LAYER2_IN_NUM; i++)
          weight_file >> LAYER2_weight1[p][q][i];
    for (int p = 0; p < 1; p++)
      for (int q = 0; q < 1; q++)
        for (int i = 0; i < LAYER2_IN_NUM; i++)
          for (int o = 0; o < LAYER2_OUT_NUM; o++){
            weight_file >> LAYER2_weight2[p][q][i][o];
//            cout << "weight loading: " << LAYER2_weight2[p][q][i][o] << endl;
          }
    for (int p = 0; p < LAYER3_K; p++)
      for (int q = 0; q < LAYER3_K; q++)
        for (int i = 0; i < LAYER3_IN_NUM; i++)
          weight_file >> LAYER3_weight1[p][q][i];
    for (int p = 0; p < 1; p++)
      for (int q = 0; q < 1; q++)
        for (int i = 0; i < LAYER3_IN_NUM; i++)
          for (int o = 0; o < LAYER3_OUT_NUM; o++){
            weight_file >> LAYER3_weight2[p][q][i][o];
//            cout << "weight loading: " << LAYER2_weight2[p][q][i][o] << endl;
          }
    for (int p = 0; p < LAYER4_K; p++)
      for (int q = 0; q < LAYER4_K; q++)
        for (int i = 0; i < LAYER4_IN_NUM; i++)
          weight_file >> LAYER4_weight1[p][q][i];
    for (int p = 0; p < 1; p++)
      for (int q = 0; q < 1; q++)
        for (int i = 0; i < LAYER4_IN_NUM; i++)
          for (int o = 0; o < LAYER4_OUT_NUM; o++){
            weight_file >> LAYER4_weight2[p][q][i][o];
//            cout << "weight loading: " << LAYER4_weight2[p][q][i][o] << endl;
          }
     for (int p = 0; p < LAYER5_K; p++)
      for (int q = 0; q < LAYER5_K; q++)
        for (int i = 0; i < LAYER5_IN_NUM; i++)
          weight_file >> LAYER5_weight1[p][q][i];
    for (int p = 0; p < 1; p++)
      for (int q = 0; q < 1; q++)
        for (int i = 0; i < LAYER5_IN_NUM; i++)
          for (int o = 0; o < LAYER5_OUT_NUM; o++){
            weight_file >> LAYER5_weight2[p][q][i][o];
//            cout << "weight loading: " << LAYER5_weight2[p][q][i][o] << endl;
          }
 
    weight_file.close();
  } else {
    cout << "Weight open failed!" << endl;
    exit(-1);
  }

  // Load bias
  static data_t2 LAYER1_bias[LAYER1_OUT_NUM];
  static data_t2 LAYER2_bias[LAYER2_OUT_NUM];
  static data_t2 LAYER3_bias[LAYER3_OUT_NUM];
  static data_t2 LAYER4_bias[LAYER4_OUT_NUM];
  static data_t2 LAYER5_bias[LAYER5_OUT_NUM];
  cout << "Loading bias..." << endl;
  file_path = string(prj_path_c) +  "/data/bias.dat";
  ifstream bias_file(file_path.c_str());

  if (bias_file.is_open()){
    for (int w = 0; w < LAYER1_OUT_NUM; w++){
      bias_file >> LAYER1_bias[w];
    }
    for (int w = 0; w < LAYER2_OUT_NUM; w++){
      bias_file >> LAYER2_bias[w];
    }
    for (int w = 0; w < LAYER3_OUT_NUM; w++){
      bias_file >> LAYER3_bias[w];
    }
    for (int w = 0; w < LAYER4_OUT_NUM; w++){
      bias_file >> LAYER4_bias[w];
    }
    for (int w = 0; w < LAYER5_OUT_NUM; w++){
      bias_file >> LAYER5_bias[w];
    }
    bias_file.close();
  } else {
    cout << "Bias open failed!" << endl;
    exit(-1);
  }

  // compute
  static data_t0 LAYER1_cout[LAYER1_OUT_NUM][LAYER1_OUT_H][LAYER1_OUT_W];
  for (int o = 0; o < LAYER1_OUT_NUM; o++)
    for (int h = 0; h < LAYER1_OUT_H; h++)
      for (int w = 0; w < LAYER1_OUT_W; w++){
        LAYER1_cout[o][h][w] = 0;
        
        for (int i = 0; i < LAYER1_IN_NUM; i++)
          for (int p = 0; p < LAYER1_K; p++)
            for (int q = 0; q < LAYER1_K; q++){
              uint shift_h = h * 2 + p - 1 + 1;
              uint shift_w = w * 2 + q - 1 + 1;
              data_t0 cin_compute = 0;
              if (shift_h < 0 || shift_h >= LAYER1_IN_H || shift_w < 0 || shift_w >= LAYER1_IN_W)
                cin_compute = 0;
              else
                cin_compute = LAYER1_cin[i][shift_h][shift_w];
              LAYER1_cout[o][h][w] += cin_compute * LAYER1_weight[p][q][i][o];
            }
        LAYER1_cout[o][h][w] = max(0, LAYER1_cout[o][h][w] + LAYER1_bias[o]);
//        LAYER1_cout[o][h][w] = LAYER1_cout[o][h][w] + LAYER1_bias[o];
      }

  static data_t0 LAYER1_conv_cout[LAYER1_OUT_NUM][LAYER1_IN_H][LAYER1_IN_W];
  for (int o = 0; o < LAYER1_OUT_NUM; o++)
    for (int h = 0; h < LAYER1_IN_H; h++)
      for (int w = 0; w < LAYER1_IN_W; w++){
        LAYER1_conv_cout[o][h][w] = 0;
        
        for (int i = 0; i < LAYER1_IN_NUM; i++)
          for (int p = 0; p < LAYER1_K; p++)
            for (int q = 0; q < LAYER1_K; q++){
              uint shift_h = h + p - 1;
              uint shift_w = w + q - 1;
              data_t0 cin_compute = 0;
              if (shift_h < 0 || shift_h >= LAYER1_IN_H || shift_w < 0 || shift_w >= LAYER1_IN_W)
                cin_compute = 0;
              else
                cin_compute = LAYER1_cin[i][shift_h][shift_w];
              LAYER1_conv_cout[o][h][w] += cin_compute * LAYER1_weight[p][q][i][o];
            }
//        LAYER1_cout[o][h][w] = max(0, LAYER1_cout[o][h][w] + LAYER1_bias[o]);
//        LAYER1_cout[o][h][w] = LAYER1_cout[o][h][w] + LAYER1_bias[o];
      }

  // Layer 2
  static data_t0 LAYER2_depth_cout[LAYER2_IN_NUM][LAYER2_IN_H][LAYER2_IN_W];
  static data_t0 LAYER2_point_conv_cout[LAYER2_OUT_NUM][LAYER2_OUT_H][LAYER2_OUT_W];
  static data_t0 LAYER2_point_cout[LAYER2_OUT_NUM][LAYER2_OUT_H][LAYER2_OUT_W];
  for (int o = 0; o < LAYER2_IN_NUM; o++)
    for (int h = 0; h < LAYER2_IN_H; h++)
      for (int w = 0; w < LAYER2_IN_W; w++){
        LAYER2_depth_cout[o][h][w] = 0;
        for (int p = 0; p < LAYER2_K; p++)
          for (int q = 0; q < LAYER2_K; q++){
            uint shift_h = h + p - 1;
            uint shift_w = w + q - 1;
            data_t0 cin_compute = 0;
            if (shift_h < 0 || shift_h >= LAYER2_IN_H || shift_w < 0 || shift_w >= LAYER2_IN_W)
              cin_compute = 0;
            else
              cin_compute = LAYER1_cout[o][shift_h][shift_w];
            LAYER2_depth_cout[o][h][w] += cin_compute * LAYER2_weight1[p][q][o];
          }
      }

  for (int o = 0; o < LAYER2_OUT_NUM; o++)
    for (int h = 0; h < LAYER2_IN_H; h++)
      for (int w = 0; w < LAYER2_IN_W; w++){
        LAYER2_point_conv_cout[o][h][w] = 0;
        
        for (int i = 0; i < LAYER2_IN_NUM; i++)
          for (int p = 0; p < 1; p++)
            for (int q = 0; q < 1; q++){
              uint shift_h = h + p - 0;
              uint shift_w = w + q - 0;
              data_t0 cin_compute = 0;
              if (shift_h < 0 || shift_h >= LAYER2_IN_H || shift_w < 0 || shift_w >= LAYER2_IN_W)
                cin_compute = 0;
              else
                cin_compute = LAYER2_depth_cout[i][shift_h][shift_w];
              LAYER2_point_conv_cout[o][h][w] += cin_compute * LAYER2_weight2[p][q][i][o];
            }
        LAYER2_point_cout[o][h][w] = max(0, LAYER2_point_conv_cout[o][h][w] + LAYER2_bias[o]);
//        LAYER1_cout[o][h][w] = LAYER1_cout[o][h][w] + LAYER1_bias[o];
      }

  // Layer 3
  static data_t0 LAYER3_depth_cout[LAYER3_IN_NUM][LAYER3_IN_H][LAYER3_IN_W];
  static data_t0 LAYER3_point_conv_cout[LAYER3_OUT_NUM][LAYER3_OUT_H][LAYER3_OUT_W];
  static data_t0 LAYER3_point_cout[LAYER3_OUT_NUM][LAYER3_OUT_H][LAYER3_OUT_W];
  for (int o = 0; o < LAYER3_IN_NUM; o++)
    for (int h = 0; h < LAYER3_IN_H; h++)
      for (int w = 0; w < LAYER3_IN_W; w++){
        LAYER3_depth_cout[o][h][w] = 0;
        for (int p = 0; p < LAYER3_K; p++)
          for (int q = 0; q < LAYER3_K; q++){
            uint shift_h = h + p - 1;
            uint shift_w = w + q - 1;
            data_t0 cin_compute = 0;
            if (shift_h < 0 || shift_h >= LAYER3_IN_H || shift_w < 0 || shift_w >= LAYER3_IN_W)
              cin_compute = 0;
            else
              cin_compute = LAYER2_point_cout[o][shift_h][shift_w];
            LAYER3_depth_cout[o][h][w] += cin_compute * LAYER3_weight1[p][q][o];
          }
      }

  for (int o = 0; o < LAYER3_OUT_NUM; o++)
    for (int h = 0; h < LAYER3_OUT_H; h++)
      for (int w = 0; w < LAYER3_OUT_W; w++){
        LAYER3_point_conv_cout[o][h][w] = 0;
        
        for (int i = 0; i < LAYER3_IN_NUM; i++)
          for (int p = 0; p < 1; p++)
            for (int q = 0; q < 1; q++){
              uint shift_h = 2 * h + 1 + p - 0;
              uint shift_w = 2 * w + 1 + q - 0;
              data_t0 cin_compute = 0;
              if (shift_h < 0 || shift_h >= LAYER3_IN_H || shift_w < 0 || shift_w >= LAYER3_IN_W)
                cin_compute = 0;
              else
                cin_compute = LAYER3_depth_cout[i][shift_h][shift_w];
              LAYER3_point_conv_cout[o][h][w] += cin_compute * LAYER3_weight2[p][q][i][o];
            }
        LAYER3_point_cout[o][h][w] = max(0, LAYER3_point_conv_cout[o][h][w] + LAYER3_bias[o]);
//        LAYER1_cout[o][h][w] = LAYER1_cout[o][h][w] + LAYER1_bias[o];
      }

  // Layer 4
  static data_t0 LAYER4_depth_cout[LAYER4_IN_NUM][LAYER4_IN_H][LAYER4_IN_W];
  static data_t0 LAYER4_point_conv_cout[LAYER4_OUT_NUM][LAYER4_OUT_H][LAYER4_OUT_W];
  static data_t0 LAYER4_point_cout[LAYER4_OUT_NUM][LAYER4_OUT_H][LAYER4_OUT_W];
  for (int o = 0; o < LAYER4_IN_NUM; o++)
    for (int h = 0; h < LAYER4_IN_H; h++)
      for (int w = 0; w < LAYER4_IN_W; w++){
        LAYER4_depth_cout[o][h][w] = 0;
        for (int p = 0; p < LAYER4_K; p++)
          for (int q = 0; q < LAYER4_K; q++){
            uint shift_h = h + p - 1;
            uint shift_w = w + q - 1;
            data_t0 cin_compute = 0;
            if (shift_h < 0 || shift_h >= LAYER4_IN_H || shift_w < 0 || shift_w >= LAYER4_IN_W)
              cin_compute = 0;
            else
              cin_compute = LAYER3_point_cout[o][shift_h][shift_w];
            LAYER4_depth_cout[o][h][w] += cin_compute * LAYER4_weight1[p][q][o];
          }
      }

  for (int o = 0; o < LAYER4_OUT_NUM; o++)
    for (int h = 0; h < LAYER4_OUT_H; h++)
      for (int w = 0; w < LAYER4_OUT_W; w++){
        LAYER4_point_conv_cout[o][h][w] = 0;
        
        for (int i = 0; i < LAYER4_IN_NUM; i++)
          for (int p = 0; p < 1; p++)
            for (int q = 0; q < 1; q++){
              uint shift_h = 1 * h + p - 0;
              uint shift_w = 1 * w + q - 0;
              data_t0 cin_compute = 0;
              if (shift_h < 0 || shift_h >= LAYER4_IN_H || shift_w < 0 || shift_w >= LAYER4_IN_W)
                cin_compute = 0;
              else
                cin_compute = LAYER4_depth_cout[i][shift_h][shift_w];
              LAYER4_point_conv_cout[o][h][w] += cin_compute * LAYER4_weight2[p][q][i][o];
            }
        LAYER4_point_cout[o][h][w] = max(0, LAYER4_point_conv_cout[o][h][w] + LAYER4_bias[o]);
//        LAYER1_cout[o][h][w] = LAYER1_cout[o][h][w] + LAYER1_bias[o];
      }

  // conv_3_pool
  static data_t0 LAYER4_pool_cout[LAYER4_OUT_NUM][LAYER4_OUT_H / 2][LAYER4_OUT_W / 2];
  for (int o = 0; o < LAYER4_OUT_NUM; o++)
    for (int h = 0; h < LAYER4_OUT_H / 2; h++)
      for (int w = 0; w < LAYER4_OUT_W / 2; w++){
        data_t0 tmp1 = LAYER4_point_cout[o][h * 2][w * 2];
        data_t0 tmp2 = LAYER4_point_cout[o][h * 2][w * 2 + 1];
        data_t0 tmp3 = LAYER4_point_cout[o][h * 2 + 1][w * 2];
        data_t0 tmp4 = LAYER4_point_cout[o][h * 2 + 1][w * 2 + 1];
        data_t0 max1 = max(tmp1, tmp2);
        data_t0 max2 = max(tmp3, tmp4);
        LAYER4_pool_cout[o][h][w] = max(max1, max2);
      }
  
  // Layer 5
  static data_t0 LAYER5_depth_cout[LAYER5_IN_NUM][LAYER5_IN_H][LAYER5_IN_W];
  static data_t0 LAYER5_depth_stride_cout[LAYER5_IN_NUM][LAYER5_OUT_H][LAYER5_OUT_W];
  static data_t0 LAYER5_point_conv_cout[LAYER5_OUT_NUM][LAYER5_OUT_H][LAYER5_OUT_W];
  static data_t0 LAYER5_point_cout[LAYER5_OUT_NUM][LAYER5_OUT_H][LAYER5_OUT_W];
  static data_t0 LAYER5_point_stride_cout[LAYER5_OUT_NUM][LAYER5_OUT_H][LAYER5_OUT_W];
  for (int o = 0; o < LAYER5_IN_NUM; o++)
    for (int h = 0; h < LAYER5_IN_H; h++)
      for (int w = 0; w < LAYER5_IN_W; w++){
        LAYER5_depth_cout[o][h][w] = 0;
        for (int p = 0; p < LAYER5_K; p++)
          for (int q = 0; q < LAYER5_K; q++){
            uint shift_h = h + p - 1;
            uint shift_w = w + q - 1;
            data_t0 cin_compute = 0;
            if (shift_h < 0 || shift_h >= LAYER5_IN_H || shift_w < 0 || shift_w >= LAYER5_IN_W)
              cin_compute = 0;
            else
              cin_compute = LAYER4_point_cout[o][shift_h][shift_w];
            LAYER5_depth_cout[o][h][w] += cin_compute * LAYER5_weight1[p][q][o];
          }
      }
  for (int o = 0; o < LAYER5_IN_NUM; o++)
    for (int h = 0; h < LAYER5_OUT_H; h++)
      for (int w = 0; w < LAYER5_OUT_W; w++){
        LAYER5_depth_stride_cout[o][h][w] = 0;
        for (int p = 0; p < LAYER5_K; p++)
          for (int q = 0; q < LAYER5_K; q++){
            uint shift_h = 2 * h + 1 + p - 1;
            uint shift_w = 2 * w + 1 + q - 1;
            data_t0 cin_compute = 0;
            if (shift_h < 0 || shift_h >= LAYER5_IN_H || shift_w < 0 || shift_w >= LAYER5_IN_W)
              cin_compute = 0;
            else
              cin_compute = LAYER4_point_cout[o][shift_h][shift_w];
            LAYER5_depth_stride_cout[o][h][w] += cin_compute * LAYER5_weight1[p][q][o];
          }
      }

  for (int o = 0; o < LAYER5_OUT_NUM; o++)
    for (int h = 0; h < LAYER5_OUT_H; h++)
      for (int w = 0; w < LAYER5_OUT_W; w++){
        LAYER5_point_conv_cout[o][h][w] = 0;
        
        for (int i = 0; i < LAYER5_IN_NUM; i++)
          for (int p = 0; p < 1; p++)
            for (int q = 0; q < 1; q++){
              uint shift_h = 2 * h + 1 + p - 0;
              uint shift_w = 2 * w + 1 + q - 0;
              data_t0 cin_compute = 0;
              if (shift_h < 0 || shift_h >= LAYER5_IN_H || shift_w < 0 || shift_w >= LAYER5_IN_W)
                cin_compute = 0;
              else
                cin_compute = LAYER5_depth_cout[i][shift_h][shift_w];
              LAYER5_point_conv_cout[o][h][w] += cin_compute * LAYER5_weight2[p][q][i][o];
            }
        LAYER5_point_cout[o][h][w] = max(0, LAYER5_point_conv_cout[o][h][w] + LAYER5_bias[o]);
//        LAYER1_cout[o][h][w] = LAYER1_cout[o][h][w] + LAYER1_bias[o];
      }
  for (int o = 0; o < LAYER5_OUT_NUM; o++)
    for (int h = 0; h < LAYER5_OUT_H; h++)
      for (int w = 0; w < LAYER5_OUT_W; w++){
        LAYER5_point_stride_cout[o][h][w] = 0;
        
        for (int i = 0; i < LAYER5_IN_NUM; i++)
          for (int p = 0; p < 1; p++)
            for (int q = 0; q < 1; q++){
              uint shift_h = h + p - 0;
              uint shift_w = w + q - 0;
              data_t0 cin_compute = 0;
              if (shift_h < 0 || shift_h >= LAYER5_OUT_H || shift_w < 0 || shift_w >= LAYER5_OUT_W)
                cin_compute = 0;
              else
                cin_compute = LAYER5_depth_stride_cout[i][shift_h][shift_w];
              LAYER5_point_stride_cout[o][h][w] += cin_compute * LAYER5_weight2[p][q][i][o];
            }
        LAYER5_point_stride_cout[o][h][w] = max(0, LAYER5_point_stride_cout[o][h][w] + LAYER5_bias[o]);
//        LAYER1_cout[o][h][w] = LAYER1_cout[o][h][w] + LAYER1_bias[o];
      }

#ifdef DEBUG
  ofstream conv_debug("conv_patch.dat");
  for (int h = 0; h < OUT_H_T; h++)
    for (int w = 0; w < OUT_W_T; w++){
      if (h >= LAYER5_IN_H || w >= LAYER5_IN_W)
        conv_debug << 0 << endl;
      else
        conv_debug << LAYER5_point_conv_cout[0][h][w] << endl;
    }
#endif  

#ifdef DEBUG
  ofstream cin_debug("cin_patch.dat");
//  for (int h = 0; h < IN_H_T + LAYER5_K - 1; h++)
//    for (int w = 0; w < IN_W_T + LAYER5_K - 1; w++){
//      if (h < 1 || w < 1 || h > IN_H_T || w > IN_W_T){
//        cin_debug << 0 << endl;
//      } else {
//        cin_debug << LAYER4_point_cout[0][h - 1][w - 1] << endl;
//      }
//    }
  for (int h = 0; h < IN_H_T; h++)
    for (int w = 0; w < IN_W_T; w++){
      if (h >= LAYER4_OUT_H || w >= LAYER4_OUT_W){
        cin_debug << 0 << endl;
      } else {
        cin_debug << LAYER4_point_cout[1][h][w] << endl;
      }
    }
 
  cin_debug.close();

  ofstream cin_debug2("point_cin_patch.dat");
  for (int h = 0; h < OUT_H_T; h++)
    for (int w = 0; w < OUT_W_T; w++){
      if (h >= LAYER5_IN_H || w >= LAYER5_IN_W)
        cin_debug2 << 0 << endl;
      else
        cin_debug2 << LAYER5_depth_cout[2][h][w] << endl;
    }
#endif  

#ifdef DEBUG
  for (int p = 0; p < LAYER5_K; p++)
    for (int q = 0; q < LAYER5_K; q++){
      cout << "compute layers (weight1): " << LAYER5_weight1[p][q][0] << endl;
    }

  for (int i = 0; i < LAYER5_IN_NUM; i++)
    for (int p = 0; p < 1; p++)
      for (int q = 0; q < 1; q++){
        cout << "compute layers (weight2): " << LAYER5_weight2[p][q][i][0] << endl;
      }
#endif
#ifdef DEBUG
  cout << "compute_layers (bias): " << LAYER5_bias[0] << endl;
#endif  
#ifdef DEBUG
  ofstream output_file0("sw_layer4_pool_output.dat");
  if (output_file0.is_open()){
    for (int h = 0; h < LAYER4_OUT_H / 2; h++)
      for (int w = 0; w < LAYER4_OUT_W / 2; w++)
        for (int o = 0; o < LAYER4_OUT_NUM; o++){
          output_file0 << LAYER4_pool_cout[o][h][w] << endl;
        }
  }

  ofstream output_file1("sw_layer5_depth_output.dat");
  if (output_file1.is_open()){
    for (int h = 0; h < LAYER5_IN_H; h++)
      for (int w = 0; w < LAYER5_IN_W; w++)
        for (int o = 0; o < LAYER5_IN_NUM; o++){
          output_file1 << LAYER5_depth_cout[o][h][w] << endl;
        }
  }
  ofstream output_file3("sw_layer5_depth_stride_output.dat");
  if (output_file3.is_open()){
    for (int h = 0; h < LAYER5_OUT_H; h++)
      for (int w = 0; w < LAYER5_OUT_W; w++)
        for (int o = 0; o < LAYER5_IN_NUM; o++){
          output_file3 << LAYER5_depth_stride_cout[o][h][w] << endl;
        }
  }
 
  ofstream output_file2("sw_layer5_point_output.dat");
  if (output_file2.is_open()){
    for (int h = 0; h < LAYER5_OUT_H; h++)
      for (int w = 0; w < LAYER5_OUT_W; w++)
        for (int o = 0; o < LAYER5_OUT_NUM; o++){
          output_file2 << LAYER5_point_cout[o][h][w] << endl;
        }
  }

  ofstream output_file4("sw_layer5_point_stride_output.dat");
  if (output_file4.is_open()){
    for (int h = 0; h < LAYER5_OUT_H; h++)
      for (int w = 0; w < LAYER5_OUT_W; w++)
        for (int o = 0; o < LAYER5_OUT_NUM; o++){
          output_file4 << LAYER5_point_stride_cout[o][h][w] << endl;
        }
  }
 
#endif  
//  cout << LAYER1_cout[0][0][0] << endl;

}


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
  //string file_path = string(prj_path_c) + "/data_layer/input.dat";  
  string file_path = string(prj_path_c) + "/data/conv0_input.dat"; 
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
    //weight_file.read(bin_input, sizeof(data_t1) * WEIGHT_SIZE);
    //data_t1* convt_input = (data_t1*)bin_input;

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
  file_path = string(prj_path_c) + "/data/stage6_l2_5.dat";
  //file_path = string(prj_path_c) + "/data_layer/output.dat";
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
