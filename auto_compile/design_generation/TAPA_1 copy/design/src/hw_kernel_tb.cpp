#include "util.h"
#include "cnn_sw.h"
#include <sys/time.h>
#include <gflags/gflags.h>
// ^((?!//|/\*).)*cout<<

template <typename T>
using aligned_vector = std::vector<T, tapa::aligned_allocator<T>>;

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]){
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);
  const uint64_t start_layer = atoll(argv[1]);
  const uint64_t end_layer = atoll(argv[2]);
  struct timeval start, end;
  // start timer.
  gettimeofday(&start, NULL);

  unsigned int cin_size = CIN_SIZE;
  unsigned int bias_size = BIAS_SIZE;
  unsigned int weight_size = WEIGHT_SIZE;
  unsigned int config_size = LAYER_NUM * CONFIG_PARAMS;
  #ifdef SIMULATION
    uint start_inst = start_layer-1;
    uint end_inst = end_layer;
    uint layer_id_test = LAYER_ID_TEST;
  #endif

  cout << "cin_size: " << cin_size << endl;
  cout << "bias_size: " << bias_size << endl;
  cout << "weight_size: " << weight_size <<endl;
 
  aligned_vector<data_t0> cin_hw(cin_size);
  aligned_vector<data_t1> weight_hw(weight_size);
  aligned_vector<data_t2> bias_hw(bias_size);

  // memset(cin_hw, 0, cin_size);
  // memset(weight_hw, 0, weight_size);
  // memset(bias_hw, 0, bias_size);

  // Load instructions 
  aligned_vector<uint> config(config_size);
  #ifdef SIMULATION
    getInsts(config, start_inst, end_inst);
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
  uint in_num_hw = config[0];
  uint out_num_hw = config[1];
  uint in_h_hw = config[2];
  uint in_w_hw = config[3];
  uint kernel_h = config[19];
  uint kernel_w = config[19];
  preprocess(cin_hw, weight_hw, bias_hw,
            in_num_hw, out_num_hw, in_h_hw, in_w_hw,
            kernel_h, kernel_w);
  

  cout << "HW acceleration..." << endl;
  // Hardware acceleration
  cout<<FLAGS_bitstream<<endl;
  int64_t kernel_time_ns = tapa::invoke(top_kernel, FLAGS_bitstream, 
    tapa::read_write_mmap<data_t0>(cin_hw).reinterpret<bus_t0>(),
    // tapa::read_only_mmap<data_t0>(cin_hw).reinterpret<bus_t0>(),
    // tapa::write_only_mmap<data_t0>(cin_hw).reinterpret<bus_t0>(),
    tapa::read_only_mmap<data_t1>(weight_hw).reinterpret<bus_t1>(),
    // tapa::read_only_mmap<data_t2>(bias_hw).reinterpret<bus_t2>(),
    tapa::read_only_mmap<data_t3>(config).reinterpret<bus_t3>(),
    start_inst,
    end_inst
  );

  cout<<"kernel finished"<<endl;
  cout<<"kernel time in seconds: "<<kernel_time_ns/1e9<<endl;
  cout<<"FPS: "<<1e9/kernel_time_ns<<endl;
  #ifdef SIMULATION
    if (SL_OPTION==2 || SL_OPTION==3){
      cout<<"saving progress..."<<endl;
      uint out_num_hw = config[(END_LAYER-1) * CONFIG_PARAMS + 1];
      uint out_h_hw = config[(END_LAYER-1) * CONFIG_PARAMS + 12] + config[(END_LAYER-1) * CONFIG_PARAMS + 4] + config[(END_LAYER-1) * CONFIG_PARAMS + 5];
      uint out_w_hw = config[(END_LAYER-1) * CONFIG_PARAMS + 13] + config[(END_LAYER-1) * CONFIG_PARAMS + 6] + config[(END_LAYER-1) * CONFIG_PARAMS + 7];
      uint cout_offset = config[(END_LAYER-1) * CONFIG_PARAMS + 17] + out_h_hw*out_w_hw*out_num_hw;
      cout<<"cout_offset: "<<cout_offset<<endl;
      save_progress(cin_hw, cout_offset);
    }
  #endif
  #ifdef FLOAT_DESIGN
  postprocess(cin_hw, outputs_hw, outputs_sw);

  cout<<"HARDWARE"<<endl;
  for(int ch=0; ch<OUT_NUM; ch++){
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
  for(int ch=0; ch<OUT_NUM; ch++){
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
  
  // delete [] cin_hw;
  // delete [] weight_hw;
  // delete [] bias_hw;
  // delete [] config;
}
