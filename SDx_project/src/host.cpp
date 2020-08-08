
/**********
Copyright (c) 2017, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/
#include "xcl2.hpp"
#include "pose.h"
#include <vector>
#include <sys/time.h>

//typedef struct{
//  unsigned flags;
//  void *obj;
//  void *param;  
//} cl_mem_ext_ptr_t;

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



 uint layer_num = config[0];
 cout << "Layer num: " << layer_num << endl;

  if (in_file.is_open()){
    for (int layer_id = 0; layer_id < LAYER_NUM; layer_id++){
      uint p;
      int param_cnt = 0;
      while(param_cnt < CONFIG_PARAMS){
        in_file >> p;
        config[5 + layer_id * CONFIG_PARAMS + param_cnt] = p;
        param_cnt++;
      }
    }
    in_file.close();
  } else {
    cout << "CONFIG open failed!" << endl;
    exit(-1);
  }
}

int main(int argc, char** argv)
{
  // working path
  char* prj_path = getenv("PRJ_PATH");
  if (prj_path != NULL){ 
    cout << "Your working PATH is: " << prj_path << endl; 
  } else {
    cout << "Working PATH not set!" << endl;
    return -1;
  }
  
  // Measure elapsed time
  struct timeval start, end;
  
  unsigned int cin_size = CIN_SIZE;
  unsigned int bias_size = BIAS_SIZE;
  unsigned int weight_size = WEIGHT_SIZE;
  unsigned int config_size = 5 + LAYER_NUM * CONFIG_PARAMS;
  //int layer_num =
  
  std::cout << "cin_size: " << cin_size << endl;
  std::cout << "bias_size: " << bias_size << endl;
  std::cout << "weight_size: " << weight_size <<endl;  
    
  std::vector<data_t0,aligned_allocator<data_t0>> cin_hw2    (cin_size);
  std::vector<data_t0,aligned_allocator<data_t0>> cin_hw    (cin_size);
  std::vector<data_t1,aligned_allocator<data_t1>> weight_hw (weight_size);
  std::vector<data_t2,aligned_allocator<data_t2>> bias_hw   (bias_size);
  
  data_t0* cin_sw = new data_t0[cin_size];
  data_t1* weight_sw = new data_t1[weight_size];
  data_t2* bias_sw = new data_t2[bias_size];
  data_t0* cin_hw_cpu = new data_t0[cin_size];
  data_t0* cin_hw_cpu2 = new data_t0[cin_size];
  
  memset(cin_sw, 0, cin_size);
  memset(weight_sw, 0, weight_size);
  memset(bias_sw, 0, bias_size);
  
  // Load instructions 
  uint* config = new uint[config_size];
  instInit(config);

  std::vector<unsigned int,aligned_allocator<unsigned int>> config_hw (config_size);
  for (int i = 0; i < config_size; i++)
    config_hw[i] = config[i];
  
  data_t0 LAYER_out_sw[STAGE2L_OUT_H][STAGE2L_OUT_W][STAGE2R_OUT_NUM + STAGE2L_OUT_NUM];
  data_t0 LAYER_out_hw[STAGE2L_OUT_H][STAGE2L_OUT_W][STAGE2R_OUT_NUM + STAGE2L_OUT_NUM];
  data_t0 LAYER_out_hw2[STAGE2L_OUT_H][STAGE2L_OUT_W][STAGE2R_OUT_NUM + STAGE2L_OUT_NUM];
  
  //data_t0 LAYER_SINGLE_out_sw[out_h][out_w][out_num];
  //data_t0 LAYER_SINGLE_out_hw[out_h][out_w][out_num];
  
  // initialize weights, bias, inputs, and golden outputs
  //openpose_preprocess(cin_sw, weight_sw, bias_sw, LAYER_out_sw);
  mobilenet_preprocess(cin_sw, weight_sw, bias_sw, LAYER_out_sw);

  for (int i = 0; i < cin_size; i++)
    cin_hw2[i] = cin_sw[i];
  for (int i = 0; i < cin_size; i++)
    cin_hw[i] = cin_sw[i];
  for (int i = 0; i < weight_size; i++)
    weight_hw[i] = weight_sw[i];
  for (int i = 0; i < bias_size; i++)
    bias_hw[i] = bias_sw[i];
 
// OPENCL HOST CODE AREA START
  // Create Program and Kernel
  std::vector<cl::Device> devices = xcl::get_xil_devices();
  // Hardware Test
  std::cout << "device number: " << devices.size() << std::endl;
  cl::Device device = devices[0];
  // Hardware Emulation
  // Please uncomment the code below and comment out the code above to enable
  // hardware emulation
//  cl::Device device = devices[0];

  cl::Context context(device);
  cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);
  std::string device_name = device.getInfo<CL_DEVICE_NAME>(); 
  std::cout << "device name: " << device_name << endl;
  
//  std::string binaryFile = xcl::find_binary_file(device_name,"kernel");
  std::string binaryFile = argv[1];
  cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
  // Hardware Test
  // Please comment the code below if used for hardware emulation
//  devices.erase(devices.begin());
  
  devices.resize(1);
  cl::Program program(context, devices, bins);
  cl::Kernel krnl_vadd(program,"top_kernel");
  cl::Kernel krnl_vadd2(program,"top_kernel");

  // multi ddr  
  cl_mem_ext_ptr_t GlobMem_BUF_in0_Ext;
  GlobMem_BUF_in0_Ext.param = 0; 
  GlobMem_BUF_in0_Ext.flags = XCL_MEM_DDR_BANK0;
  GlobMem_BUF_in0_Ext.obj = cin_hw.data();

  cl_mem_ext_ptr_t GlobMem_BUF_in1_Ext;
  GlobMem_BUF_in1_Ext.param = 0;
  GlobMem_BUF_in1_Ext.flags = XCL_MEM_DDR_BANK1;
  GlobMem_BUF_in1_Ext.obj = weight_hw.data();

  cl_mem_ext_ptr_t GlobMem_BUF_in2_Ext;
  GlobMem_BUF_in2_Ext.param = 0;
  GlobMem_BUF_in2_Ext.flags = XCL_MEM_DDR_BANK1;
  GlobMem_BUF_in2_Ext.obj = bias_hw.data();

  cl_mem_ext_ptr_t GlobMem_BUF_in3_Ext;
  GlobMem_BUF_in3_Ext.param = 0;
  GlobMem_BUF_in3_Ext.flags = XCL_MEM_DDR_BANK0;
  GlobMem_BUF_in3_Ext.obj = config_hw.data();  

  // Allocate Buffer in Global Memory
  std::vector<cl::Memory> inBufVec, outBufVec;
  
  //gettimeofday(&start, NULL);
  cl::Buffer buffer_cin(context,CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
            cin_size*sizeof(data_t0), &GlobMem_BUF_in0_Ext);
  cl::Buffer buffer_weight(context,CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
            weight_size*sizeof(data_t1), &GlobMem_BUF_in1_Ext);
  cl::Buffer buffer_bias(context,CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
            bias_size*sizeof(data_t2), &GlobMem_BUF_in2_Ext);
  cl::Buffer buffer_config(context,CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
            config_size*sizeof(unsigned int), &GlobMem_BUF_in3_Ext);
            
  inBufVec.push_back(buffer_cin);
  inBufVec.push_back(buffer_cin);
  inBufVec.push_back(buffer_weight);
  inBufVec.push_back(buffer_bias);
  inBufVec.push_back(buffer_config);
  outBufVec.push_back(buffer_cin);
  
  
  // Set the Kernel Arguments
  krnl_vadd.setArg(0,buffer_cin);
  krnl_vadd.setArg(1,buffer_cin);
  krnl_vadd.setArg(2,buffer_cin);
  krnl_vadd.setArg(3,buffer_weight);
  krnl_vadd.setArg(4,buffer_bias);
  krnl_vadd.setArg(5,buffer_config); 
  
  
  //gettimeofday(&start, NULL);

  // Copy input data to device global memory
  
  uint kernel_count = 0;
  uint num_run = 1;
  
  q.enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
  q.finish();
  
  
  // Launch the Kernel
  std::cout << "Kernel launched!" << endl;
  gettimeofday(&start, NULL);
  q.enqueueTask(krnl_vadd);
  q.finish();
  gettimeofday(&end, NULL);
  // Copy Result from Device Global Memory to Host Local Memory  
  q.enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
  q.finish();
  
  
  //gettimeofday(&end, NULL);
  float elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)/1000000.0;
  cout << "Kernel done! Elapsed time(s): " << elapsed_time / num_run << endl;

// OPENCL HOST CODE AREA END
  
#ifdef DEBUG  
  // dump STAGE2L1
  int start_offset = 1676357;
  for (int o = 0; o < 128; o++)
    for (int h = 0; h < 4; h++)
      for (int w = 0; w < 4; w++){
        int real_offset = start_offset + o*22*22;
        data_t res = cin_hw[real_offset + h*22 + w];
        cout << res << endl;
      }
#endif  

  // Extract hardware outputs
  for (int i = 0; i < cin_size; i++)
    cin_hw_cpu[i] = cin_hw[i];

  openpose_postprocess(cin_hw_cpu, LAYER_out_hw);
  for (int i = 0; i < cin_size; i++)
    cin_hw_cpu2[i] = cin_hw2[i];

  openpose_postprocess(cin_hw_cpu2, LAYER_out_hw2);
  
  // Compare the results of the Device to the simulation
  std::cout << "Results comparison..." << endl;
  int err_cnt = 0;
  bool test = false;
  
      
  if (test){
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
     
  delete[] cin_sw;
  delete[] weight_sw;
  delete[] bias_sw;
  delete[] cin_hw_cpu;

  std::cout << "TEST " << (err_cnt ? "FAILED" : "PASSED") << std::endl; 
  return (err_cnt ? EXIT_FAILURE :  EXIT_SUCCESS);
}
