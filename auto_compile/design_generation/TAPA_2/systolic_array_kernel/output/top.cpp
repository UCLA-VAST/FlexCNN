/**
 *  This file is automatically generated by PolySA CodeGen.
 *  Version: 1.0
 *  Authos: Jie Wang
 */

#include "common_header_U1.h"

void top_kernel(
  U1_bus_t0* global_cin,
  U1_bus_t1* global_weight,
  U1_bus_t2* global_cout,
  bool init,
  unsigned int FILTER_S
){
#pragma HLS INTERFACE m_axi port=global_cin offset=slave bundle=gmem0 depth=16
#pragma HLS INTERFACE m_axi port=global_weight offset=slave bundle=gmem1 depth=9
#pragma HLS INTERFACE m_axi port=global_cout offset=slave bundle=gmem2 depth=16
#pragma HLS INTERFACE s_axilite port=global_cin bundle=control
#pragma HLS INTERFACE s_axilite port=global_weight bundle=control
#pragma HLS INTERFACE s_axilite port=global_cout bundle=control
#pragma HLS INTERFACE s_axilite port=init bundle=control
#pragma HLS INTERFACE s_axilite port=FILTER_S bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  U1_kernel(global_cin, global_weight, global_cout, init, FILTER_S);
}
