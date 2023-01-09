#ifndef __UTIL_H_
#define __UTIL_H_

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
#include "debug.h"
#include <tapa.h>

#define PROJ_DIR PRJ_PATH

using namespace std;
using namespace hls;

// Macros
/*
 * Set the module lanes to the same number currently
 */

#define DATA_LDWR_LANE (SIMD_LANE)
#define DEPTH_CONV_LANE (SIMD_LANE)
#define CONV_LANE (SIMD_LANE)
#define RELU_LANE (SIMD_LANE)
#define POOL_LANE (SIMD_LANE)
#define UPSAMPLE_LANE (SIMD_LANE) // reserved
#define INTER_LOAD_LANE (SIMD_LANE)
#define INTER_WRITE_LANE (SIMD_LANE)
//#define DATA_WRITE_LANE 8

#define CONFIG_PARAMS  53
#define INST_PER_LAYER 6


using bus_t = tapa::vec_t<float, 16>;
template <typename T>
using bits = ap_uint<tapa::widthof<T>()>;

typedef bits<bus_t> bus_t0;
typedef bits<bus_t> bus_t1;
typedef bits<bus_t> bus_t2;
typedef ap_uint<32> bus_t3;

typedef tapa::mmap<bits<bus_t> > bus_mem_0;
typedef tapa::mmap<bits<bus_t> > bus_mem_1;
typedef tapa::mmap<bits<bus_t> > bus_mem_2;
typedef tapa::mmap<ap_uint<32> > bus_mem_3;

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



void top_kernel(
  bus_mem_0 dram_b0,
  bus_mem_1 dram_b1,
  bus_mem_3 layer_config,
  uint start_inst,
  uint end_inst
);




void instInit(uint config[LAYER_NUM*CONFIG_PARAMS]);


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
template <int bitWidth>
void print(ap_uint<bitWidth> line){
  data_t2 num;
  for(int i=0; i<bitWidth/32; i++){
    num = Reinterpret<data_t2>((ap_uint<32>)line((i+1)*32-1, 32*i));
    printf("%10f\t", num);
  }
  printf("\n");
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
  uint                                            layer_in_h_t,
  uint                                            layer_in_w_t
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
  int iter_bound = layer_in_h_t * layer_in_w_t + (T_WS - 1) * layer_in_w_t + T_WS - 1;
  int total_bound = oo_bound * iter_bound;

  for (int total_iter = 0; total_iter < total_bound; total_iter++){
#pragma HLS PIPELINE II=1    
    if (iter == 0){
      trans_cnt = 0;
    }

    ap_uint<T_DATA_WIDTH0 * T_UNROLL> wide_data_in;
    if (iter < layer_in_h_t * layer_in_w_t){
      wide_data_in = fifo_in.read();
    }

    for (int dup = 0; dup < T_UNROLL; dup++){
          // define the line buffers
      T_data_t0 tmp1 = line_buf1[dup][layer_in_w_t - 1];
      // cout<<layer_in_w_t<<endl;
      for (int i = T_IN_W_T - 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf1[dup][i] = line_buf1[dup][i - 1];
      }
      for (int i = T_WS - 1; i >= 1; i--){
#pragma HLS UNROLL
        line_buf2[dup][i] = line_buf2[dup][i - 1];
      }
      
      if (iter < layer_in_h_t * layer_in_w_t){  
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
        line_buf1[dup][0] = -100.0;
      }
      line_buf2[dup][0] = tmp1;

      // maxs 
      T_data_t0 mux_0_0 = max(line_buf2[dup][T_WS - 1], line_buf2[dup][T_WS - 2]);
      T_data_t0 mux_0_1 = max(line_buf1[dup][T_WS - 1], line_buf1[dup][T_WS - 2]);
      T_data_t0 mux_1_0 = max(mux_0_0, mux_0_1);

      //avg
      // T_data_t0 mux_1_0 = (line_buf2[dup][T_WS - 1] + line_buf2[dup][T_WS - 2] + line_buf1[dup][T_WS - 1] + line_buf1[dup][T_WS - 2])/4;

      if (max_en == 1)
        sums[dup] = mux_1_0;
      else
        sums[dup] = line_buf1[dup][T_WS - 2];
    }

  // If the first output element is ready, fill the output FIFO
  // The first output gets ready when all the line buffers are full
    if (iter >= (T_WS - 1) * layer_in_w_t + T_WS - 1){    
          // Check whether the row/column should be skipped due to stride > 1
      col_skip = (trans_cnt % stride != 0);
      row_skip = ((trans_cnt / layer_in_w_t) % stride != 0);
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
