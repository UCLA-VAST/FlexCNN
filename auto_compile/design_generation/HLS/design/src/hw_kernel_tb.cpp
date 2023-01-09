#include "util.h"
#include "cnn_sw.h"
#include <sys/time.h>
// ^((?!//|/\*).)*cout<<
int main(){

  struct timeval start, end;
  // start timer.
  gettimeofday(&start, NULL);

  unsigned int cin_size = CIN_SIZE;
  unsigned int bias_size = BIAS_SIZE;
  unsigned int weight_size = WEIGHT_SIZE;
  unsigned int config_size = 5 + LAYER_NUM * CONFIG_PARAMS;
  #ifdef SIMULATION
    uint start_layer = START_LAYER-1;
    uint end_layer = END_LAYER;
    uint layer_id_test = LAYER_ID_TEST;
  #endif

  cout << "cin_size: " << cin_size << endl;
  cout << "bias_size: " << bias_size << endl;
  cout << "weight_size: " << weight_size <<endl;
 
  data_t0* cin_hw = new data_t0[cin_size];
  data_t1* weight_hw = new data_t1[weight_size];
  data_t2* bias_hw = new data_t2[bias_size];

  memset(cin_hw, 0, cin_size);
  memset(weight_hw, 0, weight_size);
  memset(bias_hw, 0, bias_size);

  // Load instructions 
  uint* config = new uint[config_size];
  #ifdef SIMULATION
    getInsts(config, start_layer, end_layer);
  #endif
  #ifdef SYNTHESIS
    instInit(config);
  #endif
  static float    outputs_sw[OUT_NUM][OUT_H][OUT_W];
  static data_t0  outputs_hw[OUT_NUM][OUT_H][OUT_W];

  #ifdef SIMULATION
    if (SL_OPTION==1 || SL_OPTION==3){
      cout<<"loading progress..."<<endl;
      load_progress(cin_hw);
    }
  #endif
  uint in_num_hw = config[5];
  uint out_num_hw = config[6];
  uint in_h_hw = config[7];
  uint in_w_hw = config[8];
  uint kernel_h = config[24];
  uint kernel_w = config[24];
  preprocess(cin_hw, weight_hw, bias_hw,
            in_num_hw, out_num_hw, in_h_hw, in_w_hw,
            kernel_h, kernel_w);
  

  cout << "HW acceleration..." << endl;
  // Hardware acceleration
  top_kernel(
      (bus_t0*)cin_hw, 
      (bus_t0*)cin_hw,
      (bus_t0*)cin_hw,
      (bus_t1*)weight_hw,
      (bus_t2*)bias_hw,
      (bus_t3*)config
      #ifdef SIMULATION
      ,
      start_layer, end_layer
      #endif
      );

  cout<<"kernel finished"<<endl;

  #ifdef SIMULATION
    if (SL_OPTION==2 || SL_OPTION==3){
      cout<<"saving progress..."<<endl;
      uint out_num_hw = config[(END_LAYER-1) * CONFIG_PARAMS + 6];
      uint out_h_hw = config[(END_LAYER-1) * CONFIG_PARAMS + 17] + config[(END_LAYER-1) * CONFIG_PARAMS + 9] + config[(END_LAYER-1) * CONFIG_PARAMS + 10];
      uint out_w_hw = config[(END_LAYER-1) * CONFIG_PARAMS + 18] + config[(END_LAYER-1) * CONFIG_PARAMS + 11] + config[(END_LAYER-1) * CONFIG_PARAMS + 12];
      uint cout_offset = config[(END_LAYER-1) * CONFIG_PARAMS + 22] + out_h_hw*out_w_hw*out_num_hw;
      cout<<"cout_offset: "<<cout_offset<<endl;
      save_progress(cin_hw, cout_offset);
    }
  #endif
  #ifdef FLOAT_DESIGN
  postprocess(cin_hw, outputs_hw, outputs_sw);

  cout<<"HARDWARE"<<endl;
  for(int ch=0; ch<1; ch++){
    printf("---------------------channel %d--------------------\n", ch);
    for(int h=0; h<OUT_H; h++){
      for(int w=0; w<OUT_W; w++){
        // printf("%s\t", outputs_hw[ch][h][w].to_string(10).c_str());
        printf("%10f\t", outputs_hw[ch][h][w]);
      }
      printf("\n");
    }
  }
  cout<<"SOFTWARE"<<endl;
  for(int ch=0; ch<1; ch++){
    printf("---------------------channel %d--------------------\n", ch);
    for(int h=0; h<OUT_H; h++){
      for(int w=0; w<OUT_W; w++){
        printf("%10f\t", outputs_sw[ch][h][w]);
      }
      printf("\n");
    }
  }

  compareResults(outputs_hw, outputs_sw);
  #endif
  // stop timer.
  gettimeofday(&end, NULL);
  // Calculating total time taken by the program.
  double time_taken;
  time_taken = (end.tv_sec - start.tv_sec) * 1e6;
  time_taken = (time_taken + (end.tv_usec - start.tv_usec)) * 1e-6;
  cout  << "Time taken by program is : " << fixed
        << time_taken << setprecision(6);
  cout << " sec" << endl;
}
