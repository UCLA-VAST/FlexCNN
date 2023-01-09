#include "cnn_sw.h"
#include <iomanip>

float random_float()
{
  int r_int = rand() % 65536;
  return (r_int - 32768.0) / 32768.0;
}
union charToInt
{
  unsigned char uc_[4];
  unsigned int ui_;
};

void instInit(vector<uint> &config){
  string prj_path = string(getenv("PRJ_PATH"));
  string file_path = prj_path +"/data/instructions.dat";
  ifstream in_file(file_path.c_str());
  config[0] = LAYER_NUM;
  config[1] = 0;
  config[2] = 0;
  config[3] = 0;
  config[4] = 0;
  if (in_file.is_open()){
    for (int layer_id = 0; layer_id < LAYER_NUM; layer_id++){
      uint p;
      int param_cnt = 0;
      while(param_cnt < CONFIG_PARAMS){
        in_file >> p;
        config[5+layer_id*CONFIG_PARAMS+param_cnt] = p;
        // cout<<p<<" ";
        param_cnt++;
      }
      // cout<<endl;
      // exit(0);
      // string oct1;
      // string oct2;
      // in_file >> oct1;
      // in_file >> oct2;
      // unsigned long long int oct_num1 = strtol(oct1.c_str(), NULL, 8);
      // unsigned long long int oct_num2 = strtol(oct2.c_str(), NULL, 8);

      // config[5 + layer_id * CONFIG_PARAMS + param_cnt + 0] = oct_num1>>16;
      // config[5 + layer_id * CONFIG_PARAMS + param_cnt + 1] = ((oct_num1<<48)>>32) + (oct_num2>>32);
      // config[5 + layer_id * CONFIG_PARAMS + param_cnt + 2] =  oct_num2 & 0xFFFFFFFF;

      // cout<<config[5 + layer_id * CONFIG_PARAMS + param_cnt + 0]<<endl;
      // cout<<config[5 + layer_id * CONFIG_PARAMS + param_cnt + 1]<<endl;
      // cout<<config[5 + layer_id * CONFIG_PARAMS + param_cnt + 2]<<endl;

    }
    in_file.close();
  } else {
    cout << "CONFIG open failed!" << file_path << endl;
    exit(-1);
  }
}

void getInsts(aligned_vector<uint> &config, int start_inst, int end_inst){
  string prj_path = string(getenv("PRJ_PATH"));
  string file_path = prj_path +"/data/instructions.dat";
  ifstream in_file(file_path.c_str());
  if (in_file.is_open()){
    for (int layer_id = 0; layer_id < end_inst; layer_id++){
      uint p;
      int param_cnt = 0;
      while(param_cnt < CONFIG_PARAMS){
        in_file >> p;
        config[layer_id * CONFIG_PARAMS + param_cnt] = p;
        param_cnt++;
      }
    }
    in_file.close();
  } else {
    cout << "CONFIG open failed!" << file_path << endl;
    exit(-1);
  }
  // for (int layer_id = 0; layer_id < end_inst; layer_id++){
  //   for (int param_id = 0; param_id < CONFIG_PARAMS; param_id++){
  //     cout<<config[layer_id*CONFIG_PARAMS+param_id]<<" ";
  //   }
  //   cout<<endl;
  // }
  // exit(0);
}

void preprocess(
  aligned_vector<data_t0> &cin_hw,
  aligned_vector<data_t1> &weight_hw,
  aligned_vector<data_t2> &bias_hw,
  uint in_num_hw,
  uint out_num_hw,
  uint in_h_hw,
  uint in_w_hw,
  uint kernel_h,
  uint kernel_w
){

  string prj_path = string(getenv("PRJ_PATH"));
  string file_path = prj_path +"/data/inputs_1.dat";
  ifstream input1_file(file_path.c_str());
  if (input1_file.is_open()){
    for (int i = 0; i < in_num_hw*in_h_hw*in_w_hw; i++){
      input1_file >> cin_hw[i];
    }
    input1_file.close();
  } else {
    cout << "Input1 open failed!" << file_path <<endl;
    exit(-1);
  }

  file_path = prj_path +"/data/inputs_2.dat";
  ifstream input2_file(file_path.c_str());
  if (input2_file.is_open()){
    for (int i = in_num_hw*in_h_hw*in_w_hw; i < in_num_hw*in_h_hw*in_w_hw + 16*288*288; i++){
      input2_file >> cin_hw[i];
    }
    input2_file.close();
  } else {
    cout << "Input2 open failed!" << file_path <<endl;
    exit(-1);
  }

  cout << "Loading weight..." << endl; 
  file_path = prj_path +"/data/weights.dat";
  ifstream weight_file(file_path.c_str()); 
  if (weight_file.is_open()){
    for (int w = 0; w < WEIGHT_SIZE; w++){
      weight_file >> weight_hw[w];
    }
    weight_file.close();
  } else {
    cout << "Weight open failed!" << endl;
    exit(-1);
  }

  cout << "Loading bias..." << endl;
  file_path = prj_path +"/data/biases.dat";
  ifstream bias_file(file_path.c_str());
  if (bias_file.is_open()){
    for (int b = 0; b < BIAS_SIZE; b++){
      bias_file >> bias_hw[b];
    }
    bias_file.close();
  } else {
    cout << "Bias open failed!" << endl;
    exit(-1);
  }

}
#ifdef FLOAT_DESIGN
void postprocess(
  aligned_vector<data_t0> &cin_hw,
  data_t0  outputs_hw[OUT_NUM][OUT_H][OUT_W],
  float  outputs_sw[OUT_NUM][OUT_H][OUT_W]
){
  if(!CHANGE_LAYOUT){
    for (int o1 = 0; o1 < OUT_NUM_HW / OUT_NUM_T; o1++){
      for (int h = 0; h < OUT_H_HW; h++){
        for (int w = 0; w < OUT_W_HW; w++){
          for (int o2 = 0; o2 < OUT_NUM_T; o2++){
            int o = o1 * OUT_NUM_T + o2;
          
            int I1 = o1*OUT_H_HW*OUT_W_HW*OUT_NUM_T;
            int I2 = h*OUT_W_HW*OUT_NUM_T;
            int I3 = w*OUT_NUM_T;
            int I4 = o2;
            int idx = I1 + I2 + I3 + I4;

            if (
                  o < OUT_NUM &&
                  h >= OUT_H_NP && 
                  w >= OUT_W_EP && 
                  h < (OUT_H+OUT_H_SP) &&
                  w < (OUT_W+OUT_W_WP)
                ){
              // cout<<o<<" "<<h-OUT_H_NP<<" "<<w-OUT_W_EP<<" "<<idx<<" "<<cin_hw[OUT_OFFSET1 + idx]<<endl;
              outputs_hw[o][h-OUT_H_NP][w-OUT_W_EP] = cin_hw[OUT_OFFSET1 + idx];
              // cout<<outputs_hw[o][h-OUT_H_NP][w-OUT_W_EP]<<endl;
            }
          }
        }
      }
    }
  }else{
    int OUT_H_T_ = (IN_H_T)*((float)OUT_H/IN_H);
    int OUT_W_T_ = (IN_W_T)*((float)OUT_W/IN_W);
    for (int o1 = 0; o1 < OUT_NUM_HW / OUT_NUM_T; o1++){
      // for (int h1 = 0; h1 < OUT_H / OUT_H_T_; h1++){
      //   for (int w1 = 0; w1 < OUT_W / OUT_W_T_; w1++){
      for (int w1 = 0; w1 < OUT_W / OUT_W_T_; w1++){
        for (int h1 = 0; h1 < OUT_H / OUT_H_T_; h1++){
          for(int h2 = 0; h2 < OUT_H_T_; h2++){
            for(int w2 = 0; w2 < OUT_W_T_; w2++){
              for (int o2 = 0; o2 < OUT_NUM_T; o2++){
                int o = o1 * OUT_NUM_T + o2;
                int h = h1 * OUT_H_T_ + h2;
                int w = w1 * OUT_W_T_ + w2;
                
                int I1 = o1 * OUT_H * OUT_W * OUT_NUM_T;
                int I2 = w1 * OUT_H * OUT_W_T_ * OUT_NUM_T;
                int I3 = h1 * OUT_H_T_ * OUT_W_T_ * OUT_NUM_T;
                // int I2 = h1 * OUT_W * OUT_H_T_ * OUT_NUM_T;
                // int I3 = w1 * OUT_H_T_ * OUT_W_T_ * OUT_NUM_T;

                int I4 = h2 * OUT_W_T_ * OUT_NUM_T;
                int I5 = w2 * OUT_NUM_T;
                int I6 = o2;
                int idx = I1 + I2 + I3 + I4 + I5 + I6;

                if (o < OUT_NUM){
                  outputs_hw[o][h][w] = cin_hw[OUT_OFFSET2 + idx];
                }
              }
            }
          }
        }
      }
    }
  }

  cout << "Loading outputs..." << endl;   
  string file_path = PRJ_PATH;
  string outFile_path = OUTFILE; 
  file_path = file_path + outFile_path;
  
  ifstream ouptut_file(file_path.c_str());
  if (ouptut_file.is_open()){

    int idx = 0;
    for (int o = 0; o < OUT_NUM; o++)
      for (int h = 0; h < OUT_H; h++)
        for (int w = 0; w < OUT_W; w++)
        {
          ouptut_file >> outputs_sw[o][h][w];
          idx++;
        }

    ouptut_file.close();
  } else {
    cout << "Output open failed!" << file_path << endl;
    exit(-1);
  }
}

void compareResults(data_t0  outputs_hw[OUT_NUM][OUT_H][OUT_W], float  outputs_sw[OUT_NUM][OUT_H][OUT_W]){
  cout << "Results comparison..." << endl;
  bool flag = true;
  int err_count = 0;
  int total_count = 0;
  float largest_err = 0;
  float largest_diff = 0;
  float hw_sum = 0;
  float sw_sum = 0;
  for(int ch=0; ch<OUT_NUM; ch++){
    for(int h=0; h<OUT_H; h++){
      for(int w=0; w<OUT_W; w++){
        hw_sum += outputs_hw[ch][h][w];
        sw_sum += outputs_sw[ch][h][w];
        total_count++;
        if(abs(outputs_hw[ch][h][w]-outputs_sw[ch][h][w])>0.001){
          flag = false;
          cout<<outputs_hw[ch][h][w]<<" "<<outputs_sw[ch][h][w]<<" "<<ch<<" "<<h<<" "<<w<<" "<<abs(outputs_hw[ch][h][w]-outputs_sw[ch][h][w])<<endl;
          // cout<<ch<<" "<<h<<" "<<w<<" "<<abs(outputs_hw[ch][h][w]-outputs_sw[ch][h][w])<<endl;
          if(abs(outputs_hw[ch][h][w]-outputs_sw[ch][h][w])>largest_err)
            largest_err = abs(outputs_hw[ch][h][w]-outputs_sw[ch][h][w]);
          // cout<<abs(outputs_hw[ch][h][w]-outputs_sw[ch][h][w])<<endl;
          err_count++;
        }
        if(abs(outputs_hw[ch][h][w]-outputs_sw[ch][h][w])>largest_diff)
          largest_diff = abs(outputs_hw[ch][h][w]-outputs_sw[ch][h][w]);
      }
    }
  }
  if(flag)
    cout<<"SUCESS! Largest difference: "<<largest_diff<<endl;
  else
    cout<<"FAILURE! Largest error: "<<largest_err<<" #of errors: "<<err_count<<" out of "<<total_count<<" errors "<< err_count/float(total_count)<<endl;
  cout<<"HW sum: "<<hw_sum<<" SW sum: "<<sw_sum<<endl;
}
#endif
void save_progress(aligned_vector<data_t0> &cin_hw, uint data_offset){
  char* prj_path_c = PRJ_PATH;
  cout << "saving mem..." << endl;   
  string file_path = string(prj_path_c) + "/data/mem.dat"; 
  ofstream mem_file(file_path.c_str());
  mem_file<<setprecision(16);
  if (mem_file.is_open()){
    for(int i=0; i<data_offset; i++)
    {
      mem_file << cin_hw[i] <<endl;
    }
    mem_file.close();
  } else {
    cout << "mem open failed!" << endl;
    exit(-1);
  }
}

void load_progress(aligned_vector<data_t0> &cin_hw){
  char* prj_path_c = PRJ_PATH;
  cout << "loading mem..." << endl;   
  string file_path = string(prj_path_c) + "/data/mem.dat"; 
  ifstream mem_file(file_path.c_str());
  if (mem_file.is_open()){
    for(int i=0; i<OUT_OFFSET2; i++)
    {
        mem_file >> cin_hw[i];
    }
    mem_file.close();
  } else {
    cout << "mem open failed!" << endl;
    exit(-1);
  }
}