#define MAX_IN_NUM_T 4
#define MAX_IN_H_T 6
#define MAX_IN_W_T 6
#define MAX_OUT_NUM_T 4
#define MAX_OUT_H_T 6
#define MAX_OUT_W_T 6

#define CIN_BUFF 256
#define CIN_PREV_BUFF 0
#define WEIGHT_BUFF 144
#define COUT_BUFF 144
#define U1_DATA0_BUF_SIZE 96
#define U1_DATA1_BUF_SIZE 36
#define U1_DATA2_BUF_SIZE 24
#define U1_LOCAL_REG_NUM 6
#define U1_TRANSFER_REG_NUM 6
#define SIMD_LANE 4
#define LAYER_NUM 1
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
#define FLOAT_DESIGN
#define CIN_SIZE 14193456
#define WEIGHT_SIZE 560032
#define BIAS_SIZE 16544
#define MAX_LAYER_BATCH 1
