// clang-format off
#include "xcl2.hpp"
#include "sacc.hpp"
#include "sacc_types.h"
#include "sacc_params.h"
#ifndef NDEBUG
#include "sacc_utils.hpp"
#endif
#include <assert.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <omp.h>
// clang-format on

#define PERF
//#define PERF_iter

const unsigned int cin_size = CIN_SIZE;
const unsigned int bias_size = BIAS_SIZE;
const unsigned int weight_size = WEIGHT_SIZE;
const unsigned int config_size = 5 + LAYER_NUM * CONFIG_PARAMS;

data_t1* weight_hw = new data_t1[weight_size];
data_t2* bias_hw = new data_t2[bias_size];


#ifndef NDEBUG
std::string Sacc::lib_base_path = "/curr/atefehSZ/research/libsacc/";
#else
std::string Sacc::lib_base_path = "/usr/local/lib/sacc/";
#endif
std::string Sacc::inst_file_path = lib_base_path + "config/network.insts";
std::string Sacc::weight_file_path = lib_base_path + "data/weight_reorg.dat";
std::string Sacc::bias_file_path = lib_base_path + "data/bias_reorg.dat";
std::string Sacc::xcl_binary_path =
    Sacc::lib_base_path + "config/binary_container_1.xclbin";

Sacc::Sacc(OpKernelConstruction *context) : OpKernel(context) {
  // Get the index of the value to preserve
  int n_;
  OP_REQUIRES_OK(context,
                 context->GetAttr("N", &n_));
  // Check that preserve_index is positive
  OP_REQUIRES(context, n_ > 0,
              errors::InvalidArgument("Need N > 0, got ",
                                      n_));
#ifdef PERF
  Timer timer;
#endif
  init();
  

#ifdef PERF
  double elapsed = timer.elapsed();
  std::cout << "INFO: Down in FPGA Init time = " << elapsed / 1000 << " seconds"
            << std::endl;
#endif
}

void Sacc::Compute(OpKernelContext *context) {
  // Grab the input tensor

  const int batch_num = context->num_inputs();
  

  TensorShape output_shape;
  // refer to
  // https://github.com/tensorflow/tensorflow/blob/master/tensorflow/core/framework/tensor_shape.h
  int32 dims[4] = {1, STAGE2L_OUT_H, STAGE2L_OUT_W,
                   (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM)};
  TensorShapeUtils::MakeShape(dims, 4, &output_shape);
  
  assert(LAYER1_IN_NUM_HW ==
         LAYER1_IN_NUM_T);  
#ifdef PERF
  Timer timer;
  double elapsed10;
#endif

  int batch_count = 0;
  for (int i = 0; i < batch_num + 3; i++){
    batch_count = i;
#ifdef PERF_iter 
      std::cout << std::endl;
#endif
    if (batch_count % 3 == 0){ 
#ifdef PERF_iter 
      double elapsed = timer.elapsed();
#endif
      
      if (batch_count < batch_num){
        const Tensor &input_tensor = context->input(i);
        auto input = input_tensor.flat<float>();
        reformat_input_data_layout(input); 
      }
      
      if (batch_count > 2){
        Tensor *output_tensor = NULL;
        OP_REQUIRES_OK(context,
                       context->allocate_output(i-3, output_shape, &output_tensor));
        auto output_flat = output_tensor->flat<float>();
        q.finish();
        reformat_output_data_layout(output_flat); 
      }

#ifdef PERF_iter        
      double elapsed2 = timer.elapsed();
      std::cout << batch_count << " q out reformat = " << (elapsed - elapsed2) / 1000 << " seconds" << std::endl;
#endif 

      if (batch_count < batch_num){
        q.enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
      }

#ifdef PERF_iter       
      elapsed = timer.elapsed();
      std::cout << batch_count << " q enqueue = " << (elapsed2 - elapsed) / 1000 << " seconds" << std::endl;
#endif
      
      if (batch_count > 0 && batch_count < batch_num + 1){
        q3.finish();
        q3.enqueueTask(krnl_vadd3);
      }

#ifdef PERF_iter       
      elapsed2 = timer.elapsed();
      std::cout << batch_count << " q3 task = " << (elapsed2 - elapsed) / 1000 << " seconds" << std::endl;
#endif
      
      if (batch_count > 1 && batch_count < batch_num + 2){
        q2.finish();
        q2.enqueueMigrateMemObjects(outBufVec2,CL_MIGRATE_MEM_OBJECT_HOST);
      }

#ifdef PERF_iter       
      elapsed = timer.elapsed();
      std::cout << batch_count << " q2 dequeue = " << (elapsed - elapsed2) / 1000 << " seconds" << std::endl;
#endif
      
    } else if (batch_count % 3 == 1) {
#ifdef PERF_iter 
      double elapsed = timer.elapsed();
#endif  
      
      if (batch_count < batch_num){
        const Tensor &input_tensor = context->input(i);
        auto input = input_tensor.flat<float>();
        reformat_input_data_layout2(input); 
      }
          
      if (batch_count > 2){
        Tensor *output_tensor = NULL;
        OP_REQUIRES_OK(context,
                       context->allocate_output(i-3, output_shape, &output_tensor));
        auto output_flat2 = output_tensor->flat<float>();
        q2.finish();
        reformat_output_data_layout2(output_flat2); 
      }

#ifdef PERF_iter       
      double elapsed2 = timer.elapsed();
      std::cout << batch_count << " q2 out reformat = " << (elapsed - elapsed2) / 1000 << " seconds" << std::endl;
#endif  

      if (batch_count < batch_num){
        q2.enqueueMigrateMemObjects(inBufVec2,0/* 0 means from host*/);
      }

#ifdef PERF_iter       
      elapsed = timer.elapsed();
      std::cout << batch_count << " q2 enqueue = " << (elapsed2 - elapsed) / 1000 << " seconds" << std::endl;
#endif      
      
      if (batch_count > 0 && batch_count < batch_num + 1){
        q.finish();
        q.enqueueTask(krnl_vadd);
      }

#ifdef PERF_iter       
      elapsed2 = timer.elapsed();
      std::cout << batch_count << " q task = " << (elapsed2 - elapsed) / 1000 << " seconds" << std::endl;
#endif
      
         
      
      if (batch_count > 1 && batch_count < batch_num + 2){
        q3.finish();
        q3.enqueueMigrateMemObjects(outBufVec3,CL_MIGRATE_MEM_OBJECT_HOST);
      }
#ifdef PERF_iter 
      elapsed = timer.elapsed();
      std::cout << batch_count << " q3 dequeue = " << (elapsed - elapsed2) / 1000 << " seconds" << std::endl;
#endif

    } else if (batch_count % 3 == 2) {
#ifdef PERF_iter 
      double elapsed = timer.elapsed();
#endif  
      
      if (batch_count < batch_num){
        const Tensor &input_tensor = context->input(i);
        auto input = input_tensor.flat<float>();
        reformat_input_data_layout3(input); 
        //q.enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
      }
      
      if (batch_count > 2){
        Tensor *output_tensor = NULL;
        OP_REQUIRES_OK(context,
                       context->allocate_output(i-3, output_shape, &output_tensor));
        auto output_flat3 = output_tensor->flat<float>();
        q3.finish();
        reformat_output_data_layout3(output_flat3); 
      }

#ifdef PERF_iter       
      double elapsed2 = timer.elapsed();
      std::cout << batch_count << " q3 out reformat = " << (elapsed - elapsed2) / 1000 << " seconds" << std::endl;
#endif
    
      if (batch_count < batch_num){
        q3.enqueueMigrateMemObjects(inBufVec3,0/* 0 means from host*/);
      }
      
#ifdef PERF_iter       
      elapsed = timer.elapsed();
      std::cout << batch_count << " q3 enqueue = " << (elapsed2 - elapsed) / 1000 << " seconds" << std::endl;
#endif
      
      if (batch_count > 0 && batch_count < batch_num + 1){
        q2.finish();
        q2.enqueueTask(krnl_vadd2);
      }

#ifdef PERF_iter       
      elapsed2 = timer.elapsed();
      std::cout << batch_count << " q2 task = " << (elapsed2 - elapsed) / 1000 << " seconds" << std::endl;
#endif
          
      
      if (batch_count > 1 && batch_count < batch_num + 2){
        q.finish();
        q.enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
      }

#ifdef PERF_iter       
      elapsed = timer.elapsed();
      std::cout << batch_count << " q dequeue = " << (elapsed - elapsed2) / 1000 << " seconds" << std::endl;
#endif
    }
#ifdef PERF_iter 
    double elapsed20 = elapsed10;
    elapsed10 = timer.elapsed();
    std::cout << batch_count << " whole = " << (elapsed10) / 1000 << " iter " << (elapsed10 - elapsed20) / 1000 << " seconds" << std::endl;
#endif
  }
  
  

#ifdef PERF
  double elapsed = timer.elapsed();
  std::cout << "INFO: FPGA Inference time for whole batch = " << elapsed / 1000
            << " seconds" << std::endl;
  std::cout << "INFO: FPGA Inference time per frame = " << (elapsed / batch_num) / 1000
            << " seconds (" << 1 / ((elapsed / batch_num) / 1000) << " FPS)" << std::endl;

#endif
}


inline void Sacc::reformat_input_data_layout(auto const &src) {

  for (int iter = 0; iter < 1; iter++){
  int idx = 0;
  for (int h = 0; h < LAYER1_IN_H; h++)
    for (int w = 0; w < LAYER1_IN_W; w++)
      for (int i1 = 0; i1 < LAYER1_IN_NUM_HW / LAYER1_IN_NUM_T; i1++)
        for (int i2 = 0; i2 < LAYER1_IN_NUM_T; i2++){
          int i = i1 * LAYER1_IN_NUM_T + i2;
          if (i < LAYER1_IN_NUM){
            cin[i1 * LAYER1_IN_H_HW * LAYER1_IN_W_HW * LAYER1_IN_NUM_T + (h + int(LAYER1_K / 2)) * LAYER1_IN_W_HW * LAYER1_IN_NUM_T + (w + int(LAYER1_K / 2)) * LAYER1_IN_NUM_T + i2] = src(idx++); // filter size = 3
          }
        }
  }
  
  // Set the Kernel Arguments
  krnl_vadd.setArg(0,buffer_cin);
  krnl_vadd.setArg(1,buffer_cin);
  krnl_vadd.setArg(2,buffer_cin);
  krnl_vadd.setArg(3,buffer_weight);
  krnl_vadd.setArg(4,buffer_bias);
  krnl_vadd.setArg(5,buffer_config); 

}

inline void Sacc::reformat_output_data_layout(auto &output_flat) {

  for (int h = 0; h < STAGE2L_OUT_H; h++)
    for (int w = 0; w < STAGE2L_OUT_W; w++)
      for (int o = 0; o < STAGE2L_OUT_NUM; o++)
        output_flat(h * STAGE2L_OUT_W * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) +
                    w * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) + o +
                    STAGE2R_OUT_NUM) =
            cin[STAGE2L_OFFSET +
                (h + int(STAGE2L_K / 2)) * STAGE2L_OUT_W_HW *
                    STAGE2L_OUT_NUM_T +
                (w + int(STAGE2L_K / 2)) * STAGE2L_OUT_NUM_T + o];
  for (int h = 0; h < STAGE2R_OUT_H; h++)
    for (int w = 0; w < STAGE2R_OUT_W; w++)
      for (int o = 0; o < STAGE2R_OUT_NUM; o++)
        output_flat(h * STAGE2R_OUT_W * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) +
                    w * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) + o) =
            cin[STAGE2R_OFFSET +
                (h + int(STAGE2R_K / 2)) * STAGE2R_OUT_W_HW *
                    STAGE2R_OUT_NUM_T +
                (w + int(STAGE2R_K / 2)) * STAGE2R_OUT_NUM_T + o];
		
}

inline void Sacc::reformat_input_data_layout2(auto const &src) {

for (int iter = 0; iter < 1; iter++){
  int idx = 0;
  
  for (int h = 0; h < LAYER1_IN_H; h++)
    for (int w = 0; w < LAYER1_IN_W; w++)
      for (int i1 = 0; i1 < LAYER1_IN_NUM_HW / LAYER1_IN_NUM_T; i1++)
        for (int i2 = 0; i2 < LAYER1_IN_NUM_T; i2++){
          int i = i1 * LAYER1_IN_NUM_T + i2;
          if (i < LAYER1_IN_NUM){
            cin2[i1 * LAYER1_IN_H_HW * LAYER1_IN_W_HW * LAYER1_IN_NUM_T + (h + int(LAYER1_K / 2)) * LAYER1_IN_W_HW * LAYER1_IN_NUM_T + (w + int(LAYER1_K / 2)) * LAYER1_IN_NUM_T + i2] = src(idx++);
          }
        }
  }    
  
  // Set the Kernel Arguments
  krnl_vadd2.setArg(0,buffer_cin2);
  krnl_vadd2.setArg(1,buffer_cin2);
  krnl_vadd2.setArg(2,buffer_cin2);
  krnl_vadd2.setArg(3,buffer_weight2);
  krnl_vadd2.setArg(4,buffer_bias2);
  krnl_vadd2.setArg(5,buffer_config2); 


}

inline void Sacc::reformat_output_data_layout2(auto &output_flat) {

  for (int h = 0; h < STAGE2L_OUT_H; h++)
    for (int w = 0; w < STAGE2L_OUT_W; w++)
      for (int o = 0; o < STAGE2L_OUT_NUM; o++)
        output_flat(h * STAGE2L_OUT_W * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) +
                    w * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) + o +
                    STAGE2R_OUT_NUM) =
            cin2[STAGE2L_OFFSET +
                (h + int(STAGE2L_K / 2)) * STAGE2L_OUT_W_HW *
                    STAGE2L_OUT_NUM_T +
                (w + int(STAGE2L_K / 2)) * STAGE2L_OUT_NUM_T + o];
  for (int h = 0; h < STAGE2R_OUT_H; h++)
    for (int w = 0; w < STAGE2R_OUT_W; w++)
      for (int o = 0; o < STAGE2R_OUT_NUM; o++)
        output_flat(h * STAGE2R_OUT_W * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) +
                    w * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) + o) =
            cin2[STAGE2R_OFFSET +
                (h + int(STAGE2R_K / 2)) * STAGE2R_OUT_W_HW *
                    STAGE2R_OUT_NUM_T +
                (w + int(STAGE2R_K / 2)) * STAGE2R_OUT_NUM_T + o];
                
		
}


inline void Sacc::reformat_input_data_layout3(auto const &src) {

for (int iter = 0; iter < 1; iter++){
  int idx = 0;
  
  for (int h = 0; h < LAYER1_IN_H; h++)
    for (int w = 0; w < LAYER1_IN_W; w++)
      for (int i1 = 0; i1 < LAYER1_IN_NUM_HW / LAYER1_IN_NUM_T; i1++)
        for (int i2 = 0; i2 < LAYER1_IN_NUM_T; i2++){
          int i = i1 * LAYER1_IN_NUM_T + i2;
          if (i < LAYER1_IN_NUM){
            cin3[i1 * LAYER1_IN_H_HW * LAYER1_IN_W_HW * LAYER1_IN_NUM_T + (h + int(LAYER1_K / 2)) * LAYER1_IN_W_HW * LAYER1_IN_NUM_T + (w + int(LAYER1_K / 2)) * LAYER1_IN_NUM_T + i2] = src(idx++);
          }
        }
    }  
  
  // Set the Kernel Arguments
  krnl_vadd3.setArg(0,buffer_cin3);
  krnl_vadd3.setArg(1,buffer_cin3);
  krnl_vadd3.setArg(2,buffer_cin3);
  krnl_vadd3.setArg(3,buffer_weight3);
  krnl_vadd3.setArg(4,buffer_bias3);
  krnl_vadd3.setArg(5,buffer_config3); 


}

inline void Sacc::reformat_output_data_layout3(auto &output_flat) {

  for (int h = 0; h < STAGE2L_OUT_H; h++)
    for (int w = 0; w < STAGE2L_OUT_W; w++)
      for (int o = 0; o < STAGE2L_OUT_NUM; o++)
        output_flat(h * STAGE2L_OUT_W * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) +
                    w * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) + o +
                    STAGE2R_OUT_NUM) =
            cin3[STAGE2L_OFFSET +
                (h + int(STAGE2L_K / 2)) * STAGE2L_OUT_W_HW *
                    STAGE2L_OUT_NUM_T +
                (w + int(STAGE2L_K / 2)) * STAGE2L_OUT_NUM_T + o];
  for (int h = 0; h < STAGE2R_OUT_H; h++)
    for (int w = 0; w < STAGE2R_OUT_W; w++)
      for (int o = 0; o < STAGE2R_OUT_NUM; o++)
        output_flat(h * STAGE2R_OUT_W * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) +
                    w * (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM) + o) =
            cin3[STAGE2R_OFFSET +
                (h + int(STAGE2R_K / 2)) * STAGE2R_OUT_W_HW *
                    STAGE2R_OUT_NUM_T +
                (w + int(STAGE2R_K / 2)) * STAGE2R_OUT_NUM_T + o];
                
		
}



void Sacc::send_to_process() {
  // Set the Kernel Arguments
  krnl_vadd.setArg(0, buffer_cin);
  krnl_vadd.setArg(1, buffer_cin);
  krnl_vadd.setArg(2, buffer_cin);
  krnl_vadd.setArg(3, buffer_weight);
  krnl_vadd.setArg(4, buffer_bias);
  krnl_vadd.setArg(5, buffer_config);
  
  // Copy input data to device global memory
  q.enqueueMigrateMemObjects(inBufVec, 0 /* 0 means from host*/);
  q.finish();
  
  // Launch the Kernel
  q.enqueueTask(krnl_vadd);
  q.finish();

  // Copy Result from Device Global Memory to Host Local Memory
  q.enqueueMigrateMemObjects(outBufVec, CL_MIGRATE_MEM_OBJECT_HOST);
  q.finish();
}

/**
 * load bias from file
 * @param bias : stores values
 * @param input_file_path : specify the file path
 */
inline void Sacc::load_bias(
    std::vector<data_t2, aligned_allocator<data_t2>> &bias,
    const std::string &input_file_path) {
#ifndef NDEBUG
  std::cout << "INFO: Loading bias from: " << input_file_path << std::endl;
#endif
  std::ifstream bias_file(input_file_path.c_str());
  
  if (bias_file.is_open()){
    for (int w = 0; w < bias_size; w++){
      bias_file >> bias_hw[w];
    }

    bias_file.close();
  } else {
    std::cout << "ERROR: Bias open failed!" << std::endl;
    exit(-1);
  }
  bias_file.close();
  for (int i = 0; i < bias_size; i++)
    bias[i] = bias_hw[i];
  bias_file.close();
}

/**
 * load weight from file
 * @param weight : stores values
 * @param input_file_path : specify the file path
 */
inline void Sacc::load_weights(
    std::vector<data_t1, aligned_allocator<data_t1>> &weight,
    const std::string &input_file_path) {
#ifndef NDEBUG
  std::cout << "INFO: Loading weight from: " << input_file_path << std::endl;
#endif
  std::ifstream weight_file(input_file_path.c_str());
  
  if (weight_file.is_open()){
    for (int w = 0; w < weight_size; w++){
      weight_file >> weight_hw[w];
    }

    weight_file.close();
  } else {
    std::cout << "ERROR: Weight open failed!" << std::endl;
    exit(-1);
  }
  weight_file.close();
  for (int i = 0; i < weight_size; i++)
    weight[i] = weight_hw[i];
}

/* *
 * load instruction for SA accelerator
 * @param config : store the value
 * @param input_file_path : specify the file path
 */
inline void Sacc::load_inst(
    std::vector<unsigned int, aligned_allocator<unsigned int>> &config,
    const std::string &input_file_path) {
#ifndef NDEBUG
  std::cout << "INFO: Loading instructions from: " << input_file_path
            << std::endl;
#endif

  std::ifstream in_file(input_file_path.c_str());

  // model configuration
#ifdef VGG_LAYERS
  config[0] = VGG_LAYERS;
  config[1] = MobileNetV2_LAYERS;
  config[2] = STAGE1_LAYERS;
  config[3] = STAGE2_LAYERS;
  config[4] = STAGE2_ITER;
#else
  config[0] = LAYER_NUM;
#endif

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
  std::cout << "CONFIG open failed!" << std::endl;
    exit(-1);
  }
}

inline void Sacc::init() {
  devices = xcl::get_xil_devices();

  device_name = devices[0].getInfo<CL_DEVICE_NAME>();

  context = devices[0];
  q = cl::CommandQueue(context, devices[0]);
  q2 = cl::CommandQueue(context, devices[0]);
  q3 = cl::CommandQueue(context, devices[0]);
  bins = xcl::import_binary_file(xcl_binary_path);
  program = cl::Program(context, devices, bins);
  krnl_vadd = cl::Kernel(program, "top_kernel");
  krnl_vadd2 = cl::Kernel(program, "top_kernel");
  krnl_vadd3 = cl::Kernel(program, "top_kernel");

#ifndef NDEBUG
  std::cout << "INFO: Device name: " << device_name << std::endl;
#endif

  assert(STAGE2L_OUT_NUM_HW ==
         STAGE2L_OUT_NUM_T);  // refer to develpor for the data structure
  assert(STAGE2R_OUT_NUM_HW ==
         STAGE2R_OUT_NUM_T);  // refer to develpor for the data structure

  load_inst(config, inst_file_path);
  load_weights(weight, weight_file_path);
  load_bias(bias, bias_file_path);

  GlobMem_BUF_in0_Ext.param = 0;
  GlobMem_BUF_in0_Ext.flags = XCL_MEM_DDR_BANK0;
  GlobMem_BUF_in0_Ext.obj = cin.data();

  GlobMem_BUF_in1_Ext.param = 0;
  GlobMem_BUF_in1_Ext.flags = XCL_MEM_DDR_BANK1;
  GlobMem_BUF_in1_Ext.obj = weight.data();

  GlobMem_BUF_in2_Ext.param = 0;
  GlobMem_BUF_in2_Ext.flags = XCL_MEM_DDR_BANK1;
  GlobMem_BUF_in2_Ext.obj = bias.data();

  GlobMem_BUF_in3_Ext.param = 0;
  GlobMem_BUF_in3_Ext.flags = XCL_MEM_DDR_BANK0;
  GlobMem_BUF_in3_Ext.obj = config.data();

  cl::Buffer buffer_cin_local(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
      cin_size * sizeof(data_t0), &GlobMem_BUF_in0_Ext);
  cl::Buffer buffer_weight_local(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
      weight_size * sizeof(data_t1), &GlobMem_BUF_in1_Ext);
  cl::Buffer buffer_bias_local(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
      bias_size * sizeof(data_t2), &GlobMem_BUF_in2_Ext);
  cl::Buffer buffer_config_local(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
      config_size * sizeof(unsigned int), &GlobMem_BUF_in3_Ext);

  buffer_cin = buffer_cin_local;
  buffer_weight = buffer_weight_local;
  buffer_bias = buffer_bias_local;
  buffer_config = buffer_config_local;

  inBufVec.push_back(buffer_cin);
  inBufVec.push_back(buffer_cin);
  inBufVec.push_back(buffer_weight);
  inBufVec.push_back(buffer_bias);
  inBufVec.push_back(buffer_config);
  outBufVec.push_back(buffer_cin);
  
  
  //// Atefeh : Creating another set of buffer to enable double buffering ////
  GlobMem_BUF_in0_Ext2.param = 0;
  GlobMem_BUF_in0_Ext2.flags = XCL_MEM_DDR_BANK0;
  GlobMem_BUF_in0_Ext2.obj = cin2.data();

  GlobMem_BUF_in1_Ext2.param = 0;
  GlobMem_BUF_in1_Ext2.flags = XCL_MEM_DDR_BANK1;
  GlobMem_BUF_in1_Ext2.obj = weight.data();

  GlobMem_BUF_in2_Ext2.param = 0;
  GlobMem_BUF_in2_Ext2.flags = XCL_MEM_DDR_BANK1;
  GlobMem_BUF_in2_Ext2.obj = bias.data();

  GlobMem_BUF_in3_Ext2.param = 0;
  GlobMem_BUF_in3_Ext2.flags = XCL_MEM_DDR_BANK0;
  GlobMem_BUF_in3_Ext2.obj = config.data();

  cl::Buffer buffer_cin_local2(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
      cin_size * sizeof(data_t0), &GlobMem_BUF_in0_Ext2);
  cl::Buffer buffer_weight_local2(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
      weight_size * sizeof(data_t1), &GlobMem_BUF_in1_Ext2);
  cl::Buffer buffer_bias_local2(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
      bias_size * sizeof(data_t2), &GlobMem_BUF_in2_Ext2);
  cl::Buffer buffer_config_local2(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
      config_size * sizeof(unsigned int), &GlobMem_BUF_in3_Ext2);

  buffer_cin2 = buffer_cin_local2;
  buffer_weight2 = buffer_weight_local2;
  buffer_bias2 = buffer_bias_local2;
  buffer_config2 = buffer_config_local2;

  inBufVec2.push_back(buffer_cin2);
  inBufVec2.push_back(buffer_cin2);
  inBufVec2.push_back(buffer_weight2);
  inBufVec2.push_back(buffer_bias2);
  inBufVec2.push_back(buffer_config2);
  outBufVec2.push_back(buffer_cin2);

  GlobMem_BUF_in0_Ext3.param = 0;
  GlobMem_BUF_in0_Ext3.flags = XCL_MEM_DDR_BANK0;
  GlobMem_BUF_in0_Ext3.obj = cin3.data();

  GlobMem_BUF_in1_Ext3.param = 0;
  GlobMem_BUF_in1_Ext3.flags = XCL_MEM_DDR_BANK1;
  GlobMem_BUF_in1_Ext3.obj = weight.data();

  GlobMem_BUF_in2_Ext3.param = 0;
  GlobMem_BUF_in2_Ext3.flags = XCL_MEM_DDR_BANK1;
  GlobMem_BUF_in2_Ext3.obj = bias.data();

  GlobMem_BUF_in3_Ext3.param = 0;
  GlobMem_BUF_in3_Ext3.flags = XCL_MEM_DDR_BANK0;
  GlobMem_BUF_in3_Ext3.obj = config.data();

  cl::Buffer buffer_cin_local3(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
      cin_size * sizeof(data_t0), &GlobMem_BUF_in0_Ext3);
  cl::Buffer buffer_weight_local3(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
      weight_size * sizeof(data_t1), &GlobMem_BUF_in1_Ext3);
  cl::Buffer buffer_bias_local3(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
      bias_size * sizeof(data_t2), &GlobMem_BUF_in2_Ext3);
  cl::Buffer buffer_config_local3(
      context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
      config_size * sizeof(unsigned int), &GlobMem_BUF_in3_Ext3);

  buffer_cin3 = buffer_cin_local3;
  buffer_weight3 = buffer_weight_local3;
  buffer_bias3 = buffer_bias_local3;
  buffer_config3 = buffer_config_local3;

  inBufVec3.push_back(buffer_cin3);
  inBufVec3.push_back(buffer_cin3);
  inBufVec3.push_back(buffer_weight3);
  inBufVec3.push_back(buffer_bias3);
  inBufVec3.push_back(buffer_config3);
  outBufVec3.push_back(buffer_cin3);

}

std::vector<data_t0, aligned_allocator<data_t0>> Sacc::cin2(cin_size);
std::vector<data_t0, aligned_allocator<data_t0>> Sacc::cin3(cin_size);
std::vector<data_t0, aligned_allocator<data_t0>> Sacc::cin(cin_size);
std::vector<data_t1, aligned_allocator<data_t1>> Sacc::weight(weight_size);
std::vector<data_t2, aligned_allocator<data_t2>> Sacc::bias(bias_size);
std::vector<unsigned int, aligned_allocator<unsigned int>> Sacc::config(
    config_size);
