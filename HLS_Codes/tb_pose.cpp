#include "pose.h"

// Loads instructions
void instInit(
  uint* config
){
  cout << "Loading instructions..." << endl;
  char* prj_path_c = getenv("PRJ_PATH");
  string prj_path = prj_path_c;
  string file_path = prj_path + "/auto_compile/inst_gen/network.insts";
  ifstream in_file(file_path.c_str());
  
  // model configuration
  config[0] = LAYER_NUM;
 
  if (in_file.is_open()){
    for (int layer_id = 0; layer_id < LAYER_NUM; layer_id++){
      uint p;
      int param_cnt = 0;
      while(param_cnt < CONFIG_PARAMS){
        in_file >> p;
        config[5 + layer_id * CONFIG_PARAMS + param_cnt] = p;
//        if (layer_id == 0){
//          cout << p << endl;
//        }
        param_cnt++;
      }
    }
    in_file.close();
  } else {
    cout << "CONFIG open failed!" << endl;
    exit(-1);
  }
}

int main(){
  // working path
  char* prj_path = getenv("PRJ_PATH");
  if (prj_path != NULL){
    cout << "Your working PATH is: " << prj_path << endl;
  } else {
    cout << "Working PATH not set!" << endl;
    return -1;
  }
  
  unsigned int cin_size = CIN_SIZE;
  unsigned int bias_size = BIAS_SIZE;
  unsigned int weight_size = WEIGHT_SIZE;
  unsigned int config_size = 5 + LAYER_NUM * CONFIG_PARAMS;

  cout << "cin_size: " << cin_size << endl;
  cout << "bias_size: " << bias_size << endl;
  cout << "weight_size: " << weight_size <<endl;
 
  data_t0* cin_hw = new data_t0[cin_size];
  data_t0* cin_hw2 = new data_t0[cin_size];
  data_t1* weight_hw = new data_t1[weight_size];
  data_t2* bias_hw = new data_t2[bias_size];

  memset(cin_hw, 0, cin_size);
  memset(weight_hw, 0, weight_size);
  memset(bias_hw, 0, bias_size);

  // Load instructions 
  uint* config = new uint[config_size];
  instInit(config);
  
  

  data_t0 LAYER_out_sw[STAGE2L_OUT_H][STAGE2L_OUT_W][STAGE2R_OUT_NUM + STAGE2L_OUT_NUM];
  data_t0 LAYER_out_hw[STAGE2L_OUT_H][STAGE2L_OUT_W][STAGE2R_OUT_NUM + STAGE2L_OUT_NUM];
  data_t0 LAYER_out[out_h2][out_w2][out_num2];

  
  mobilenet_preprocess(cin_hw, weight_hw, bias_hw, LAYER_out_sw);


  cout << "HW acceleration..." << endl;


  // Hardware acceleration
  top_kernel(
      (bus_t0*)cin_hw, (bus_t0*)cin_hw, (bus_t0*)cin_hw,
      (bus_t1*)weight_hw, (bus_t2*)bias_hw,
      (bus_t3*)config);


  int size = 24*24*32;
  
  
  
  for (int h = 0; h < out_h2; h++)
    for (int w = 0; w < out_w2; w++)
      for (int o = 0; o < out_num2; o++){
        data_t0 hw_result = LAYER_out[h][w][o];
      }
	  
  

  // Extract hardware outputs
  openpose_postprocess(cin_hw, LAYER_out_hw);   

  // Results comparison
  cout << "Results comparison..." << endl;
  int err_cnt = 0;
  bool test = true;
  if(test){
  for (int h = 0; h < STAGE2L_OUT_H; h++)
    for (int w = 0; w < STAGE2L_OUT_W; w++)
      for (int o = 0; o < STAGE2L_OUT_NUM + STAGE2R_OUT_NUM; o++){
        data_t0 sw_result = LAYER_out_sw[h][w][o];
        data_t0 hw_result = LAYER_out_hw[h][w][o];
        if (abs(sw_result - hw_result) > 0.001){
          err_cnt++;
          cout << "Mismatch: STAGE2(" << o << "," << h << "," << w << ") sw: " << sw_result << " hw: " << hw_result << endl;
        } else {
//          cout << "Match: STAGE2(" << o << "," << h << "," << w << ") sw: " << sw_result << " hw: " << hw_result << endl;
        }
      }
  }
  delete[] cin_hw;
  delete[] weight_hw;
  delete[] bias_hw;
  delete[] config;

  if (err_cnt > 0){
    cout << "Failed! " << err_cnt << " errors!" << endl;
    return -1;
  } else {
    cout << "Success!" << endl;
    return 0;
  }
}
