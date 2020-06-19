#ifndef __POSE_H_
#define __POSE_H_

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <ctime>
#include <cstring>

#include "ap_fixed.h"
#include "hls_stream.h"
#include "ap_int.h"
#include <assert.h>

#include "params.h"


using namespace std;
using namespace hls;

// Macros
/*
 * Set the module lanes to the same number currently
 */
#define SIMD_LANE 8
#define DATA_LDWR_LANE (SIMD_LANE)
#define DEPTH_CONV_LANE (SIMD_LANE)
#define CONV_LANE (SIMD_LANE)
#define RELU_LANE (SIMD_LANE)
#define POOL_LANE (SIMD_LANE)
#define UPSAMPLE_LANE (SIMD_LANE) // reserved
#define INTER_LOAD_LANE (SIMD_LANE)
#define INTER_WRITE_LANE (SIMD_LANE)
//#define DATA_WRITE_LANE 8

#define CONFIG_PARAMS  32
#define INST_PER_LAYER 5

// Data Types
// primtive data types
typedef float data_t0; // cin, cout
typedef float data_t1; // weight
typedef float data_t2; // bias
typedef unsigned int data_t3; // inst
#define DATA_W0 32
#define DATA_W1 32
#define DATA_W2 32
#define DATA_W3 32
typedef ap_uint<512> bus_t0;
typedef ap_uint<512> bus_t1;
typedef ap_uint<512> bus_t2;
typedef ap_uint<32> bus_t3;
#define BUS_PACK_FACTOR0 (512 / DATA_W0)
#define BUS_PACK_FACTOR1 (512 / DATA_W1)
#define BUS_PACK_FACTOR2 (512 / DATA_W2)
#define BUS_PACK_FACTOR3 (32 / DATA_W3)

#define DATA_SEL_FACTOR0 (BUS_PACK_FACTOR0 / SIMD_LANE)
#define DATA_SEL_FACTOR1 (BUS_PACK_FACTOR1 / SIMD_LANE)
#define DATA_SEL_FACTOR2 (BUS_PACK_FACTOR2 / SIMD_LANE)

typedef unsigned int uint;
typedef ap_uint<192> ConfigInst;
// Inst0: in_num_hw  | out_num_hw    | in_h_hw     | in_w_hw     | out_h_hw | out_w_hw
// Inst1: in_num     | out_num       | in_h        | in_w        | out_h    | out_w
// Inst2: cin_offset | weight_offset | bias_offset | cout_offset | filter_s1, filter_s2 | stride
// Inst3: layer_en: conv_1st_en, depth_conv_en, conv_en, relu_en, relu6_en, pool_en, up_sample_en, bias_en, inter_load_en, inter_write_en, batch_norm_en_conv, load_prev_cin, batch_norm_en_depth | prev_cin_offset | in_num_t, out_num_t | in_h_t | in_w_t | nxt_layer_batch
// Inst4: task_num1 | task_num2 | local_accum_num | local_reg_num | row_il_factor | col_il_factor

typedef ap_uint<DATA_W0 * DEPTH_CONV_LANE> CinLoadData0Type;

typedef ap_uint<DATA_W1 * DEPTH_CONV_LANE> WeightLoadData0Type;
typedef ap_uint<DATA_W1 * CONV_LANE> WeightLoadData1Type;
typedef ap_uint<DATA_W1 * RELU_LANE> WeightLoadData2Type;

typedef ap_uint<DATA_W0 * INTER_LOAD_LANE> InterLoadData0Type;
typedef ap_uint<DATA_W0 * CONV_LANE> DepthConvData0Type;
typedef ap_uint<DATA_W0 * RELU_LANE> ConvData0Type;
typedef ap_uint<DATA_W0 * POOL_LANE> ReluData0Type;
typedef ap_uint<DATA_W0 * DATA_LDWR_LANE> PoolData0Type; 
typedef ap_uint<DATA_W0 * INTER_WRITE_LANE> InterWriteData0Type;
typedef ap_uint<DATA_W0 * INTER_WRITE_LANE> InterWriteData1Type;
typedef ap_uint<DATA_W0 * UPSAMPLE_LANE> UpsampleData0Type;
// typedef ap_uint<CIN_DATA_W*DATA_WRITE_LANE> UpsampleData0Type;

extern "C"{
void top_kernel(
  bus_t0 *global_cin,
  bus_t0 *global_prev_cin,
  bus_t0 *global_cout,
  bus_t1 *global_weight,
  bus_t2 *global_bias,
  bus_t3 *layer_config
);
}



const uint out_num = 57;
const uint out_h = 48;
const uint out_w = 48;

const uint out_num2 = 32;
const uint out_h2 = 24;
const uint out_w2 = 24;

void mobilenet_preprocess(
  data_t0* cin_hw,
  data_t1* weight_hw,
  data_t2* bias_hw,
  data_t0  LAYER_out[out_h][out_w][out_num]
);

void instInit(uint config[LAYER_NUM*CONFIG_PARAMS]);

void openpose_postprocess(
  data_t0* cin_hw,
  data_t0  LAYER_out[STAGE2L_OUT_H][STAGE2L_OUT_W][STAGE2R_OUT_NUM + STAGE2L_OUT_NUM]
);



void mobilenet_postprocess(
  data_t0* cin_hw,
  data_t0  LAYER_out[24][24][out_num2],
  uint offset,
  uint out_h_hw,
  uint out_h,
  uint out_w_hw,
  uint out_w,
  uint out_num_t,
  uint out_num_hw,
  uint out_num,
  uint k
);


void extract_layer(
  data_t0* cin_hw,
  uint*    config,
  uint     layer_id
);

void compute_layer();

/**
 * Helper Functions
 * - max
 * - reinterpret
 * - stencil_w3
 * - stencil_w1
 * - maxpool_w2 
 * - upsample_w2
 */
#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)>(b)?(b):(a))

// systolic array kernel
void kernel(
  stream<DepthConvData0Type>  &fifo_cin,
  stream<WeightLoadData1Type> &fifo_weight,
  stream<ConvData0Type>       &fifo_cout,
  stream<ConfigInst>          &fifo_config_in,
  stream<ConfigInst>          &fifo_config_out
);

template <typename To, typename From>
inline To Reinterpret(const From& val){
  return reinterpret_cast<const To&>(val);
}


/**
 * Function name: stencil_w3
 * Function description: This function does the computation for depth_conv module with filter size of 3
 *                       The computation pattern is like a stencil computation
 */
template <class T_data_t0, class T_data_t1, int T_IN_NUM_T, int T_IN_H_T, int T_IN_W_T, int T_UNROLL, int T_WS, int T_DATA_WIDTH0, int T_DATA_WIDTH1>
void stencil_w3(
  hls::stream<ap_uint<T_DATA_WIDTH0 * T_UNROLL> > &fifo_in,
  T_data_t1                                       weights[T_IN_NUM_T / T_UNROLL][T_UNROLL][K_T][K_T],  
  hls::stream<ap_uint<T_DATA_WIDTH0 * T_UNROLL> > &fifo_out,
  uint                                            stride,
  uint                                            layer_in_num_t,
  uint                                            layer_in_h_t,
  uint                                            layer_in_w_t
){
#pragma HLS INLINE off
  T_data_t0 line_buf1[T_UNROLL][T_IN_W_T];
  T_data_t0 line_buf2[T_UNROLL][T_IN_W_T];
  T_data_t0 line_buf3[T_UNROLL][T_WS];
#pragma HLS ARRAY_PARTITION variable=line_buf1 dim=1 complete
#pragma HLS ARRAY_PARTITIOn variable=line_buf1 dim=2 complete
#pragma HLS ARRAY_PARTITION variable=line_buf2 dim=1 complete
#pragma HLS ARRAY_PARTITION variable=line_buf2 dim=2 complete
#pragma HLS ARRAY_PARTITION variable=line_buf3 dim=1 complete
#pragma HLS ARRAY_PARTITION variable=line_buf3 dim=2 complete

  bool col_skip = 0;
  bool row_skip = 0;
  bool col_strip_skip = 0;
  bool row_strip_skip = 0;
  uint trans_cnt = 0;
  uint inner_trans_cnt = 0;

  ap_uint<T_DATA_WIDTH0> utmp[T_UNROLL];
#pragma HLS ARRAY_PARTITION variable=utmp complete
  T_data_t0 sums[T_UNROLL];
#pragma HLS ARRAY_PARTITION variable=sums complete

  assert ((layer_in_w_t == 26) || (layer_in_w_t == 50) || (layer_in_w_t == 98));
  int oo = 0;
  int iter = 0;
  int oo_bound = layer_in_num_t / T_UNROLL;
  int iter_bound = layer_in_h_t * layer_in_w_t + (T_WS - 1) * layer_in_w_t + T_WS - 1;
  int bound = oo_bound * iter_bound;

  for (int total_iter = 0; total_iter < bound; total_iter++){
#pragma HLS PIPELINE II=1 
    if (iter == 0){
      trans_cnt = 0;
      inner_trans_cnt = 0;
    }

    ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data_in;
    if (iter < layer_in_h_t * layer_in_w_t){
      //fifo_in.read_nb(wide_data_in);
    	wide_data_in = fifo_in.read();
    }

    for (int dup = 0; dup < T_UNROLL; dup++){
#pragma HLS UNROLL
      T_data_t0 tmp1 = 0;
	  // add a mux to support dynamic tiling
      if (layer_in_w_t == 26) {
        tmp1 = line_buf1[dup][25];
      } else if (layer_in_w_t == 50) {
        tmp1 = line_buf1[dup][49];
      } else if (layer_in_w_t == 98) {
        tmp1 = line_buf1[dup][97];
      }
      
      T_data_t0 tmp2 = 0;
      if (layer_in_w_t == 26) {
        tmp2 = line_buf2[dup][25];
      } else if (layer_in_w_t == 50) {
        tmp2 = line_buf2[dup][49];
      } else if (layer_in_w_t == 98) {
        tmp2 = line_buf2[dup][97];
      }
	  
	  // define the line buffers
      for (int i = T_IN_W_T - 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf1[dup][i] = line_buf1[dup][i - 1];
        line_buf2[dup][i] = line_buf2[dup][i - 1];
      }
      for (int i = T_WS - 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf3[dup][i] = line_buf3[dup][i - 1];
      }
      
      if (iter < layer_in_h_t * layer_in_w_t){ 
		// Unpack the data based on the SIMD_LANE
        ap_uint<T_DATA_WIDTH0> sel_tmp;
#if DEPTH_CONV_LANE == 16
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
          case 4:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 5 - 1, T_DATA_WIDTH0 * 4);
            break;
          case 5:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 6 - 1, T_DATA_WIDTH0 * 5);
            break;
          case 6:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 7 - 1, T_DATA_WIDTH0 * 6);
            break;
          case 7:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 8 - 1, T_DATA_WIDTH0 * 7);
            break;
          case 8:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 9 - 1, T_DATA_WIDTH0 * 8);
            break;
          case 9:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 10 - 1, T_DATA_WIDTH0 * 9);
            break;
          case 10:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 11 - 1, T_DATA_WIDTH0 * 10);
            break;
          case 11:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 12 - 1, T_DATA_WIDTH0 * 11);
            break;
          case 12:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 13 - 1, T_DATA_WIDTH0 * 12);
            break;
          case 13:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 14 - 1, T_DATA_WIDTH0 * 13);
            break;
          case 14:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 15 - 1, T_DATA_WIDTH0 * 14);
            break;
          case 15:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 16 - 1, T_DATA_WIDTH0 * 15);
            break;
        }
#elif DEPTH_CONV_LANE == 8
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
          case 4:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 5 - 1, T_DATA_WIDTH0 * 4);
            break;
          case 5:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 6 - 1, T_DATA_WIDTH0 * 5);
            break;
          case 6:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 7 - 1, T_DATA_WIDTH0 * 6);
            break;
          case 7:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 8 - 1, T_DATA_WIDTH0 * 7);
            break;
        }
#elif DEPTH_CONV_LANE == 4
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
        }
#elif DEPTH_CONV_LANE == 2
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
        }
#elif DEPTH_CONV_LANE == 1
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
        }
#endif        
        line_buf1[dup][0] = Reinterpret<T_data_t0>(sel_tmp);
      } else {      
        line_buf1[dup][0] = 0.0;
      }
      line_buf2[dup][0] = tmp1;
      line_buf3[dup][0] = tmp2;

      // mults
      T_data_t0 prod_0_0 = line_buf3[dup][T_WS - 1] * weights[oo][dup][0][0];
      T_data_t0 prod_0_1 = line_buf3[dup][T_WS - 2] * weights[oo][dup][0][1];
      T_data_t0 prod_0_2 = line_buf3[dup][T_WS - 3] * weights[oo][dup][0][2];
      T_data_t0 prod_1_0 = line_buf2[dup][T_WS - 1] * weights[oo][dup][1][0];
      T_data_t0 prod_1_1 = line_buf2[dup][T_WS - 2] * weights[oo][dup][1][1];
      T_data_t0 prod_1_2 = line_buf2[dup][T_WS - 3] * weights[oo][dup][1][2];
      T_data_t0 prod_2_0 = line_buf1[dup][T_WS - 1] * weights[oo][dup][2][0];
      T_data_t0 prod_2_1 = line_buf1[dup][T_WS - 2] * weights[oo][dup][2][1];
      T_data_t0 prod_2_2 = line_buf1[dup][T_WS - 3] * weights[oo][dup][2][2];
    
      // adds
      T_data_t0 sum_0_0 = prod_0_0 + prod_0_1;
      T_data_t0 sum_0_1 = prod_0_2 + prod_1_0;
      T_data_t0 sum_0_2 = prod_1_1 + prod_1_2;
      T_data_t0 sum_0_3 = prod_2_0 + prod_2_1;
      T_data_t0 sum_1_0 = sum_0_0 + sum_0_1;
      T_data_t0 sum_1_1 = sum_0_2 + sum_0_3;
      T_data_t0 sum_2_0 = sum_1_0 + sum_1_1;
      T_data_t0 sum_3_0 = sum_2_0 + prod_2_2;

      sums[dup]  = sum_3_0;
    }

	// If the first output element is ready, fill the output FIFO
	// The first output gets ready when all the line buffers are full
    if (iter >= (T_WS - 1) * layer_in_w_t + T_WS - 1){     
	  // Check whether the row/column should be skipped due to stride > 1
      col_skip = (inner_trans_cnt % stride != stride - 1);
      row_skip = ((inner_trans_cnt / (layer_in_w_t - (T_WS - 1))) % stride != stride - 1);
      col_strip_skip = trans_cnt % layer_in_w_t >= (layer_in_w_t - (T_WS - 1));
      row_strip_skip = trans_cnt / layer_in_w_t >= (layer_in_h_t - (T_WS - 1));
      if (!col_strip_skip && !row_strip_skip){
        if (!col_skip && !row_skip){

        for (int ii = 0; ii < T_UNROLL; ii++){
#pragma HLS UNROLL
          T_data_t0 sum_tmp = sums[ii];
#ifdef DEBUG_stencil
  cout << "ii: " << ii << " sums: " << sums[ii] << endl;
#endif
          ap_uint<T_DATA_WIDTH0> utmp_tmp = Reinterpret<ap_uint<T_DATA_WIDTH0> >(sum_tmp);
          utmp[ii] = utmp_tmp;
        }          
        ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data = (
#if DEPTH_CONV_LANE == 16
            utmp[15], utmp[14], utmp[13], utmp[12],
            utmp[11], utmp[10], utmp[9], utmp[8],
            utmp[7], utmp[6], utmp[5], utmp[4],
            utmp[3], utmp[2], utmp[1], utmp[0]
#elif DEPTH_CONV_LANE == 8          
            utmp[7], utmp[6], utmp[5], utmp[4],
            utmp[3], utmp[2], utmp[1], utmp[0]
#elif DEPTH_CONV_LANE == 4
            utmp[3], utmp[2], utmp[1], utmp[0]
#elif DEPTH_CONV_LANE == 2           
            utmp[1], utmp[0]
#elif DEPTH_CONV_LANE == 1            
            utmp[0]
#endif
            );
        fifo_out.write(wide_data); 
      
        }
        inner_trans_cnt++;
      }
      trans_cnt++;
    }

    iter++;
    if (iter == iter_bound){
      iter = 0;
      oo++;
      if (oo == oo_bound){
        oo = 0;
      }
    }
  }
}


/**
 * Function name: stencil_w1
 * Function description: This function does the computation for depth_conv module with filter size of 1
 *                       The computation pattern is like a stencil computation
 */
template <class T_data_t0, class T_data_t1, int T_IN_NUM_T, int T_IN_H_T, int T_IN_W_T, int T_UNROLL, int T_WS, int T_DATA_WIDTH0, int T_DATA_WIDTH1>
void stencil_w1(
  hls::stream<ap_uint<T_DATA_WIDTH0 * T_UNROLL> > &fifo_in,
  T_data_t1                                       weights[T_IN_NUM_T / T_UNROLL][T_UNROLL][K_T][K_T],
  hls::stream<ap_uint<T_DATA_WIDTH0 * T_UNROLL> > &fifo_out,
  uint                                            stride,
  uint                                            layer_in_num_t,
  uint                                            layer_in_h_t,
  uint                                            layer_in_w_t
){
#pragma HLS INLINE off
  T_data_t0 line_buf1[T_UNROLL][T_WS];
#pragma HLS ARRAY_PARTITION variable=line_buf1 dim=1 complete
#pragma HLS ARRAY_PARTITION variable=line_buf1 dim=2 complete


  ap_uint<T_DATA_WIDTH0> utmp[T_UNROLL];
#pragma HLS ARRAY_PARTITION variable=utmp complete
  T_data_t0 sums[T_UNROLL];
#pragma HLS ARRAY_PARTITION variable=sums complete

  bool col_skip = 0;
  bool row_skip = 0;
  bool col_strip_skip = 0;
  bool row_strip_skip = 0;

  assert ((layer_in_w_t == 24) || (layer_in_w_t == 48) || (layer_in_w_t == 96));
  
  int oo = 0;
  int iter = 0;
  int oo_bound = layer_in_num_t / T_UNROLL;
  int iter_bound = layer_in_h_t * layer_in_w_t + (T_WS - 1) * layer_in_w_t + T_WS - 1;
  int total_bound = oo_bound * iter_bound;
  uint trans_cnt = 0;
  uint inner_trans_cnt = 0;

  for (int total_iter = 0; total_iter < total_bound; total_iter++){
#pragma HLS PIPELINE II=1  
    if (iter == 0){
      trans_cnt = 0;
      inner_trans_cnt = 0;
    }

    ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data_in;
    if (iter < layer_in_h_t * layer_in_w_t){
      //fifo_in.read_nb(wide_data_in);
      wide_data_in = fifo_in.read();
    }

    for (int dup = 0; dup < T_UNROLL; dup++){
      for (int i = T_WS - 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf1[dup][i] = line_buf1[dup][i - 1];
      }
      
      if (iter < layer_in_h_t * layer_in_w_t){ 
		// Unpack the data based on the SIMD_LANE
        ap_uint<T_DATA_WIDTH0> sel_tmp;
#if DEPTH_CONV_LANE == 16
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
          case 4:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 5 - 1, T_DATA_WIDTH0 * 4);
            break;
          case 5:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 6 - 1, T_DATA_WIDTH0 * 5);
            break;
          case 6:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 7 - 1, T_DATA_WIDTH0 * 6);
            break;
          case 7:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 8 - 1, T_DATA_WIDTH0 * 7);
            break;
          case 8:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 9 - 1, T_DATA_WIDTH0 * 8);
            break;
          case 9:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 10 - 1, T_DATA_WIDTH0 * 9);
            break;
          case 10:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 11 - 1, T_DATA_WIDTH0 * 10);
            break;
          case 11:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 12 - 1, T_DATA_WIDTH0 * 11);
            break;
          case 12:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 13 - 1, T_DATA_WIDTH0 * 12);
            break;
          case 13:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 14 - 1, T_DATA_WIDTH0 * 13);
            break;
          case 14:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 15 - 1, T_DATA_WIDTH0 * 14);
            break;
          case 15:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 16 - 1, T_DATA_WIDTH0 * 15);
            break;
        }       
#elif DEPTH_CONV_LANE == 8
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
          case 4:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 5 - 1, T_DATA_WIDTH0 * 4);
            break;
          case 5:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 6 - 1, T_DATA_WIDTH0 * 5);
            break;
          case 6:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 7 - 1, T_DATA_WIDTH0 * 6);
            break;
          case 7:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 8 - 1, T_DATA_WIDTH0 * 7);
            break;
        }
#elif DEPTH_CONV_LANE == 4
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
        }
#elif DEPTH_CONV_LANE == 2
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
        }
#elif DEPTH_CONV_LANE == 1
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
        }
#endif        
        line_buf1[dup][0] = Reinterpret<T_data_t0>(sel_tmp);
      } else {      
        line_buf1[dup][0] = 0.0;
      }

      // mults
      T_data_t0 prod_0_0 = line_buf1[dup][T_WS - 1] * weights[oo][dup][0][0];
      sums[dup]  = prod_0_0;
    }

	// If the first output element is ready, fill the output FIFO
	// The first output gets ready when all the line buffers are full
    if (iter >= (T_WS - 1) * layer_in_w_t + T_WS - 1){      
      // Check whether the row/column should be skipped due to stride > 1
	  col_skip = (inner_trans_cnt % stride != stride - 1);
      row_skip = ((inner_trans_cnt / (layer_in_w_t - (T_WS - 1))) % stride != stride - 1);
      col_strip_skip = trans_cnt % layer_in_w_t >= (layer_in_w_t - (T_WS - 1));
      row_strip_skip = trans_cnt / layer_in_w_t >= (layer_in_h_t - (T_WS - 1));
     
      if (!col_strip_skip && !row_strip_skip){
        if (!col_skip && !row_skip){

        for (int ii = 0; ii < T_UNROLL; ii++){
          T_data_t0 sum_tmp = sums[ii];
          ap_uint<T_DATA_WIDTH0> utmp_tmp = Reinterpret<ap_uint<T_DATA_WIDTH0> >(sum_tmp);
          utmp[ii] = utmp_tmp;
        }          
        ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data = (
#if DEPTH_CONV_LANE == 16
            utmp[15], utmp[14], utmp[13], utmp[12],
            utmp[11], utmp[10], utmp[9], utmp[8],
            utmp[7], utmp[6], utmp[5], utmp[4],
            utmp[3], utmp[2], utmp[1], utmp[0]           
#elif DEPTH_CONV_LANE == 8          
            utmp[7], utmp[6], utmp[5], utmp[4],
            utmp[3], utmp[2], utmp[1], utmp[0]
#elif DEPTH_CONV_LANE == 4
            utmp[3], utmp[2], utmp[1], utmp[0]
#elif DEPTH_CONV_LANE == 2           
            utmp[1], utmp[0]
#elif DEPTH_CONV_LANE == 1            
            utmp[0]
#endif
            );
        fifo_out.write(wide_data); 

        }
        inner_trans_cnt++;
      }
      trans_cnt++;
    }

    iter++;
    if (iter == iter_bound){
      iter = 0;
      oo++;
      if (oo == oo_bound){
        oo = 0;
      }
    }
  }
}


/**
 * Function name: upsample_w2
 * Function description: This function does the computation for bilinear upsampling to 2x
 *                       The computation pattern is like a stencil computation
 */
template <class T_data_t0, int T_IN_H_T, int T_IN_W_T, int T_UNROLL, int T_WS, int T_DATA_WIDTH0>
void upsample_w2(
  hls::stream<ap_uint<T_DATA_WIDTH0 * T_UNROLL> > &fifo_in,
  hls::stream<ap_uint<T_DATA_WIDTH0 * T_UNROLL> > &fifo_out1,
  hls::stream<ap_uint<T_DATA_WIDTH0 * T_UNROLL> > &fifo_out2,
  uint                                            stride,  
  bool                                            up_en,
  uint                                            layer_out_num_t,
  uint                                            layer_in_h_t,
  uint                                            layer_in_w_t
){
#pragma HLS INLINE off
  T_data_t0 line_buf1[T_UNROLL][T_IN_W_T+T_WS];
  T_data_t0 line_buf2[T_UNROLL][T_WS];
#pragma HLS ARRAY_PARTITION variable=line_buf1 dim=1 complete
#pragma HLS ARRAY_PARTITION variable=line_buf1 dim=2 complete
#pragma HLS ARRAY_PARTITION variable=line_buf2 dim=1 complete
#pragma HLS ARRAY_PARTITION variable=line_buf2 dim=2 complete

  T_data_t0 line_buf_inp[T_UNROLL][T_IN_W_T+T_WS];
#pragma HLS ARRAY_PARTITION variable=line_buf_inp dim=1 complete
#pragma HLS ARRAY_PARTITION variable=line_buf_inp dim=2 complete

  bool col_skip = 0;
  bool row_skip = 0;
  uint trans_cnt = 0;

#ifdef DEBUG
  uint max_pool_cout_cnt = 0;
#endif

  ap_uint<T_DATA_WIDTH0> utmp[T_WS][T_UNROLL];
#pragma HLS ARRAY_PARTITION variable=utmp dim=1 complete
#pragma HLS ARRAY_PARTITION variable=utmp dim=2 complete
  T_data_t0 sums[T_UNROLL][T_WS];
#pragma HLS ARRAY_PARTITION variable=sums dim=1 complete
#pragma HLS ARRAY_PARTITION variable=sums dim=2 complete

  ap_uint<T_DATA_WIDTH0> utmp_inp[T_WS][T_UNROLL];
#pragma HLS ARRAY_PARTITION variable=utmp_inp dim=1 complete
#pragma HLS ARRAY_PARTITION variable=utmp_inp dim=2 complete
  T_data_t0 sums_inp[T_UNROLL][T_WS];
#pragma HLS ARRAY_PARTITION variable=sums_inp dim=1 complete
#pragma HLS ARRAY_PARTITION variable=sums_inp dim=2 complete

  int oo =0;
  int iter = 0;
  int oo_bound = layer_out_num_t / T_UNROLL;
  int iter_bound = layer_in_h_t * T_IN_W_T + (T_WS - 1) * T_IN_W_T + T_WS - 1;
  int total_bound = oo_bound * iter_bound;
  int end_row = 2 * T_IN_W_T;

  for (int total_iter = 0; total_iter < total_bound; total_iter++){
#pragma HLS PIPELINE II=1    
    if (iter == 0){
      trans_cnt = 0;
    }

    ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data_in;
    if (iter < layer_in_h_t * T_IN_W_T){
      wide_data_in = fifo_in.read();
#ifdef DEBUG_w2_inp
  cout << iter << " in: " << wide_data_in << endl;
#endif
    }

    for (int dup = 0; dup < T_UNROLL; dup++){
	  // define the line buffers
      T_data_t0 tmp1 = line_buf1[dup][T_IN_W_T - 1];
      for (int i = T_IN_W_T - 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf1[dup][i] = line_buf1[dup][i - 1];
      }
      for (int i = T_WS - 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf2[dup][i] = line_buf2[dup][i - 1];
      }
      for (int i = T_IN_W_T + 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf_inp[dup][i] = line_buf_inp[dup][i - 1];
      }
      
	  
      if (iter < layer_in_h_t * T_IN_W_T){
        ap_uint<T_DATA_WIDTH0> sel_tmp;
#if POOL_LANE == 16
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
          case 4:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 5 - 1, T_DATA_WIDTH0 * 4);
            break;
          case 5:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 6 - 1, T_DATA_WIDTH0 * 5);
            break;
          case 6:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 7 - 1, T_DATA_WIDTH0 * 6);
            break;
          case 7:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 8 - 1, T_DATA_WIDTH0 * 7);
            break;
          case 8:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 9 - 1, T_DATA_WIDTH0 * 8);
            break;
          case 9:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 10 - 1, T_DATA_WIDTH0 * 9);
            break;
          case 10:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 11 - 1, T_DATA_WIDTH0 * 10);
            break;
          case 11:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 12 - 1, T_DATA_WIDTH0 * 11);
            break;
          case 12:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 13 - 1, T_DATA_WIDTH0 * 12);
            break;
          case 13:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 14 - 1, T_DATA_WIDTH0 * 13);
            break;
          case 14:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 15 - 1, T_DATA_WIDTH0 * 14);
            break;
          case 15:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 16 - 1, T_DATA_WIDTH0 * 15);
            break;           
        }
#elif POOL_LANE == 8        
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
          case 4:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 5 - 1, T_DATA_WIDTH0 * 4);
            break;
          case 5:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 6 - 1, T_DATA_WIDTH0 * 5);
            break;
          case 6:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 7 - 1, T_DATA_WIDTH0 * 6);
            break;
          case 7:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 8 - 1, T_DATA_WIDTH0 * 7);
            break;
        }
#elif POOL_LANE == 4
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
        }
#elif POOL_LANE == 2
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
        }
#elif POOL_LANE == 1
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
        }
#endif     
        line_buf1[dup][0] = Reinterpret<T_data_t0>(sel_tmp);
        line_buf_inp[dup][0] = Reinterpret<T_data_t0>(sel_tmp);
      } else {      
        line_buf1[dup][0] = 0.0;
        line_buf_inp[dup][0] = 0.0;
      }
      line_buf2[dup][0] = tmp1;

      // maxs 
      /*T_data_t0 mux_0_0 = max(line_buf2[dup][T_WS - 1], line_buf2[dup][T_WS - 2]);
      T_data_t0 mux_0_1 = max(line_buf1[dup][T_WS - 1], line_buf1[dup][T_WS - 2]);
      T_data_t0 mux_1_0 = max(mux_0_0, mux_0_1);*/
      
	  // compute the data for the even rows
      T_data_t0 data1 = 0.5*(line_buf2[dup][T_WS - 1] + line_buf1[dup][T_WS - 1]);
      T_data_t0 data2 = 0.25*(line_buf2[dup][T_WS - 1] + line_buf1[dup][T_WS - 1] + line_buf2[dup][T_WS - 2] + line_buf1[dup][T_WS - 2]);
#ifdef DEBUG_w2
      if(dup == 0) 
        cout << "first: " << line_buf2[dup][T_WS - 1] << " " << line_buf1[dup][T_WS - 1] << " " <<line_buf2[dup][T_WS - 2] << " " << line_buf1[dup][T_WS - 2] << " " << data1 << " " << data2 << endl;
#endif
      
	  // compute the data for the odd rows
      T_data_t0 data1_inp = line_buf_inp[dup][T_IN_W_T + 1];
      T_data_t0 data2_inp = 0.5*(line_buf_inp[dup][T_IN_W_T] + line_buf_inp[dup][T_IN_W_T + 1]);

#ifdef DEBUG_w2_inp
      if(dup == 0) 
        cout << "second: " << line_buf_inp[dup][layer_in_w_t + 1] << " " << line_buf_inp[dup][layer_in_w_t] << " " << data1_inp << " " << data2_inp << endl;
#endif

      if (up_en == 1){
		
		// If it is the last row of input, the last two rows of the output should be identical
        if(iter >= (layer_in_h_t + T_WS - 2) * T_IN_W_T + T_WS - 1) {
          data1 = data1_inp;
          data2 = data2_inp;
        }
		
		// even rows
        sums[dup][T_WS - 2] = data1;
        sums[dup][T_WS - 1] = data2;
        
        // odd rows
        sums_inp[dup][T_WS - 2] = data1_inp;
        if(iter == end_row){
          sums[dup][T_WS - 1] = data1;
          sums_inp[dup][T_WS - 1] = data1_inp;
        } else sums_inp[dup][T_WS - 1] = data2_inp;
      } else {
        sums[dup][0] = line_buf1[dup][T_WS - 2];
      }
    }
    
    if(iter == end_row) end_row += T_IN_W_T;

	// If the first output element of the odd rows is ready, fill the first output FIFO
	// The first output gets ready when all the line buffers are full
    if (iter >= (T_WS - 1) * T_IN_W_T + T_WS - 1){
	  // Pack the data based on SIMD_LANE
      for (int tw = 0; tw < T_WS; tw++){
      #pragma HLS unroll
        for (int ii = 0; ii < T_UNROLL; ii++){
        
          T_data_t0 sum_tmp = sums_inp[ii][tw];
          ap_uint<T_DATA_WIDTH0> utmp_tmp = Reinterpret<ap_uint<T_DATA_WIDTH0> >(sum_tmp);
          utmp_inp[tw][ii] = utmp_tmp;
        }          
        ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data = (
#if POOL_LANE == 16
            utmp_inp[tw][15], utmp_inp[tw][14], utmp_inp[tw][13], utmp_inp[tw][12],
            utmp_inp[tw][11], utmp_inp[tw][10], utmp_inp[tw][9], utmp_inp[tw][8],
            utmp_inp[tw][7], utmp_inp[tw][6], utmp_inp[tw][5], utmp_inp[tw][4],
            utmp_inp[tw][3], utmp_inp[tw][2], utmp_inp[tw][1], utmp_inp[tw][0]           
#elif POOL_LANE == 8            
            utmp_inp[tw][7], utmp_inp[tw][6], utmp_inp[tw][5], utmp_inp[tw][4],
            utmp_inp[tw][3], utmp_inp[tw][2], utmp_inp[tw][1], utmp_inp[tw][0]
#elif POOL_LANE == 4
            utmp_inp[tw][3], utmp_inp[tw][2], utmp_inp[tw][1], utmp_inp[tw][0]
#elif POOL_LANE == 2
            utmp_inp[tw][1], utmp_inp[tw][0]
#elif POOL_LANE == 1
            utmp_inp[tw][0]
#endif            
            );
        fifo_out1.write(wide_data); 
#ifdef DEBUG_merge
			for (int lane = 0; lane < CONV_LANE; lane++){
				ap_uint<DATA_W0> u32_tmp = wide_data(DATA_W0 - 1, 0);
				cout << "up lane:" << lane<< "->" << Reinterpret<data_t0>(u32_tmp) << " ";
				wide_data = wide_data >> DATA_W0;
			}
			cout << endl;
#endif
      }
    }
    
	// If the first output element of the even rows is ready, fill the second output FIFO
	// The first output gets ready when all the line buffers are full
    if (iter >= (T_WS - 1) * T_IN_W_T + T_WS - 1){
	  // Pack the data based on SIMD_LANE
      for (int tw = 0; tw < T_WS; tw++){
      #pragma HLS unroll
        for (int ii = 0; ii < T_UNROLL; ii++){
        
          T_data_t0 sum_tmp = sums[ii][tw];
          ap_uint<T_DATA_WIDTH0> utmp_tmp = Reinterpret<ap_uint<T_DATA_WIDTH0> >(sum_tmp);
          utmp[tw][ii] = utmp_tmp;
        }          
        ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data = (
#if POOL_LANE == 16
            utmp[tw][15], utmp[tw][14], utmp[tw][13], utmp[tw][12],
            utmp[tw][11], utmp[tw][10], utmp[tw][9], utmp[tw][8],
            utmp[tw][7], utmp[tw][6], utmp[tw][5], utmp[tw][4],
            utmp[tw][3], utmp[tw][2], utmp[tw][1], utmp[tw][0]           
#elif POOL_LANE == 8            
            utmp[tw][7], utmp[tw][6], utmp[tw][5], utmp[tw][4],
            utmp[tw][3], utmp[tw][2], utmp[tw][1], utmp[tw][0]
#elif POOL_LANE == 4
            utmp[tw][3], utmp[tw][2], utmp[tw][1], utmp[tw][0]
#elif POOL_LANE == 2
            utmp[tw][1], utmp[tw][0]
#elif POOL_LANE == 1
            utmp[tw][0]
#endif            
            );
        fifo_out2.write(wide_data); 
      }
      trans_cnt++;
    }

    iter++;
    if (iter == iter_bound){
      iter = 0;
      end_row = 2 * T_IN_W_T;
      oo++;
      if (oo == oo_bound){
        oo = 0;
      }
    }
  }
}


/**
 * Function name: maxpool_w2
 * Function description: This function does the computation for pooling module with window size of w
 *                       The computation pattern is like a stencil computation
 */
template <class T_data_t0, int T_IN_H_T, int T_IN_W_T, int T_UNROLL, int T_WS, int T_DATA_WIDTH0>
void maxpool_w2(
  hls::stream<ap_uint<T_DATA_WIDTH0 * T_UNROLL> > &fifo_in,
  hls::stream<ap_uint<T_DATA_WIDTH0 * T_UNROLL> > &fifo_out,
  uint                                            stride,  
  bool                                            max_en,
  uint                                            layer_out_num_t,
  uint                                            layer_in_h_t
){
#pragma HLS INLINE off
  T_data_t0 line_buf1[T_UNROLL][T_IN_W_T];
  T_data_t0 line_buf2[T_UNROLL][T_WS];
#pragma HLS ARRAY_PARTITION variable=line_buf1 dim=1 complete
#pragma HLS ARRAY_PARTITION variable=line_buf1 dim=2 complete
#pragma HLS ARRAY_PARTITION variable=line_buf2 dim=1 complete
#pragma HLS ARRAY_PARTITION variable=line_buf2 dim=2 complete

  bool col_skip = 0;
  bool row_skip = 0;
  uint trans_cnt = 0;

#ifdef DEBUG
  uint max_pool_cout_cnt = 0;
#endif

  ap_uint<T_DATA_WIDTH0> utmp[T_UNROLL];
#pragma HLS ARRAY_PARTITION variable=utmp complete
  T_data_t0 sums[T_UNROLL];
#pragma HLS ARRAY_PARTITION variable=sums complete

  int oo =0;
  int iter = 0;
  int oo_bound = layer_out_num_t / T_UNROLL;
  int iter_bound = layer_in_h_t * T_IN_W_T + (T_WS - 1) * T_IN_W_T + T_WS - 1;
  int total_bound = oo_bound * iter_bound;

  for (int total_iter = 0; total_iter < total_bound; total_iter++){
#pragma HLS PIPELINE II=1    
    if (iter == 0){
      trans_cnt = 0;
    }

    ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data_in;
    if (iter < layer_in_h_t * T_IN_W_T){
      wide_data_in = fifo_in.read();
    }

    for (int dup = 0; dup < T_UNROLL; dup++){
	  // define the line buffers
      T_data_t0 tmp1 = line_buf1[dup][T_IN_W_T - 1];
      for (int i = T_IN_W_T - 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf1[dup][i] = line_buf1[dup][i - 1];
      }
      for (int i = T_WS - 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf2[dup][i] = line_buf2[dup][i - 1];
      }
      
      if (iter < layer_in_h_t * T_IN_W_T){  
		// Unpack the data based on the SIMD_LANE
        ap_uint<T_DATA_WIDTH0> sel_tmp;
#if POOL_LANE == 16
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
          case 4:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 5 - 1, T_DATA_WIDTH0 * 4);
            break;
          case 5:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 6 - 1, T_DATA_WIDTH0 * 5);
            break;
          case 6:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 7 - 1, T_DATA_WIDTH0 * 6);
            break;
          case 7:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 8 - 1, T_DATA_WIDTH0 * 7);
            break;
          case 8:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 9 - 1, T_DATA_WIDTH0 * 8);
            break;
          case 9:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 10 - 1, T_DATA_WIDTH0 * 9);
            break;
          case 10:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 11 - 1, T_DATA_WIDTH0 * 10);
            break;
          case 11:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 12 - 1, T_DATA_WIDTH0 * 11);
            break;
          case 12:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 13 - 1, T_DATA_WIDTH0 * 12);
            break;
          case 13:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 14 - 1, T_DATA_WIDTH0 * 13);
            break;
          case 14:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 15 - 1, T_DATA_WIDTH0 * 14);
            break;
          case 15:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 16 - 1, T_DATA_WIDTH0 * 15);
            break;           
        }
#elif POOL_LANE == 8        
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
          case 4:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 5 - 1, T_DATA_WIDTH0 * 4);
            break;
          case 5:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 6 - 1, T_DATA_WIDTH0 * 5);
            break;
          case 6:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 7 - 1, T_DATA_WIDTH0 * 6);
            break;
          case 7:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 8 - 1, T_DATA_WIDTH0 * 7);
            break;
        }
#elif POOL_LANE == 4
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
          case 2:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 3 - 1, T_DATA_WIDTH0 * 2);
            break;
          case 3:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 4 - 1, T_DATA_WIDTH0 * 3);
            break;
        }
#elif POOL_LANE == 2
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
          case 1:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 2 - 1, T_DATA_WIDTH0 * 1);
            break;
        }
#elif POOL_LANE == 1
        switch(dup){
          case 0:
            sel_tmp = wide_data_in(T_DATA_WIDTH0 * 1 - 1, T_DATA_WIDTH0 * 0);
            break;
        }
#endif     
        line_buf1[dup][0] = Reinterpret<T_data_t0>(sel_tmp);
      } else {      
        line_buf1[dup][0] = 0.0;
      }
      line_buf2[dup][0] = tmp1;

      // maxs 
      T_data_t0 mux_0_0 = max(line_buf2[dup][T_WS - 1], line_buf2[dup][T_WS - 2]);
      T_data_t0 mux_0_1 = max(line_buf1[dup][T_WS - 1], line_buf1[dup][T_WS - 2]);
      T_data_t0 mux_1_0 = max(mux_0_0, mux_0_1);

      if (max_en == 1)
        sums[dup] = mux_1_0;
      else
        sums[dup] = line_buf1[dup][T_WS - 2];
    }

	// If the first output element is ready, fill the output FIFO
	// The first output gets ready when all the line buffers are full
    if (iter >= (T_WS - 1) * T_IN_W_T + T_WS - 1){    
	  // Check whether the row/column should be skipped due to stride > 1
      col_skip = (trans_cnt % stride != 0);
      row_skip = ((trans_cnt / T_IN_W_T) % stride != 0);
      if (!col_skip && !row_skip){
        for (int ii = 0; ii < T_UNROLL; ii++){
          T_data_t0 sum_tmp = sums[ii];
          ap_uint<T_DATA_WIDTH0> utmp_tmp = Reinterpret<ap_uint<T_DATA_WIDTH0> >(sum_tmp);
          utmp[ii] = utmp_tmp;
        }  
		// Pack the data based on SIMD_LANE
        ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data = (
#if POOL_LANE == 16
            utmp[15], utmp[14], utmp[13], utmp[12],
            utmp[11], utmp[10], utmp[9], utmp[8],
            utmp[7], utmp[6], utmp[5], utmp[4],
            utmp[3], utmp[2], utmp[1], utmp[0]           
#elif POOL_LANE == 8            
            utmp[7], utmp[6], utmp[5], utmp[4],
            utmp[3], utmp[2], utmp[1], utmp[0]
#elif POOL_LANE == 4
            utmp[3], utmp[2], utmp[1], utmp[0]
#elif POOL_LANE == 2
            utmp[1], utmp[0]
#elif POOL_LANE == 1
            utmp[0]
#endif            
            );
        fifo_out.write(wide_data); 
      }
      trans_cnt++;
    }

    iter++;
    if (iter == iter_bound){
      iter = 0;
      oo++;
      if (oo == oo_bound){
        oo = 0;
      }
    }
  }
}

#endif
