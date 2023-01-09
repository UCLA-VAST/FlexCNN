#include "util.h"
#define PRJ_PATH "C:/Users/Suhail/Desktop/Projects/FlexCNN_VSA/designs/test_16bit"
#define STRIDE 2
#define FILTER_S2_H 3
#define FILTER_S2_W 3
#define CIN_OFFSET 0
#define LAYER_ID_TEST 1
#define START_LAYER 1
#define END_LAYER 1
#define SL_OPTION 0
#define OUTFILE "/data/outputs/L1_outputs.dat"
#define OUT_OFFSET1 1336352
#define OUT_OFFSET2 1336352
#define CHANGE_LAYOUT 1
#define IN_NUM_HW 16
#define OUT_NUM_HW 16
#define IN_H_HW 290
#define IN_W_HW 290
#define OUT_H_HW 144
#define OUT_W_HW 144
#define IN_NUM 3
#define OUT_NUM 13
#define IN_H 288
#define IN_W 288
#define OUT_H 144
#define OUT_W 144
#define IN_NUM_T 16
#define OUT_NUM_T 16
#define IN_H_T 8
#define IN_W_T 288
#define OUT_H_NP 0
#define OUT_H_SP 0
#define OUT_W_EP 0
#define OUT_W_WP 0
void instInit(uint* config);
void getInsts(uint* config, int start_layer, int end_layer);
void preprocess(
  data_t0* cin_hw,
  data_t1* weight_hw,
  data_t2* bias_hw,
  uint in_num_hw,
  uint out_num_hw,
  uint in_h_hw,
  uint in_w_hw,
  uint kernel_h,
  uint kernel_w
);
void postprocess(
  data_t0* cin_hw,
  data_t0  outputs_hw[OUT_NUM][OUT_H][OUT_W],
  float  outputs_sw[OUT_NUM][OUT_H][OUT_W]
);
void compareResults(data_t0  outputs_hw[OUT_NUM][OUT_H][OUT_W], float  outputs_sw[OUT_NUM][OUT_H][OUT_W]);
void save_progress(data_t0* cin_hw, uint data_offset);
void load_progress(data_t0* cin_hw);