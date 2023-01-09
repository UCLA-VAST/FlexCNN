//1_1_1_00
#include "util.h"
#define PRJ_PATH "/home/suhailb/Projects/StreamVSA/designs/tapa_test"
#define STRIDE 1
#define FILTER_S2_H 1
#define FILTER_S2_W 1
#define CIN_OFFSET 0
#define LAYER_ID_TEST 1
#define START_LAYER 1
#define END_LAYER 1
#define SL_OPTION 0
#define OUTFILE "/data/outputs/L1_outputs.dat"
#define OUT_OFFSET1 144
#define OUT_OFFSET2 144
#define CHANGE_LAYOUT 1
#define IN_NUM_HW 4
#define OUT_NUM_HW 4
#define IN_H_HW 6
#define IN_W_HW 6
#define OUT_H_HW 6
#define OUT_W_HW 6
#define IN_NUM 4
#define OUT_NUM 4
#define IN_H 6
#define IN_W 6
#define OUT_H 6
#define OUT_W 6
#define IN_NUM_T 4
#define OUT_NUM_T 4
#define IN_H_T 6
#define IN_W_T 6
#define OUT_H_NP 0
#define OUT_H_SP 0
#define OUT_W_EP 0
#define OUT_W_WP 0
template <typename T>
using aligned_vector = std::vector<T, tapa::aligned_allocator<T>>;
void instInit(vector<uint> &config);
void getInsts(aligned_vector<uint> &config, int start_layer, int end_layer);
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
);
void postprocess(
  aligned_vector<data_t0> &cin_hw,
  data_t0  outputs_hw[OUT_NUM][OUT_H][OUT_W],
  float  outputs_sw[OUT_NUM][OUT_H][OUT_W]
);
void compareResults(data_t0  outputs_hw[OUT_NUM][OUT_H][OUT_W], float  outputs_sw[OUT_NUM][OUT_H][OUT_W]);
void save_progress(aligned_vector<data_t0> &cin_hw, uint data_offset);
void load_progress(aligned_vector<data_t0> &cin_hw);