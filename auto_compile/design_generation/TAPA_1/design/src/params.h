#include "ap_fixed.h"
#include <tapa.h>
#define MAX_IN_NUM_T 64
#define MAX_IN_H_T 18
#define MAX_IN_W_T 288
#define MAX_OUT_NUM_T 64
#define MAX_OUT_H_T 36
#define MAX_OUT_W_T 576

#define CIN_BUFF 108800
#define CIN_PREV_BUFF 36864
#define WEIGHT_BUFF 9216
#define COUT_BUFF 36864
#define U1_DATA0_BUF_SIZE 57600
#define U1_DATA1_BUF_SIZE 1152
#define U1_DATA2_BUF_SIZE 4096
#define U1_LOCAL_REG_NUM 512
#define U1_TRANSFER_REG_NUM 288
#define SIMD_LANE 8
#define LAYER_NUM 89
// Data Types
// primtive data types
typedef float data_t0; // cin, cout
typedef float data_t1; // weight
typedef float data_t2; // bias
typedef unsigned int data_t3; // inst
using bus_t = tapa::vec_t<data_t0, 16>;
#define DATA_W0 32
#define DATA_W1 32
#define DATA_W2 32
#define DATA_W3 32
#define FLOAT_DESIGN
#define CIN_SIZE 25000000
#define WEIGHT_SIZE 20000000
#define BIAS_SIZE 50000
#define MAX_LAYER_BATCH 1
//on-chip memory assignments
#define U1_DataFeed0Head_MEM 1
#define U1_DataFeed1Head_MEM 0
#define U1_DataCollect2Head_MEM 0
#define U1_DataFeed0Engine0_MEM 1
#define U1_DataFeed0EngineLast_MEM 0
#define U1_DataFeed1Engine0_MEM 0
#define U1_DataFeed1EngineLast_MEM 0
#define U1_DataCollect2Engine0_MEM 0
#define U1_DataCollect2EngineLast_MEM 0
#define cin_load_MEM 1
#define cin_load_prev_MEM 1
#define weight_load_MEM 1
#define bias_load_MEM 1
#define cout_write_MEM 1
