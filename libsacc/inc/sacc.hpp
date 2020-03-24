#ifndef SYSTOLIC_ARRAY_H_
#define SYSTOLIC_ARRAY_H_

// clang-format off
#include "xcl2.hpp"
#include "sacc_types.h"
#include "sacc_params.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/types.h"
// clang-format on

using namespace tensorflow;

REGISTER_OP("Sacc")
    .Attr("N: int=1")
    .Input("to_sacc: N * float")
    .Output("from_sacc: N * float")
    .SetShapeFn([](::tensorflow::shape_inference::InferenceContext *c) {
      shape_inference::ShapeHandle output_shape;
      // refer to https://github.com/tensorflow/tensorflow/blob/master/tensorflow/core/framework/shape_inference.h
      output_shape = c->MakeShape({1, STAGE2L_OUT_H,  STAGE2L_OUT_W,
                                   (STAGE2R_OUT_NUM + STAGE2L_OUT_NUM)});
      for (int i = 0; i < c->num_inputs(); i++)
        c->set_output(i, output_shape);
      return Status::OK();
    });

class Sacc : public OpKernel {
public:
  explicit Sacc(OpKernelConstruction *context);
  void Compute(OpKernelContext *context) override;

private:
  static std::string lib_base_path;
  static std::string inst_file_path;
  static std::string weight_file_path;
  static std::string bias_file_path;
  static std::string xcl_binary_path;
  // Create Program and Kernel
  std::vector<cl::Device> devices;
  // Hardware Test
  cl::Context context;
  cl::CommandQueue q;
  cl::CommandQueue q2;
  cl::CommandQueue q3;
  std::string device_name;
  cl::Program::Binaries bins;

  cl::Program program;
  cl::Kernel krnl_vadd;
  cl::Kernel krnl_vadd2;
  cl::Kernel krnl_vadd3;
  // multi ddr
  cl_mem_ext_ptr_t GlobMem_BUF_in0_Ext2;
  cl_mem_ext_ptr_t GlobMem_BUF_in1_Ext2;
  cl_mem_ext_ptr_t GlobMem_BUF_in2_Ext2;
  cl_mem_ext_ptr_t GlobMem_BUF_in3_Ext2;
  
  

  // Allocate Buffer in Global Memory
  std::vector<cl::Memory> inBufVec2, outBufVec2;

  cl::Buffer buffer_cin2;
  cl::Buffer buffer_weight2;
  cl::Buffer buffer_bias2;
  cl::Buffer buffer_config2;
  static std::vector<data_t0, aligned_allocator<data_t0>> cin2;
  
  cl_mem_ext_ptr_t GlobMem_BUF_in0_Ext3;
  cl_mem_ext_ptr_t GlobMem_BUF_in1_Ext3;
  cl_mem_ext_ptr_t GlobMem_BUF_in2_Ext3;
  cl_mem_ext_ptr_t GlobMem_BUF_in3_Ext3;
  
  

  // Allocate Buffer in Global Memory
  std::vector<cl::Memory> inBufVec3, outBufVec3;

  cl::Buffer buffer_cin3;
  cl::Buffer buffer_weight3;
  cl::Buffer buffer_bias3;
  cl::Buffer buffer_config3;
  static std::vector<data_t0, aligned_allocator<data_t0>> cin3;
  

  // multi ddr
  cl_mem_ext_ptr_t GlobMem_BUF_in0_Ext;
  cl_mem_ext_ptr_t GlobMem_BUF_in1_Ext;
  cl_mem_ext_ptr_t GlobMem_BUF_in2_Ext;
  cl_mem_ext_ptr_t GlobMem_BUF_in3_Ext;
  
  

  // Allocate Buffer in Global Memory
  std::vector<cl::Memory> inBufVec, outBufVec;

  cl::Buffer buffer_cin;
  cl::Buffer buffer_weight;
  cl::Buffer buffer_bias;
  cl::Buffer buffer_config;

  static std::vector<data_t0, aligned_allocator<data_t0>> cin;
  static std::vector<data_t1, aligned_allocator<data_t1>> weight;
  static std::vector<data_t2, aligned_allocator<data_t2>> bias;
  static std::vector<unsigned int, aligned_allocator<unsigned int>>
      config;

  void send_to_process();

  inline void reformat_input_data_layout(auto const &);

  inline void reformat_output_data_layout(auto &);
  
  inline void reformat_input_data_layout2(auto const &);

  inline void reformat_output_data_layout2(auto &);
  
  inline void reformat_input_data_layout3(auto const &);

  inline void reformat_output_data_layout3(auto &);
  
  
  /**
   * load bias from file
   * @param bias : stores values
   * @param input_file_path : specify the file path
   */
  inline void load_bias(
      std::vector<data_t2, aligned_allocator<data_t2>> &bias,
      const std::string &input_file_path);

  /**
   * load weight from file
   * @param weight : stores values
   * @param input_file_path : specify the file path
   */
  inline void load_weights(
      std::vector<data_t1, aligned_allocator<data_t1>> &weight,
      const std::string &input_file_path);
  /* *
   * load instruction for SA accelerator
   * @param config : store the value
   * @param input_file_path : specify the file path
   */
  inline void load_inst(
          std::vector<unsigned int, aligned_allocator<unsigned int>> &config,
      const std::string &input_file_path);
  inline void init();
};

REGISTER_KERNEL_BUILDER(Name("Sacc").Device(DEVICE_CPU), Sacc);
#endif
