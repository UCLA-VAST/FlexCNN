import json
import argparse
import numpy as np
import os
import argparse

def cal_width(range):
  if range > 0:
    width = np.ceil(np.log2(range)) + 1
  else:
    width = 1
  return int(width)

def cnn_pass(vsa, config):
  # LOCAL_REG_NUM
  vsa['LOCAL_REG_NUM'] = int(vsa['PARAMETERS']['OUT_IMG_H_T'] * vsa['ROW_IL_FACTOR'] * vsa['COL_IL_FACTOR'])
  # LOCAL_ACCUM_NUM
  vsa['LOCAL_ACCUM_NUM'] = int(vsa['PARAMETERS']['IN_NUM_T'] * vsa['PARAMETERS']['K'] * vsa['PARAMETERS']['K'] / vsa['SIMD_FACTOR'])
  # GLOBAL_ACCUM_NUM
  vsa['GLOBAL_ACCUM_NUM'] = [1, 1]

  # MAC_statement
  vsa['MAC_STAT'] = 'sum += op0_u[i] * op1_u[i];\n'

  # generate the buf size
  vsa['DFC_BUF_SIZE'] = []
  vsa['DFC_HEAD_BUF_SIZE'] = []
  vsa['DFC_BUF_SIZE'] = [
      int(vsa['PARAMETERS']['IN_NUM_T'] * (vsa['PARAMETERS']['OUT_IMG_H_T'] + vsa['PARAMETERS']['K'] - 1) * (vsa['COL_IL_FACTOR'] + vsa['PARAMETERS']['K'] - 1)), \
      int(vsa['ROW_IL_FACTOR'] * vsa['PARAMETERS']['IN_NUM_T'] * vsa['PARAMETERS']['K'] * vsa['PARAMETERS']['K']), \
      int(vsa['ROW_IL_FACTOR'] * vsa['SA_ROWS'] * vsa['PARAMETERS']['OUT_IMG_H_T'] * vsa['COL_IL_FACTOR'])
      ]

  vsa['DFC_HEAD_BUF_SIZE'] = [
      int(vsa['PARAMETERS']['IN_IMG_H_T'] * vsa['PARAMETERS']['IN_IMG_W_T'] * vsa['PARAMETERS']['IN_NUM']), \
      int(vsa['PARAMETERS']['OUT_NUM_T'] * vsa['PARAMETERS']['K'] * vsa['PARAMETERS']['K'] * vsa['PARAMETERS']['IN_NUM']), \
      int(vsa['PARAMETERS']['OUT_IMG_H_T'] * vsa['PARAMETERS']['OUT_IMG_W_T'] * vsa['PARAMETERS']['OUT_NUM'])
      ]

  vsa['ARRAY_SIZE'] = [
      int(vsa['PARAMETERS']['IN_NUM'] * vsa['PARAMETERS']['IN_IMG_H'] * vsa['PARAMETERS']['IN_IMG_W']), \
      int(vsa['PARAMETERS']['OUT_NUM'] * vsa['PARAMETERS']['IN_NUM'] * vsa['PARAMETERS']['K'] * vsa['PARAMETERS']['K']), \
      int(vsa['PARAMETERS']['OUT_NUM'] * vsa['PARAMETERS']['OUT_IMG_H'] * vsa['PARAMETERS']['OUT_IMG_W'])]

  # generate the code snippet
  # - DF_FEED_COUNTER
  vsa['DF_FEED_COUNTER'] = []

  width = cal_width(vsa['PARAMETERS']['OUT_IMG_H_T'])
  bound_upper = int(vsa['PARAMETERS']['OUT_IMG_H_T'])
  counter = {}
  counter['VARIABLE'] = 'c0_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  width = cal_width(vsa['ROW_IL_FACTOR'])
  bound_upper = int(vsa['ROW_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c1_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  width = cal_width(vsa['COL_IL_FACTOR'])
  bound_upper = int(vsa['COL_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c2_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  width = cal_width(vsa['PARAMETERS']['K'])
  bound_upper = int(vsa['PARAMETERS']['K'])
  counter = {}
  counter['VARIABLE'] = 'c3_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  width = cal_width(vsa['PARAMETERS']['K'])
  bound_upper = int(vsa['PARAMETERS']['K'])
  counter = {}
  counter['VARIABLE'] = 'c4_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  width = cal_width(vsa['PARAMETERS']['IN_NUM_T'] / vsa['SIMD_FACTOR'])
  bound_upper = int(vsa['PARAMETERS']['IN_NUM_T'] / vsa['SIMD_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c5_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  # - DC_COLLECT_COUTNER
  vsa['DC_COLLECT_COUNTER'] = []

  width = cal_width(vsa['PARAMETERS']['OUT_IMG_H_T'])
  bound_upper = int(vsa['PARAMETERS']['OUT_IMG_H_T'])
  counter = {}
  counter['VARIABLE'] = 'c0_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  width = cal_width(vsa['ROW_IL_FACTOR'])
  bound_upper = int(vsa['ROW_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c1_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  width = cal_width(vsa['COL_IL_FACTOR'])
  bound_upper = int(vsa['COL_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c2_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  width = cal_width(vsa['PARAMETERS']['OUT_NUM_T'] / vsa['ROW_IL_FACTOR'])
  bound_upper = int(vsa['PARAMETERS']['OUT_NUM_T'] / vsa['ROW_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c3_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  var_prefix = 'U' + str(vsa['KERNEL_ID']) + '_'
  # - SW_KERNEL_CODE
  code = []
  code.append('for (int out_num = 0; out_num < %sOUT_NUM; out_num++){\n' % (var_prefix))
  code.append('  for (int out_img_h = 0; out_img_h < %sOUT_IMG_H; out_img_h++){\n' % (var_prefix))
  code.append('    for (int out_img_w = 0; out_img_w < %sOUT_IMG_W; out_img_w++){\n' % (var_prefix))
  code.append('      if (init == 1){\n')
  code.append('        global_cout[out_img_h*%sOUT_IMG_W*%sOUT_NUM + out_img_w*%sOUT_NUM + out_num] = 0;\n' % (var_prefix, var_prefix, var_prefix))
  code.append('      }\n')
  code.append('      for (int in_num = 0; in_num < %sIN_NUM; in_num++){\n' % (var_prefix))
  code.append('        for (int p = 0; p < %sK; p++){\n' % (var_prefix))
  code.append('          for (int q = 0; q < %sK; q++){\n' % (var_prefix))
  code.append('            global_cout[out_img_h*%sOUT_IMG_W*%sOUT_NUM + out_img_w*%sOUT_NUM + out_num] += global_cin[(out_img_h + p)*%sIN_IMG_W*%sIN_NUM + (out_img_w+q)*%sIN_NUM + in_num] * global_weight[out_num*%sK*%sK*%sIN_NUM + p*%sK*%sIN_NUM + q*%sIN_NUM + in_num];' % (var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix))
  code.append('          }\n')
  code.append('        }\n')
  code.append('      }\n')
  code.append('    }\n')
  code.append('  }\n')
  code.append('}\n')

  vsa['SW_KERNEL_CODE'] = code

  # - LAST_TILE_CODE
  code = []
  code.append('if (in_num_t == ' + var_prefix + 'IN_NUM - ' + var_prefix + 'IN_NUM_T){\n')

  vsa['LAST_TILE_CODE'] = code

  # - LAST_PATCH_CODE
  code = []
  code.append('(in_num_t == ' + var_prefix + 'IN_NUM - ' + var_prefix + 'IN_NUM_T) && (out_num_t == ' + var_prefix + 'OUT_NUM - ' + var_prefix + 'OUT_NUM_T) && (out_img_h_t == ' + var_prefix + 'OUT_IMG_H - ' + var_prefix + 'OUT_IMG_H_T) && (out_img_w_t == ' + var_prefix + 'OUT_IMG_W - ' + var_prefix + 'OUT_IMG_W_T)')

  vsa['LAST_PATCH_CODE'] = code

  # - DF_FEED_ADDR_CAL_CODE
  code = []
  code.append('c5_counter * %sSIMD_FACTOR + (c2_counter + c4_counter) * %sIN_NUM_T + (c0_counter + c3_counter) * (%sCOL_IL_FACTOR + FILTER_S - 1) * %sIN_NUM_T' % (var_prefix, var_prefix, var_prefix, var_prefix))
  code.append('c1_counter * FILTER_S * FILTER_S * %sIN_NUM_T + c3_counter * FILTER_S * %sIN_NUM_T + c4_counter * %sIN_NUM_T + c5_counter * %sSIMD_FACTOR' % (var_prefix, var_prefix, var_prefix, var_prefix))

  vsa['DF_FEED_ADDR_CAL_CODE'] = code

  # - DC_COLLECT_ADDR_CAL_CODE
  code = []
  code.append('c0_counter * %sCOL_IL_FACTOR * %sSA_ROWS * %sROW_IL_FACTOR + c2_counter * %sSA_ROWS * %sROW_IL_FACTOR + ((%s - 1 - c3_counter) * %sROW_IL_FACTOR + c1_counter)' % (var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, str(vsa['SA_ROWS']), var_prefix))

  vsa['DC_COLLECT_ADDR_CAL_CODE'] = code

  # - HEAD_CODE
  vsa['HEAD_CODE'] = []

  out_img_h_t_width = cal_width(vsa['PARAMETERS']['OUT_IMG_H'])
  out_img_w_t_width = cal_width(vsa['PARAMETERS']['OUT_IMG_W'])
  out_num_t_width = cal_width(vsa['PARAMETERS']['OUT_NUM'])
  in_img_h_t_width = cal_width(vsa['PARAMETERS']['IN_IMG_H_T'])
  in_num_t_width = cal_width(vsa['PARAMETERS']['IN_NUM'] / vsa['SIMD_FACTOR'])

  t0_width = cal_width(vsa['PARAMETERS']['OUT_IMG_W_T'] / vsa['COL_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][0])
  t1_width = cal_width(vsa['PARAMETERS']['OUT_IMG_H_T'] + vsa['PARAMETERS']['K'])
  t2_width = cal_width(vsa['COL_IL_FACTOR'] + vsa['PARAMETERS']['K'])
  t3_width = cal_width(vsa['PARAMETERS']['IN_NUM_T'] / vsa['FC_SIMD_FACTOR'][0])

  A_head = {}
  code = []
  code.append('for (ap_uint<' + str(out_img_h_t_width) + '> out_img_h_t = 0; out_img_h_t < ' + var_prefix + 'OUT_IMG_H; out_img_h_t += ' + var_prefix + 'OUT_IMG_H_T){\n')
  code.append('  for (ap_uint<%s> out_img_w_t = 0; out_img_w_t < %sOUT_IMG_W; out_img_w_t += %sOUT_IMG_W_T){\n' %(str(out_img_w_t_width), var_prefix, var_prefix))
  code.append('    unsigned int chunk_offset = out_img_h_t * ' + var_prefix + 'IN_IMG_W * %sIN_NUM;\n' % (var_prefix))
  code.append('    for (int in_img_h_t = 0; in_img_h_t < %sIN_IMG_H_T; in_img_h_t++){\n' % (var_prefix))
  code.append('      unsigned int local_chunk_offset = chunk_offset + in_img_h_t * %sIN_IMG_W * %sIN_NUM + out_img_w_t * %sIN_NUM;\n' % (var_prefix, var_prefix, var_prefix))
  code.append('      memcpy((void*)(cin_buf + in_img_h_t * %sIN_IMG_W_T * %sIN_NUM / %sDATA0_PACK_FACTOR), (void*)(cin + local_chunk_offset / %sDATA0_PACK_FACTOR), sizeof(%sdata_t0) * %sIN_IMG_W_T * %sIN_NUM);\n' % (var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix))
  code.append('    }\n')
  code.append('    for (ap_uint<%s> out_num_t = 0; out_num_t < %sOUT_NUM; out_num_t += %sOUT_NUM_T){\n' %(str(out_num_t_width), var_prefix, var_prefix))
  code.append('      for (ap_uint<' + str(in_num_t_width) + '> in_num_t = 0; in_num_t < ' + var_prefix + 'IN_NUM / ' + var_prefix + 'SIMD_FACTOR; in_num_t += ' + var_prefix + 'IN_NUM_T / ' + var_prefix + 'SIMD_FACTOR){\n')
  code.append('        bool init_internal = (in_num_t == 0);\n')
  code.append('        bool init_final = init && init_internal;\n')
  code.append('        bool last = (in_num_t == (' + var_prefix + 'IN_NUM - ' + var_prefix + 'IN_NUM_T) / ' + var_prefix + 'SIMD_FACTOR);\n')

  A_head['DRAM_CODE'] = code
  A_head['DRAM_CODE_INDENT_INC'] = 4

  code = []
  code.append('for (ap_uint<' + str(t0_width) + '> t0 = 0; t0 < ' + var_prefix + 'OUT_IMG_W_T / ' + var_prefix + 'COL_IL_FACTOR / ' + var_prefix + 'DATA0_FC_SPLIT_FACTOR; t0++){\n')
  code.append('  for (ap_uint<%s> t1 = 0; t1 < %sOUT_IMG_H_T + FILTER_S - 1; t1++){\n' % (str(t1_width), var_prefix))
  code.append('    for (ap_uint<' + str(t2_width) + '> t2 = 0; t2 < ' + var_prefix + 'COL_IL_FACTOR + FILTER_S - 1; t2++){\n')
  code.append('      for (ap_uint<' + str(t3_width) + '> t3 = 0; t3 < ' + var_prefix + 'IN_NUM_T / ' + var_prefix + 'DATA0_FC_SIMD_FACTOR; t3++){\n')
  code.append('      #pragma HLS PIPELINE II=1\n')
  code.append('        unsigned int local_in_img_w = t0 * ' + var_prefix + 'COL_IL_FACTOR + t2;\n')
  code.append('        unsigned int local_in_num = in_num_t * ' + var_prefix + 'SIMD_FACTOR + t3 * ' + var_prefix + 'DATA0_FC_SIMD_FACTOR;\n')
  code.append('        unsigned int local_in_img_h = t1;\n')
  code.append('        unsigned int feeder_id = t0 / ' + var_prefix + 'DATA0_FC_GROUP_FACTOR;\n')
  code.append('        unsigned int cin_index = local_in_img_h * %sIN_IMG_W_T * %sIN_NUM + local_in_img_w * %sIN_NUM + local_in_num;\n' % (var_prefix, var_prefix, var_prefix))

  A_head['BRAM_CODE'] = code
  A_head['BRAM_CODE_INDENT_INC'] = 4
  A_head['BRAM_ACCESS_CODE'] = [
      '[cin_bus_index]'
      ]

  vsa['HEAD_CODE'].append(A_head)

  B_head = {}

  out_img_h_t_width = cal_width(vsa['PARAMETERS']['OUT_IMG_H'])
  out_img_w_t_width = cal_width(vsa['PARAMETERS']['OUT_IMG_W'])
  out_num_t_width = cal_width(vsa['PARAMETERS']['OUT_NUM'])
  in_num_t_width = cal_width(vsa['PARAMETERS']['IN_NUM'] / vsa['SIMD_FACTOR'])

  t0_width = cal_width(vsa['PARAMETERS']['OUT_NUM_T'] / vsa['ROW_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][1])
  t1_width = cal_width(vsa['ROW_IL_FACTOR'])
  t2_width = cal_width(vsa['PARAMETERS']['K'])
  t3_width = cal_width(vsa['PARAMETERS']['K'])
  t4_width = cal_width(vsa['PARAMETERS']['IN_NUM_T'] / vsa['FC_SIMD_FACTOR'][1])

  code = []
  code.append('for (ap_uint<' + str(out_img_h_t_width) + '> out_img_h_t = 0; out_img_h_t < ' + var_prefix + 'OUT_IMG_H; out_img_h_t += ' + var_prefix + 'OUT_IMG_H_T){\n')
  code.append('  for (ap_uint<%s> out_img_w_t = 0; out_img_w_t < %sOUT_IMG_W; out_img_w_t += %sOUT_IMG_W_T){\n' %(str(out_img_w_t_width), var_prefix, var_prefix))
  code.append('    for (ap_uint<%s> out_num_t = 0; out_num_t < %sOUT_NUM; out_num_t += %sOUT_NUM_T){\n' % (str(out_num_t_width), var_prefix, var_prefix))
  code.append('      unsigned int chunk_offset = out_num_t * FILTER_S * FILTER_S * %sIN_NUM;\n' % (var_prefix))
  code.append('      memcpy((void*)weight_buf, (void*)(weight + chunk_offset / ' + var_prefix + 'DATA1_PACK_FACTOR), sizeof(' + var_prefix + 'data_t1) * ' + var_prefix + 'OUT_NUM_T * FILTER_S * FILTER_S * %sIN_NUM);\n' % (var_prefix))
  code.append('      for (ap_uint<' + str(in_num_t_width) + '> in_num_t = 0; in_num_t < ' + var_prefix + 'IN_NUM / ' + var_prefix + 'SIMD_FACTOR; in_num_t += ' + var_prefix + 'IN_NUM_T / ' + var_prefix + 'SIMD_FACTOR){\n')
  code.append('        bool init_internal = (in_num_t == 0);\n')
  code.append('        bool init_final = init && init_internal;\n')
  code.append('        bool last = (in_num_t == (' + var_prefix + 'IN_NUM - ' + var_prefix + 'IN_NUM_T) / ' + var_prefix + 'SIMD_FACTOR);\n')
  B_head['DRAM_CODE'] = code
  B_head['DRAM_CODE_INDENT_INC'] = 4

  code = []
  code.append('for (ap_uint<%s> t0 = 0; t0 < %sOUT_NUM_T / %sROW_IL_FACTOR / %sDATA1_FC_SPLIT_FACTOR; t0++){\n' %(str(t0_width), var_prefix, var_prefix, var_prefix))
  code.append('  for (ap_uint<%s> t1 = 0; t1 < %sROW_IL_FACTOR; t1++){\n' %(str(t1_width), var_prefix))
  code.append('    for (ap_uint<%s> t2 = 0; t2 < FILTER_S; t2++){\n' % (str(t2_width)))
  code.append('      for (ap_uint<%s> t3 = 0; t3 < FILTER_S; t3++){\n' % (str(t3_width)))
  code.append('        for (ap_uint<' + str(t4_width) + '> t4 = 0; t4 < ' + var_prefix + 'IN_NUM_T / ' + var_prefix + 'DATA1_FC_SIMD_FACTOR; t4++){\n')
  code.append('        #pragma HLS PIPELINE II=1\n')
  code.append('          unsigned int local_out_num = t0 * %sROW_IL_FACTOR + t1;\n' % (var_prefix))
  code.append('          unsigned int local_in_num = in_num_t * ' + var_prefix + 'SIMD_FACTOR + t4 * ' + var_prefix + 'DATA1_FC_SIMD_FACTOR;\n')
  code.append('          unsigned int feeder_id = t0 / ' + var_prefix + 'DATA1_FC_GROUP_FACTOR;\n')
  code.append('          unsigned int weight_index = local_out_num * FILTER_S * FILTER_S * %sIN_NUM + t2 * FILTER_S * %sIN_NUM + t3 * %sIN_NUM + local_in_num;\n' %(var_prefix, var_prefix, var_prefix))
  B_head['BRAM_CODE'] = code
  B_head['BRAM_CODE_INDENT_INC'] = 5
  B_head['BRAM_ACCESS_CODE'] = [
      "[weight_bus_index]"
      ]

  vsa['HEAD_CODE'].append(B_head)

  C_head = {}

  out_img_h_t_width = cal_width(vsa['PARAMETERS']['OUT_IMG_H'])
  out_img_w_t_width = cal_width(vsa['PARAMETERS']['OUT_IMG_W'])
  out_num_t_width = cal_width(vsa['PARAMETERS']['OUT_NUM'])
  in_img_h_t_width = cal_width(vsa['PARAMETERS']['IN_IMG_H_T'])
  in_num_t_width = cal_width(vsa['PARAMETERS']['IN_NUM'] / vsa['SIMD_FACTOR'])

  t0_width = cal_width(vsa['PARAMETERS']['OUT_IMG_W_T'] / vsa['COL_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][2]) + 1
  t1_width = cal_width(vsa['PARAMETERS']['OUT_IMG_H_T'])
  t2_width = cal_width(vsa['COL_IL_FACTOR'])
  t3_width = cal_width(vsa['PARAMETERS']['OUT_NUM_T'] / vsa['FC_SIMD_FACTOR'][2])

  code = []
  code.append('unsigned int chunk_offset = out_img_h_t * %sOUT_IMG_W * %sOUT_NUM;\n' % (var_prefix, var_prefix))
  code.append('for (int h = 0; h < %sOUT_IMG_H_T; h++){\n' % (var_prefix))
  code.append('  unsigned int local_chunk_offset = chunk_offset + h * %sOUT_IMG_W * %sOUT_NUM + out_img_w_t * %sOUT_NUM;\n' % (var_prefix, var_prefix, var_prefix))
  code.append('  memcpy((void*)(cout + local_chunk_offset / ' + var_prefix + 'DATA2_PACK_FACTOR), (void*)(cout_buf + h * %sOUT_IMG_W_T * %sOUT_NUM / %sDATA2_PACK_FACTOR), sizeof(' % (var_prefix, var_prefix, var_prefix) + var_prefix + 'data_t2) * ' + var_prefix + 'OUT_IMG_W_T * %sOUT_NUM);\n' % (var_prefix))
  code.append('}\n')
  C_head['DRAM_CODE'] = code
  C_head['DRAM_CODE_INDENT_INC'] = 0
  C_head['DRAM_ACCESS_CODE'] = [
      '[t0 * ' + var_prefix + 'DATA2_PACK_FACTOR + dup]',
      '[t0]'
      ]
  code = []
  code.append('for (ap_uint<' + str(out_img_h_t_width) + '> out_img_h_t = 0; out_img_h_t < ' + var_prefix + 'OUT_IMG_H; out_img_h_t += ' + var_prefix + 'OUT_IMG_H_T){\n')
  code.append('  for (ap_uint<%s> out_img_w_t = 0; out_img_w_t < %sOUT_IMG_W; out_img_w_t += %sOUT_IMG_W_T){\n' % (str(out_img_w_t_width), var_prefix, var_prefix))
  code.append('    for (ap_uint<%s> out_num_t = 0; out_num_t < %sOUT_NUM; out_num_t += %sOUT_NUM_T){\n' % (str(out_num_t_width), var_prefix, var_prefix))
  code.append('      for (ap_int<' + str(t0_width) + '> t0 = ' + var_prefix + 'OUT_IMG_W_T / ' + var_prefix + 'COL_IL_FACTOR / ' + var_prefix + 'DATA2_FC_SPLIT_FACTOR - 1; t0 >= 0; t0--){\n')
  code.append('        for (ap_uint<%s> t1 = 0; t1 < %sOUT_IMG_H_T; t1++){\n' % (str(t1_width), var_prefix))
  code.append('          for (ap_uint<%s> t2 = 0; t2 < %sCOL_IL_FACTOR; t2++){\n' % (str(t2_width), var_prefix))
  code.append('            for (ap_uint<%s> t3 = 0; t3 < %sOUT_NUM_T / %sDATA2_FC_SIMD_FACTOR; t3++){\n' % (str(t3_width), var_prefix, var_prefix))
  code.append('              #pragma HLS PIPELINE II=1\n')
  code.append('              unsigned int local_out_img_w = t0 * ' + var_prefix + 'COL_IL_FACTOR + t2;\n')
  code.append('              unsigned int local_out_num = out_num_t + t3 * %sDATA2_FC_SIMD_FACTOR;\n' % (var_prefix))
  code.append('              unsigned int local_out_img_h = t1;\n')
  code.append('              unsigned int cout_index = local_out_img_h * %sOUT_IMG_W_T * %sOUT_NUM + local_out_img_w * %sOUT_NUM + local_out_num;\n' % (var_prefix, var_prefix, var_prefix))

  C_head['BRAM_CODE'] = code
  C_head['BRAM_CODE_INDENT_INC'] = [7, 5]
  C_head['BRAM_ACCESS_CODE'] = [
      'local_out_img_h * %sOUT_IMG_W_T * %sOUT_NUM + local_out_img_w * %sOUT_NUM + (local_out_num + offset)' % (var_prefix, var_prefix, var_prefix)
      ]

  vsa['HEAD_CODE'].append(C_head)

def mm_pass(vsa, config):
  # LOCAL_REG_NUM
  vsa['LOCAL_REG_NUM'] = int(vsa['ROW_IL_FACTOR'] * vsa['COL_IL_FACTOR'])
  # LOCAL_ACCUM_NUM
  vsa['LOCAL_ACCUM_NUM'] = int(vsa['PARAMETERS']['K_T'] / vsa['SIMD_FACTOR'])
  # GLOBAL_ACCUM_NUM
  vsa['GLOBAL_ACCUM_NUM'] = [1, 1]

  # MAC_statement
  vsa['MAC_STAT'] = 'sum += op0_u[i] * op1_u[i];\n'

  # generate the buf size
  vsa['DFC_BUF_SIZE'] = []
  vsa['DFC_HEAD_BUF_SIZE'] = []
  vsa['DFC_BUF_SIZE'] = [
      int(vsa['COL_IL_FACTOR'] * vsa['PARAMETERS']['K_T']), \
      int(vsa['ROW_IL_FACTOR'] * vsa['PARAMETERS']['K_T']), \
      int(vsa['COL_IL_FACTOR'] * vsa['PARAMETERS']['J_T'])]

  vsa['DFC_HEAD_BUF_SIZE'] = [
      int(vsa['PARAMETERS']['I_T'] * vsa['PARAMETERS']['K']), \
      int(vsa['PARAMETERS']['J_T'] * vsa['PARAMETERS']['K']), \
      int(vsa['PARAMETERS']['I_T'] * vsa['PARAMETERS']['J_T'])]

  vsa['ARRAY_SIZE'] = [
      int(vsa['PARAMETERS']['I'] * vsa['PARAMETERS']['K']), \
      int(vsa['PARAMETERS']['K'] * vsa['PARAMETERS']['J']), \
      int(vsa['PARAMETERS']['I'] * vsa['PARAMETERS']['J'])]

  # generate the code snippet
  # - DF_FEED_COUNTER
  vsa['DF_FEED_COUNTER'] = []

  width = cal_width(vsa['ROW_IL_FACTOR'])
  bound_upper = int(vsa['ROW_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c0_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  width = cal_width(vsa['COL_IL_FACTOR'])
  bound_upper = int(vsa['COL_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c1_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  width = cal_width(vsa['PARAMETERS']['K_T'] / vsa['SIMD_FACTOR'])
  bound_upper = int(vsa['PARAMETERS']['K_T'] / vsa['SIMD_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c2_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  # - DC_COLLECT_COUTNER
  vsa['DC_COLLECT_COUNTER'] = []

  width = cal_width(vsa['ROW_IL_FACTOR'])
  bound_upper = int(vsa['ROW_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c0_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  width = cal_width(vsa['COL_IL_FACTOR'])
  bound_upper = int(vsa['COL_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c1_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  width = cal_width(vsa['PARAMETERS']['J_T'] / vsa['ROW_IL_FACTOR'])
  bound_upper = int(vsa['PARAMETERS']['J_T'] / vsa['ROW_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c2_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  var_prefix = 'U' + str(vsa['KERNEL_ID']) + '_'
  # - SW_KERNEL_CODE
  code = []
  code.append('for (int i = 0; i < %sI; i++){\n' % (var_prefix))
  code.append('  for (int j = 0; j < %sJ; j++){\n' % (var_prefix))
  code.append('    if (init == 1){\n')
  code.append('      global_C[i * %sJ + j] = 0;\n' % (var_prefix))
  code.append('    }\n')
  code.append('    for (int k = 0; k < %sK; k++){\n' % (var_prefix))
  code.append('      global_C[i * %sJ + j] += global_A[i * %sK + k] * global_B[j * %sK + k];\n' % (var_prefix, var_prefix, var_prefix))
  code.append('    }\n')
  code.append('  }\n')
  code.append('}\n')

  vsa['SW_KERNEL_CODE'] = code

  # - LAST_TILE_CODE
  code = []
  code.append('if (k_t == ' + var_prefix + 'K - ' + var_prefix + 'K_T){\n')

  vsa['LAST_TILE_CODE'] = code

  # - LAST_PATCH_CODE
  code = []
  code.append('(k_t == ' + var_prefix + 'K - ' + var_prefix + 'K_T) && (i_t == ' + var_prefix + 'I - ' + var_prefix + 'I_T) && (j_t == ' + var_prefix + 'J - ' + var_prefix + 'J_T)')

  vsa['LAST_PATCH_CODE'] = code

  # - DF_FEED_ADDR_CAL_CODE
  code = []
  code.append('c1_counter * ' + var_prefix + 'K_T + c2_counter * ' + var_prefix + 'SIMD_FACTOR')
  code.append('c0_counter * ' + var_prefix + 'K_T + c2_counter * ' + var_prefix + 'SIMD_FACTOR')

  vsa['DF_FEED_ADDR_CAL_CODE'] = code

  # - DC_COLLECT_ADDR_CAL_CODE
  code = []
  code.append('c1_counter * %sSA_ROWS * %sROW_IL_FACTOR + ((%s - 1 - c2_counter) * %sROW_IL_FACTOR + c0_counter)' % (var_prefix, var_prefix, str(vsa['SA_ROWS']), var_prefix))

  vsa['DC_COLLECT_ADDR_CAL_CODE'] = code

  # - HEAD_CODE
  vsa['HEAD_CODE'] = []

  i_t_width = cal_width(vsa['PARAMETERS']['I'])
  j_t_width = cal_width(vsa['PARAMETERS']['J'])
  k_t_width = cal_width(vsa['PARAMETERS']['K'] / vsa['SIMD_FACTOR'])

  t0_width = cal_width(vsa['PARAMETERS']['I_T'] / vsa['COL_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][0])
  t1_width = cal_width(vsa['COL_IL_FACTOR'])
  t2_width = cal_width(vsa['PARAMETERS']['K_T'] / vsa['FC_SIMD_FACTOR'][0])

  A_head = {}
  code = []
  code.append('for (ap_uint<' + str(i_t_width) + '> i_t = 0; i_t < ' + var_prefix + 'I; i_t += ' + var_prefix + 'I_T){\n')
  code.append('  unsigned int chunk_offset = i_t * ' + var_prefix + 'K;\n')
  code.append('  memcpy((void*)A_buf, (void*)(A + chunk_offset / ' + var_prefix + 'DATA0_PACK_FACTOR), sizeof(' + var_prefix + 'data_t0) * ' + var_prefix + 'I_T * ' + var_prefix + 'K);\n')
  code.append('  for (ap_uint<%s> j_t = 0; j_t < %sJ; j_t += %sJ_T){\n' %(str(j_t_width), var_prefix, var_prefix))
  code.append('    for (ap_uint<' + str(j_t_width) + '> k_t = 0; k_t < ' + var_prefix + 'K / ' + var_prefix + 'SIMD_FACTOR; k_t += ' + var_prefix + 'K_T / ' + var_prefix + 'SIMD_FACTOR){\n')
  code.append('      bool init_internal = (k_t == 0);\n')
  code.append('      bool init_final = init && init_internal;\n')
  code.append('      bool last = (k_t == (' + var_prefix + 'K - ' + var_prefix + 'K_T) / ' + var_prefix + 'SIMD_FACTOR);\n')

  A_head['DRAM_CODE'] = code
  A_head['DRAM_CODE_INDENT_INC'] = 3

  code = []
  code.append('for (ap_uint<' + str(t0_width) + '> t0 = 0; t0 < ' + var_prefix + 'I_T / ' + var_prefix + 'COL_IL_FACTOR / ' + var_prefix + 'DATA0_FC_SPLIT_FACTOR; t0++){\n')
  code.append('  for (ap_uint<' + str(t1_width) + '> t1 = 0; t1 < ' + var_prefix + 'COL_IL_FACTOR; t1++){\n')
  code.append('    for (ap_uint<' + str(t2_width) + '> t2 = 0; t2 < ' + var_prefix + 'K_T / ' + var_prefix + 'DATA0_FC_SIMD_FACTOR; t2++){\n')
  code.append('    #pragma HLS PIPELINE II=1\n')
  code.append('      unsigned int local_i = t0 * ' + var_prefix + 'COL_IL_FACTOR + t1;\n')
  code.append('      unsigned int local_k = k_t * ' + var_prefix + 'SIMD_FACTOR + t2 * ' + var_prefix + 'DATA0_FC_SIMD_FACTOR;\n')
  code.append('      unsigned int feeder_id = t0 / ' + var_prefix + 'DATA0_FC_GROUP_FACTOR;\n')
  code.append('      unsigned int A_index = local_i * ' + var_prefix + 'K + local_k;\n')

  A_head['BRAM_CODE'] = code
  A_head['BRAM_CODE_INDENT_INC'] = 3
  A_head['BRAM_ACCESS_CODE'] = [
      '[A_bus_index]'
      ]

  vsa['HEAD_CODE'].append(A_head)

  B_head = {}

  i_t_width = cal_width(vsa['PARAMETERS']['I'])
  j_t_width = cal_width(vsa['PARAMETERS']['J'])
  k_t_width = cal_width(vsa['PARAMETERS']['K'] / vsa['SIMD_FACTOR'])
  t0_width = cal_width(vsa['PARAMETERS']['J_T'] / vsa['ROW_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][1])
  t1_width = cal_width(vsa['ROW_IL_FACTOR'])
  t2_width = cal_width(vsa['PARAMETERS']['K_T'] / vsa['FC_SIMD_FACTOR'][1])

  code = []
  code.append('for (ap_uint<' + str(i_t_width) + '> i_t = 0; i_t < ' + var_prefix + 'I; i_t += ' + var_prefix + 'I_T){\n')
  code.append('  for (ap_uint<%s> j_t = 0; j_t < %sJ; j_t += %sJ_T){\n' %(str(j_t_width), var_prefix, var_prefix))
  code.append('    unsigned int chunk_offset = j_t *%sK;\n' % (var_prefix))
  code.append('    memcpy((void*)B_buf, (void*)(B + chunk_offset / ' + var_prefix + 'DATA1_PACK_FACTOR), sizeof(' + var_prefix + 'data_t1) * ' + var_prefix + 'J_T * %sK);\n' % (var_prefix))
  code.append('    for (ap_uint<' + str(k_t_width) + '> k_t = 0; k_t < ' + var_prefix + 'K / ' + var_prefix + 'SIMD_FACTOR; k_t += ' + var_prefix + 'K_T / ' + var_prefix + 'SIMD_FACTOR){\n')
  code.append('      bool init_internal = (k_t == 0);\n')
  code.append('      bool init_final = init && init_internal;\n')
  code.append('      bool last = (k_t == (' + var_prefix + 'K - ' + var_prefix + 'K_T) / ' + var_prefix + 'SIMD_FACTOR);\n')
  B_head['DRAM_CODE'] = code
  B_head['DRAM_CODE_INDENT_INC'] = 3

  code = []
  code.append('for (ap_uint<%s> t0 = 0; t0 < %sJ_T / %sROW_IL_FACTOR / %sDATA1_FC_SPLIT_FACTOR; t0++){\n' %(str(t0_width), var_prefix, var_prefix, var_prefix))
  code.append('  for (ap_uint<%s> t1 = 0; t1 < %sROW_IL_FACTOR; t1++){\n' %(str(t1_width), var_prefix))
  code.append('    for (ap_uint<' + str(t2_width) + '> t2 = 0; t2 < ' + var_prefix + 'K_T / ' + var_prefix + 'DATA1_FC_SIMD_FACTOR; t2++){\n')
  code.append('    #pragma HLS PIPELINE II=1\n')
  code.append('      unsigned int local_j = t0 * %sROW_IL_FACTOR + t1;\n' % (var_prefix))
  code.append('      unsigned int local_k = k_t * ' + var_prefix + 'SIMD_FACTOR + t2 * ' + var_prefix + 'DATA1_FC_SIMD_FACTOR;\n')
  code.append('      unsigned int feeder_id = t0 / ' + var_prefix + 'DATA1_FC_GROUP_FACTOR;\n')
  code.append('      unsigned int B_index = local_j * %sK + local_k;\n' %(var_prefix))
  B_head['BRAM_CODE'] = code
  B_head['BRAM_CODE_INDENT_INC'] = 3
  B_head['BRAM_ACCESS_CODE'] = [
      "[B_bus_index]"
      ]

  vsa['HEAD_CODE'].append(B_head)

  C_head = {}

  i_t_width = cal_width(vsa['PARAMETERS']['I'])
  j_t_width = cal_width(vsa['PARAMETERS']['J'])
  k_t_width = cal_width(vsa['PARAMETERS']['K'] / vsa['SIMD_FACTOR'])
  t0_width = cal_width(vsa['PARAMETERS']['I_T'] / vsa['COL_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][2]) + 1
  t1_width = cal_width(vsa['COL_IL_FACTOR'])
  t2_width = cal_width(vsa['PARAMETERS']['J_T'] / vsa['FC_SIMD_FACTOR'][2])

  code = []
  code.append('unsigned int chunk_offset = ((i_t / %sI_T) * (%sJ / %sJ_T) + (j_t / %sJ_T)) * (%sI_T * %sJ_T);\n' % (var_prefix, var_prefix, var_prefix, var_prefix, var_prefix, var_prefix))
  code.append('memcpy((void*)(C + chunk_offset / ' + var_prefix + 'DATA2_PACK_FACTOR), (void*)C_buf, sizeof(' + var_prefix + 'data_t2) * ' + var_prefix + 'I_T * %sJ_T);\n' % (var_prefix))
  C_head['DRAM_CODE'] = code
  C_head['DRAM_CODE_INDENT_INC'] = 0
  C_head['DRAM_ACCESS_CODE'] = [
      '[t0 * ' + var_prefix + 'DATA2_PACK_FACTOR + dup]',
      '[t0]'
      ]
  code = []
  code.append('for (ap_uint<' + str(i_t_width) + '> i_t = 0; i_t < ' + var_prefix + 'I; i_t += ' + var_prefix + 'I_T){\n')
  code.append('  for (ap_uint<%s> j_t = 0; j_t < %sJ; j_t += %sJ_T){\n' % (str(j_t_width), var_prefix, var_prefix))
  code.append('    for (ap_int<' + str(t0_width) + '> t0 = ' + var_prefix + 'I_T / ' + var_prefix + 'COL_IL_FACTOR / ' + var_prefix + 'DATA2_FC_SPLIT_FACTOR - 1; t0 >= 0; t0--){\n')
  code.append('      for (ap_uint<%s> t1 = 0; t1 < %sCOL_IL_FACTOR; t1++){\n' % (str(t1_width), var_prefix))
  code.append('        for (ap_uint<%s> t2 = 0; t2 < %sJ_T / %sDATA2_FC_SIMD_FACTOR; t2++){\n' % (str(t2_width), var_prefix, var_prefix))
  code.append('          #pragma HLS PIPELINE II=1\n')
  code.append('          unsigned int local_i = t0 * ' + var_prefix + 'COL_IL_FACTOR + t1;\n')
  code.append('          unsigned int local_j = t2 * %sDATA2_FC_SIMD_FACTOR;\n' % (var_prefix))
  code.append('          unsigned int C_index = local_i * %sJ_T + local_j;\n\n' % (var_prefix))
  C_head['BRAM_CODE'] = code
  C_head['BRAM_CODE_INDENT_INC'] = [5, 3]
  C_head['BRAM_ACCESS_CODE'] = [
      'local_i * %sJ_T + (local_j + offset)' % (var_prefix)
      ]

  vsa['HEAD_CODE'].append(C_head)


def mv_pass(vsa, config):
  # LOCAL_REG_NUM
  vsa['LOCAL_REG_NUM'] = int(vsa['ROW_IL_FACTOR'] * vsa['COL_IL_FACTOR'])
  # LOCAL_ACCUM_NUM
  vsa['LOCAL_ACCUM_NUM'] = int(vsa['PARAMETERS']['J_T'] / vsa['SIMD_FACTOR'])
  # GLOBAL_ACCUM_NUM
  vsa['GLOBAL_ACCUM_NUM'] = [1, 1]

  # MAC statement
  vsa['MAC_STAT'] = 'sum += op0_u[i] * op1_u[i];\n'

  # generate the buf size
  vsa['DFC_BUF_SIZE'] = []
  vsa['DFC_HEAD_BUF_SIZE'] = []
  vsa['DFC_BUF_SIZE'] = [
      int(vsa['COL_IL_FACTOR'] * vsa['PARAMETERS']['J_T']), \
      vsa['PARAMETERS']['J_T'], \
      vsa['COL_IL_FACTOR']]

  vsa['DFC_HEAD_BUF_SIZE'] = [
      int(vsa['PARAMETERS']['I_T'] * vsa['PARAMETERS']['J']), \
      vsa['PARAMETERS']['J'], \
      vsa['PARAMETERS']['I_T']]

  vsa['ARRAY_SIZE'] = [
      int(vsa['PARAMETERS']['I'] * vsa['PARAMETERS']['J']), \
      int(vsa['PARAMETERS']['J']), \
      int(vsa['PARAMETERS']['I'])]

  # generate the code snippet
  # - DF_FEED_COUNTER
  vsa['DF_FEED_COUNTER'] = []

  width = cal_width(vsa['COL_IL_FACTOR'])
  bound_upper = int(vsa['COL_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c0_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  width = cal_width(vsa['PARAMETERS']['J_T'] / vsa['SIMD_FACTOR'])
  bound_upper = int(vsa['PARAMETERS']['J_T'] / vsa['SIMD_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c1_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  # - DC_COLLECT_COUTNER
  vsa['DC_COLLECT_COUNTER'] = []

  width = cal_width(vsa['COL_IL_FACTOR'])
  bound_upper = int(vsa['COL_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c0_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  var_prefix = 'U' + str(vsa['KERNEL_ID']) + '_'
  # - SW_KERNEL_CODE
  code = []
  code.append('for (int i = 0; i < ' + var_prefix + 'I; i++){\n')
  code.append('  if (init == 1){\n')
  code.append('    global_C[i] = 0;\n')
  code.append('  }\n')
  code.append('  for (int j = 0; j < ' + var_prefix + 'J; j++){\n')
  code.append('    global_C[i] += global_A[i * ' + var_prefix + 'J + j] * global_B[j];\n')
  code.append('  }\n')
  code.append('}\n')

  vsa['SW_KERNEL_CODE'] = code

  # - LAST_TILE_CODE
  code = []
  code.append('if (j_t == ' + var_prefix + 'J - ' + var_prefix + 'J_T){\n')

  vsa['LAST_TILE_CODE'] = code

  # - LAST_PATCH_CODE
  code = []
  code.append('(j_t == ' + var_prefix + 'J - ' + var_prefix + 'J_T) && (i_t == ' + var_prefix + 'I - ' + var_prefix + 'I_T)')

  vsa['LAST_PATCH_CODE'] = code

  # - DF_FEED_ADDR_CAL_CODE
  code = []
  code.append('c0_counter * ' + var_prefix + 'J_T + c1_counter * ' + var_prefix + 'SIMD_FACTOR')
  code.append('c1_counter * ' + var_prefix + 'SIMD_FACTOR')

  vsa['DF_FEED_ADDR_CAL_CODE'] = code

  # - DC_COLLECT_ADDR_CAL_CODE
  code = []
  code.append('c0_counter')

  vsa['DC_COLLECT_ADDR_CAL_CODE'] = code

  # - HEAD_CODE
  vsa['HEAD_CODE'] = []

  i_t_width = cal_width(vsa['PARAMETERS']['I'])
  j_t_width = cal_width(vsa['PARAMETERS']['J'] / vsa['SIMD_FACTOR'])
  t0_width = cal_width(vsa['PARAMETERS']['I_T'] / vsa['COL_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][0])
  t1_width = cal_width(vsa['COL_IL_FACTOR'])
  t2_width = cal_width(vsa['PARAMETERS']['J_T'] / vsa['FC_SIMD_FACTOR'][0])

  A_head = {}
  code = []
  code.append('for (ap_uint<' + str(i_t_width) + '> i_t = 0; i_t < ' + var_prefix + 'I; i_t += ' + var_prefix + 'I_T){\n')
  code.append('  unsigned int chunk_offset = i_t * ' + var_prefix + 'J;\n')
  code.append('  memcpy((void*)A_buf, (void*)(A + chunk_offset / ' + var_prefix + 'DATA0_PACK_FACTOR), sizeof(' + var_prefix + 'data_t0) * ' + var_prefix + 'I_T * ' + var_prefix + 'J);\n')
  code.append('  for (ap_uint<' + str(j_t_width) + '> j_t = 0; j_t < ' + var_prefix + 'J / ' + var_prefix + 'SIMD_FACTOR; j_t += ' + var_prefix + 'J_T / ' + var_prefix + 'SIMD_FACTOR){\n')
  code.append('    bool init_internal = (j_t == 0);\n')
  code.append('    bool init_final = init && init_internal;\n')
  code.append('    bool last = (j_t == (' + var_prefix + 'J - ' + var_prefix + 'J_T) / ' + var_prefix + 'SIMD_FACTOR);\n')

  A_head['DRAM_CODE'] = code
  A_head['DRAM_CODE_INDENT_INC'] = 2

  code = []
  code.append('for (ap_uint<' + str(t0_width) + '> t0 = 0; t0 < ' + var_prefix + 'I_T / ' + var_prefix + 'COL_IL_FACTOR / ' + var_prefix + 'DATA0_FC_SPLIT_FACTOR; t0++){\n')
  code.append('  for (ap_uint<' + str(t1_width) + '> t1 = 0; t1 < ' + var_prefix + 'COL_IL_FACTOR; t1++){\n')
  code.append('    for (ap_uint<' + str(t2_width) + '> t2 = 0; t2 < ' + var_prefix + 'J_T / ' + var_prefix + 'DATA0_FC_SIMD_FACTOR; t2++){\n')
  code.append('    #pragma HLS PIPELINE II=1\n')
  code.append('      unsigned int local_i = t0 * ' + var_prefix + 'COL_IL_FACTOR + t1;\n')
  code.append('      unsigned int local_j = j_t * ' + var_prefix + 'SIMD_FACTOR + t2 * ' + var_prefix + 'DATA0_FC_SIMD_FACTOR;\n')
  code.append('      unsigned int feeder_id = t0 / ' + var_prefix + 'DATA0_FC_GROUP_FACTOR;\n')
  code.append('      unsigned int A_index = local_i * ' + var_prefix + 'J + local_j;\n')

  A_head['BRAM_CODE'] = code
  A_head['BRAM_CODE_INDENT_INC'] = 3
  A_head['BRAM_ACCESS_CODE'] = [
      '[A_bus_index]'
      ]

  vsa['HEAD_CODE'].append(A_head)

  B_head = {}

  i_t_width = cal_width(vsa['PARAMETERS']['I'])
  j_t_width = cal_width(vsa['PARAMETERS']['J'] / vsa['SIMD_FACTOR'])
  t0_width = cal_width(1 / vsa['FC_SPLIT_FACTOR'][1])
  t1_width = cal_width(vsa['PARAMETERS']['J_T'] / vsa['FC_SIMD_FACTOR'][1])

  code = []
  code.append('unsigned int chunk_offset = 0;\n')
  code.append('memcpy((void*)B_buf, (void*)(B + chunk_offset / ' + var_prefix + 'DATA1_PACK_FACTOR), sizeof(' + var_prefix + 'data_t1) * ' + var_prefix + 'J);\n')
  code.append('for (ap_uint<' + str(i_t_width) + '> i_t = 0; i_t < ' + var_prefix + 'I; i_t += ' + var_prefix + 'I_T){\n')
  code.append('  for (ap_uint<' + str(j_t_width) + '> j_t = 0; j_t < ' + var_prefix + 'J / ' + var_prefix + 'SIMD_FACTOR; j_t += ' + var_prefix + 'J_T / ' + var_prefix + 'SIMD_FACTOR){\n')
  code.append('    bool init_internal = (j_t == 0);\n')
  code.append('    bool init_final = init && init_internal;\n')
  code.append('    bool last = (j_t == (' + var_prefix + 'J - ' + var_prefix + 'J_T) / ' + var_prefix + 'SIMD_FACTOR);\n')
  B_head['DRAM_CODE'] = code
  B_head['DRAM_CODE_INDENT_INC'] = 2

  code = []
  code.append('for (ap_uint<' + str(t0_width) + '> t0 = 0; t0 < 1 / ' + var_prefix + 'DATA1_FC_SPLIT_FACTOR; t0++){\n')
  code.append('  for (ap_uint<' + str(t1_width) + '> t1 = 0; t1 < ' + var_prefix + 'J_T / ' + var_prefix + 'DATA1_FC_SIMD_FACTOR; t1++){\n')
  code.append('  #pragma HLS PIPELINE II=1\n')
  code.append('    unsigned int local_j = j_t * ' + var_prefix + 'SIMD_FACTOR + t1 * ' + var_prefix + 'DATA1_FC_SIMD_FACTOR;\n')
  code.append('    unsigned int feeder_id = t0 / ' + var_prefix + 'DATA1_FC_GROUP_FACTOR;\n')
  code.append('    unsigned int B_index = local_j;\n')
  B_head['BRAM_CODE'] = code
  B_head['BRAM_CODE_INDENT_INC'] = 2
  B_head['BRAM_ACCESS_CODE'] = [
      "[B_bus_index]"
      ]

  vsa['HEAD_CODE'].append(B_head)

  C_head = {}

  i_t_width = cal_width(vsa['PARAMETERS']['I'])
  t0_width = cal_width(vsa['PARAMETERS']['I_T'] / vsa['COL_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][2]) + 1
  t1_width = cal_width(vsa['COL_IL_FACTOR'] / vsa['FC_SIMD_FACTOR'][2])

  code = []
  code.append('unsigned int chunk_offset = i_t;\n')
  code.append('memcpy((void*)(C + chunk_offset / ' + var_prefix + 'DATA2_PACK_FACTOR), (void*)C_buf, sizeof(' + var_prefix + 'data_t2) * ' + var_prefix + 'I_T);\n')
  C_head['DRAM_CODE'] = code
  C_head['DRAM_CODE_INDENT_INC'] = 0
  C_head['DRAM_ACCESS_CODE'] = [
      '[t0 * ' + var_prefix + 'DATA2_PACK_FACTOR + dup]',
      '[t0]'
      ]
  code = []
  code.append('for (ap_uint<' + str(i_t_width) + '> i_t = 0; i_t < ' + var_prefix + 'I; i_t += ' + var_prefix + 'I_T){\n')
  code.append('  for (ap_int<' + str(t0_width) + '> t0 = ' + var_prefix + 'I_T / ' + var_prefix + 'COL_IL_FACTOR / ' + var_prefix + 'DATA2_FC_SPLIT_FACTOR - 1; t0 >= 0; t0--){\n')
  code.append('    for (ap_uint<' + str(t1_width) + '> t1 = 0; t1 < ' + var_prefix + 'COL_IL_FACTOR / ' + var_prefix + 'DATA2_FC_SIMD_FACTOR; t1++){\n')
  code.append('    #pragma HLS PIPELINE II=1\n')
  code.append('      unsigned int local_i = t0 * ' + var_prefix + 'COL_IL_FACTOR + t1 * ' + var_prefix + 'DATA2_FC_SIMD_FACTOR;\n')
  code.append('      unsigned int C_index = local_i;\n')

  C_head['BRAM_CODE'] = code
  C_head['BRAM_CODE_INDENT_INC'] = [3, 2]
  C_head['BRAM_ACCESS_CODE'] = [
      'local_i + offset'
      ]

  vsa['HEAD_CODE'].append(C_head)

def nw_pass(vsa, config):
  # INTER_NAME
  vsa['INTER_NAME'] = ['Mleft']
  # INTER_DATA_TYPe
  vsa['INTER_DATA_TYPE'] = ['short']

  # LOCAL_REG_NUM
  vsa['LOCAL_REG_NUM'] = int(vsa['ROW_IL_FACTOR'] * vsa['COL_IL_FACTOR'])
  # LOCAL_ACCUM_NUM
  vsa['LOCAL_ACCUM_NUM'] = int(vsa['PARAMETERS']['BLEN_T'] / vsa['SIMD_FACTOR'])
  # GLOBAL_ACCUM_NUM
  vsa['GLOBAL_ACCUM_NUM'] = [1, 1]

  # MAC statement
  vsa['MAC_STAT'] = 'sum += op0_u[i] * op1_u[i];\n'

  # generate the buf size
  vsa['DFC_BUF_SIZE'] = []
  vsa['DFC_HEAD_BUF_SIZE'] = []
  vsa['DFC_BUF_SIZE'] = [
      int(vsa['COL_IL_FACTOR']), \
      int(vsa['PARAMETERS']['BLEN_T']), \
      int(vsa['COL_IL_FACTOR'] * vsa['PARAMETERS']['BLEN_T'])]

  vsa['DFC_HEAD_BUF_SIZE'] = [
      vsa['PARAMETERS']['ALEN'], \
      vsa['PARAMETERS']['BLEN'], \
      int(vsa['PARAMETERS']['ALEN'] * vsa['PARAMETERS']['BLEN'])]

  vsa['ARRAY_SIZE'] = [
      int(vsa['PARAMETERS']['ALEN']), \
      int(vsa['PARAMETERS']['BLEN']), \
      int(vsa['PARAMETERS']['ALEN'] * vsa['PARAMETERS']['BLEN'])]

  # generate the code snippet
  # - DF_FEED_COUNTER
  vsa['DF_FEED_COUNTER'] = []

  width = cal_width(vsa['COL_IL_FACTOR'])
  bound_upper = int(vsa['COL_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c0_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  width = cal_width(vsa['PARAMETERS']['BLEN_T'] / vsa['SIMD_FACTOR'])
  bound_upper = int(vsa['PARAMETERS']['BLEN_T'] / vsa['SIMD_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c1_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DF_FEED_COUNTER'].append(counter)

  # - DC_COLLECT_COUTNER
  vsa['DC_COLLECT_COUNTER'] = []

  width = cal_width(vsa['COL_IL_FACTOR'])
  bound_upper = int(vsa['COL_IL_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c0_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  width = cal_width(vsa['PARAMETERS']['BLEN_T'] / vsa['SIMD_FACTOR'])
  bound_upper = int(vsa['PARAMETERS']['BLEN_T'] / vsa['SIMD_FACTOR'])
  counter = {}
  counter['VARIABLE'] = 'c1_counter'
  counter['WIDTH'] = width
  counter['BOUND'] = [0, bound_upper]
  vsa['DC_COLLECT_COUNTER'].append(counter)

  var_prefix = 'U' + str(vsa['KERNEL_ID']) + '_'
  # - SW_KERNEL_CODE
  code = []
  code.append('short M[(%sALEN + 1) * (%sBLEN + 1)];\n' % (var_prefix, var_prefix))
  code.append('for (int a_idx = 0; a_idx < (%sALEN + 1); a_idx++){\n' % (var_prefix))
  code.append('  M[a_idx] = a_idx * %sGAP_SCORE;\n' % (var_prefix))
  code.append('}\n')
  code.append('for (int b_idx = 0; b_idx < (%sBLEN + 1); b_idx++){\n' % (var_prefix))
  code.append('  M[b_idx * (%sALEN + 1)] = b_idx * %sGAP_SCORE;\n' % (var_prefix, var_prefix))
  code.append('}\n')
  code.append('for (int b_idx = 1; b_idx < (%sBLEN + 1); b_idx++){\n' % (var_prefix))
  code.append('  for (int a_idx = 1; a_idx < (%sALEN + 1); a_idx++){\n' % (var_prefix))
  code.append('    short score;\n')
  code.append('    if (global_SEQA[a_idx - 1] == global_SEQB[b_idx - 1]){\n')
  code.append('      score = %sMATCH_SCORE;\n' % (var_prefix))
  code.append('    } else {\n')
  code.append('      score = %sMISMATCH_SCORE;\n' % (var_prefix))
  code.append('    }\n')
  code.append('    int row_up = (b_idx - 1) * (%sALEN + 1);\n' % (var_prefix))
  code.append('    int row = b_idx * (%sALEN + 1);\n' % (var_prefix))
  code.append('    short up_left = M[row_up + (a_idx - 1)] + score;\n')
  code.append('    short up = M[row_up + a_idx] + %sGAP_SCORE;\n' % (var_prefix))
  code.append('    short left = M[row + a_idx - 1] + %sGAP_SCORE;\n' % (var_prefix))
  code.append('    short max1 = (up > left)? up : left;\n')
  code.append('    short max = (up_left > max1)? up_left : max1;\n')
  code.append('    M[row + a_idx] = max;\n')
  code.append('    if (max == left){\n')
  code.append('      global_PTR[(a_idx - 1) * (%sBLEN) + (b_idx - 1)] = %sSKIPB;\n' % (var_prefix, var_prefix))
  code.append('    } else if (max == up){\n')
  code.append('      global_PTR[(a_idx - 1) * (%sBLEN) + (b_idx - 1)] = %sSKIPA;\n' % (var_prefix, var_prefix))
  code.append('    } else {\n')
  code.append('      global_PTR[(a_idx - 1) * (%sBLEN) + (b_idx - 1)] = %sALIGN;\n' % (var_prefix, var_prefix))
  code.append('    }\n')
  code.append('  }\n')
  code.append('}\n')

  vsa['SW_KERNEL_CODE'] = code

  # - LAST_TILE_CODE
  code = []
  code.append('if (1){\n')

  vsa['LAST_TILE_CODE'] = code

  # - LAST_PATCH_CODE
  code = []
  code.append('(b_idx_t == ' + var_prefix + 'BLEN - ' + var_prefix + 'BLEN_T) && (a_idx_t == ' + var_prefix + 'ALEN - ' + var_prefix + 'ALEN_T)')

  vsa['LAST_PATCH_CODE'] = code

  # - DF_FEED_ADDR_CAL_CODE
  code = []
  code.append('c0_counter')
  code.append('c1_counter')

  vsa['DF_FEED_ADDR_CAL_CODE'] = code

  # - DC_COLLECT_ADDR_CAL_CODE
  code = []
  code.append('c0_counter * %sBLEN_T + c1_counter' % (var_prefix))

  vsa['DC_COLLECT_ADDR_CAL_CODE'] = code

  # - HEAD_CODE
  vsa['HEAD_CODE'] = []

  a_idx_t_width = cal_width(vsa['PARAMETERS']['ALEN'])
  b_idx_t_width = cal_width(vsa['PARAMETERS']['BLEN'])

  t0_width = cal_width(vsa['PARAMETERS']['ALEN_T'] / vsa['COL_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][0])
  t1_width = cal_width(vsa['COL_IL_FACTOR'] / vsa['FC_SIMD_FACTOR'][0])

  A_head = {}
  code = []
  code.append('unsigned int chunk_offset = 0;\n')
  code.append('memcpy((void*)SEQA_buf, (void*)(SEQA + chunk_offset / %sDATA0_PACK_FACTOR), sizeof(%sdata_t0) * %sALEN);\n' % (var_prefix, var_prefix, var_prefix))
  code.append('for (ap_uint<' + str(a_idx_t_width) + '> a_idx_t = 0; a_idx_t < ' + var_prefix + 'ALEN; a_idx_t += ' + var_prefix + 'ALEN_T){\n')
  code.append('  for (ap_uint<' + str(b_idx_t_width) + '> b_idx_t = 0; b_idx_t < ' + var_prefix + 'BLEN / ' + var_prefix + 'SIMD_FACTOR; b_idx_t += ' + var_prefix + 'BLEN_T / ' + var_prefix + 'SIMD_FACTOR){\n')
  code.append('    bool init_internal = (b_idx_t == 0);\n')
  code.append('    bool init_final = init && init_internal;\n')
  code.append('    bool last = (b_idx_t == (' + var_prefix + 'BLEN - ' + var_prefix + 'BLEN_T) / ' + var_prefix + 'SIMD_FACTOR);\n')

  A_head['DRAM_CODE'] = code
  A_head['DRAM_CODE_INDENT_INC'] = 2

  code = []
  code.append('for (ap_uint<' + str(t0_width) + '> t0 = 0; t0 < ' + var_prefix + 'ALEN_T / ' + var_prefix + 'COL_IL_FACTOR / ' + var_prefix + 'DATA0_FC_SPLIT_FACTOR; t0++){\n')
  code.append('  for (ap_uint<' + str(t1_width) + '> t1 = 0; t1 < ' + var_prefix + 'COL_IL_FACTOR / %sDATA0_FC_SIMD_FACTOR; t1++){\n' % (var_prefix))
  code.append('    #pragma HLS PIPELINE II=1\n')
  code.append('    unsigned int local_a_idx = a_idx_t + t0 * %sCOL_IL_FACTOR + t1 * %sDATA0_FC_SIMD_FACTOR;\n' % (var_prefix, var_prefix))
  code.append('    unsigned int feeder_id = t0 / ' + var_prefix + 'DATA0_FC_GROUP_FACTOR;\n')
  code.append('    unsigned int SEQA_index = local_a_idx;\n')

  A_head['BRAM_CODE'] = code
  A_head['BRAM_CODE_INDENT_INC'] = 2
  A_head['BRAM_ACCESS_CODE'] = [
      '[A_bus_index]'
      ]

  vsa['HEAD_CODE'].append(A_head)

  B_head = {}

  a_idx_t_width = cal_width(vsa['PARAMETERS']['ALEN'])
  b_idx_t_width = cal_width(vsa['PARAMETERS']['BLEN'])

  t0_width = cal_width(1 / vsa['FC_SPLIT_FACTOR'][1])
  t1_width = cal_width(vsa['PARAMETERS']['BLEN'] / vsa['FC_SIMD_FACTOR'][1])

  code = []
  code.append('unsigned int chunk_offset = 0;\n')
  code.append('memcpy((void*)SEQB_buf, (void*)(SEQB + chunk_offset / ' + var_prefix + 'DATA1_PACK_FACTOR), sizeof(' + var_prefix + 'data_t1) * ' + var_prefix + 'BLEN);\n')
  code.append('for (ap_uint<' + str(a_idx_t_width) + '> a_idx_t = 0; a_idx_t < ' + var_prefix + 'ALEN; a_idx_t += ' + var_prefix + 'ALEN_T){\n')
  code.append('  for (ap_uint<' + str(b_idx_t_width) + '> b_idx_t = 0; b_idx_t < ' + var_prefix + 'BLEN / ' + var_prefix + 'SIMD_FACTOR; b_idx_t += ' + var_prefix + 'BLEN_T / ' + var_prefix + 'SIMD_FACTOR){\n')
  code.append('    bool init_internal = (b_idx_t == 0);\n')
  code.append('    bool init_final = init && init_internal;\n')
  code.append('    bool last = (b_idx_t == (' + var_prefix + 'BLEN - ' + var_prefix + 'BLEN_T) / ' + var_prefix + 'SIMD_FACTOR);\n')
  B_head['DRAM_CODE'] = code
  B_head['DRAM_CODE_INDENT_INC'] = 2

  code = []
  code.append('for (ap_uint<' + str(t0_width) + '> t0 = 0; t0 < 1 / ' + var_prefix + 'DATA1_FC_SPLIT_FACTOR; t0++){\n')
  code.append('  for (ap_uint<' + str(t1_width) + '> t1 = 0; t1 < ' + var_prefix + 'BLEN_T / ' + var_prefix + 'DATA1_FC_SIMD_FACTOR; t1++){\n')
  code.append('  #pragma HLS PIPELINE II=1\n')
  code.append('    unsigned int local_b_idx = b_idx_t + t1 * %sDATA1_FC_SIMD_FACTOR;\n' % (var_prefix))
  code.append('    unsigned int feeder_id = t0 / ' + var_prefix + 'DATA1_FC_GROUP_FACTOR;\n')
  code.append('    unsigned int SEQB_index = local_b_idx;\n')
  B_head['BRAM_CODE'] = code
  B_head['BRAM_CODE_INDENT_INC'] = 2
  B_head['BRAM_ACCESS_CODE'] = [
      "[B_bus_index]"
      ]

  vsa['HEAD_CODE'].append(B_head)

  C_head = {}

  a_idx_t_width = cal_width(vsa['PARAMETERS']['ALEN'])
  b_idx_t_width = cal_width(vsa['PARAMETERS']['BLEN'])

  t0_width = cal_width(vsa['PARAMETERS']['ALEN_T'] / vsa['COL_IL_FACTOR'] / vsa['FC_SPLIT_FACTOR'][2]) + 1
  t1_width = cal_width(vsa['COL_IL_FACTOR'])
  t2_width = cal_width(vsa['PARAMETERS']['BLEN_T'] / vsa['FC_SIMD_FACTOR'][2])

  code = []
  code.append('unsigned int chunk_offset = a_idx_t * %sALEN_T * %sBLEN;\n' % (var_prefix, var_prefix))
  code.append('memcpy((void*)(PTR + chunk_offset / ' + var_prefix + 'DATA2_PACK_FACTOR), (void*)PTR_buf, sizeof(' + var_prefix + 'data_t2) * ' + var_prefix + 'ALEN_T * %sBLEN);\n' % (var_prefix))
  C_head['DRAM_CODE'] = code
  C_head['DRAM_CODE_INDENT_INC'] = 0
#  C_head['DRAM_ACCESS_CODE'] = [
#      '[t0 * ' + var_prefix + 'DATA2_PACK_FACTOR + dup]',
#      '[t0]'
#      ]
  code = []
  code.append('for (ap_uint<' + str(a_idx_t_width) + '> a_idx_t = 0; a_idx_t < ' + var_prefix + 'ALEN; a_idx_t += ' + var_prefix + 'ALEN_T){\n')
  code.append('  for (ap_uint<%s> b_idx_t = 0; b_idx_t < %sBLEN; b_idx_t += %sBLEN_T){\n' % (b_idx_t_width, var_prefix, var_prefix))
  code.append('    for (ap_int<' + str(t0_width) + '> t0 = ' + var_prefix + 'ALEN_T / ' + var_prefix + 'COL_IL_FACTOR / ' + var_prefix + 'DATA2_FC_SPLIT_FACTOR - 1; t0 >= 0; t0--){\n')
  code.append('      for (ap_uint<%s> t1 = 0; t1 < %sCOL_IL_FACTOR; t1++){\n' % (str(t1_width), var_prefix))
  code.append('        for (ap_uint<%s> t2 = 0; t2 < %sBLEN_T / %sDATA2_FC_SIMD_FACTOR; t2++){\n' % (str(t2_width), var_prefix, var_prefix))
  code.append('        #pragma HLS PIPELINE II=1\n')
  code.append('          unsigned int local_a_idx = a_idx_t + t0 * %sCOL_IL_FACTOR + t1;\n' % (var_prefix))
  code.append('          unsigned int local_b_idx = b_idx_t + t2 * %sDATA2_FC_SIMD_FACTOR;\n' % (var_prefix))
  code.append('          unsigned int PTR_index = local_a_idx * %sBLEN + local_b_idx;\n' % (var_prefix))

  C_head['BRAM_CODE'] = code
  C_head['BRAM_CODE_INDENT_INC'] = [5, 4]
  C_head['BRAM_ACCESS_CODE'] = [
      'local_i + offset'
      ]

  vsa['HEAD_CODE'].append(C_head)

  # compute
  vsa['COMPUTE_CODE'] = {}

  code = []
  code.append('short M = (pe_col_id + 1) * %sGAP_SCORE;\n' % (var_prefix))
  code.append('short Mleft = (pe_col_id) * %sGAP_SCORE;\n' % (var_prefix))
  code.append('for (int la_counter = 0; la_counter < %sLOCAL_ACCUM_NUM; la_counter++){\n' % (var_prefix))
  if vsa['COL_IL_FACTOR'] == 1:
    code.append('  //for (int local_reg_id = 0; local_reg_id < %sLOCAL_REG_NUM; local_reg_id++){\n' % (var_prefix))
  else:
    code.append('  for (int local_reg_id = 0; local_reg_id < %sLOCAL_REG_NUM; local_reg_id++){\n' % (var_prefix))
  code.append('  #pragma HLS PIPELINE II=1\n')
  code.append('    %sData0PEChannelType fifo0_in_data;\n' % (var_prefix))
  code.append('    fifo0_in_data = fifo0_local.read();\n')
  code.append('    ap_uint<8> op0_data = fifo0_in_data.data;\n')
  code.append('    char op0_u = Reinterpret<U1_data_t0>(op0_data);\n')

  code.append('    %sData1PEChannelType fifo1_in_data;\n' % (var_prefix))
  code.append('    fifo1_in_data = fifo1_local.read();\n')
  code.append('    ap_uint<8> op1_data = fifo1_in_data.data;\n')
  code.append('    char op1_u = Reinterpret<U1_data_t1>(op1_data);\n')

  code.append('    bool init = fifo0_in_data.new_pair;\n')
  code.append('    bool last = fifo1_in_data.last_pair;\n')
  code.append('    short M_prev = M;\n')
  code.append('    short Mleft_prev = Mleft;\n')

#  code.append('    short Mleft = fifo_Mleft_local_in.read();\n')
  code.append('    Mleft = (la_counter + 1) * %sGAP_SCORE;\n' % (var_prefix))

  code.append('    short score;\n')
  code.append('    if (op0_u == op1_u){\n')
  code.append('      score = %sMATCH_SCORE;\n' % (var_prefix))
  code.append('    } else {\n')
  code.append('      score = %sMISMATCH_SCORE;\n' % (var_prefix))
  code.append('    }\n')
  code.append('    short up_left = Mleft_prev + score;\n')
  code.append('    short up = M + %sGAP_SCORE;\n' % (var_prefix))
  code.append('    short left = Mleft + %sGAP_SCORE;\n' % (var_prefix))

  code.append('    short max1 = (up > left)? up : left;\n')
  code.append('    short max = (up_left > max1)? up_left : max1;\n')
  code.append('    M = max;\n')
  code.append('    fifo_Mleft_local_out.write(M);\n')

  code.append('    char ptr;\n')
  code.append('    if (max == left){\n')
  code.append('      ptr = %sSKIPB;\n' % (var_prefix))
  code.append('    } else if (max == up){\n')
  code.append('      ptr = %sSKIPA;\n' % (var_prefix))
  code.append('    } else {\n')
  code.append('      ptr = %sALIGN;\n' % (var_prefix))
  code.append('    }\n')
  code.append('    fifo2_local.write(%sData2PEChannelType(ptr));\n' % (var_prefix))
  if vsa['COL_IL_FACTOR'] == 1:
    code.append('  //}\n')
  else:
    code.append('  }\n')
  code.append('}\n')

  vsa['COMPUTE_CODE']['FIRST'] = code

  code = []
  code.append('short M = (pe_col_id + 1) * %sGAP_SCORE;\n' % (var_prefix))
  code.append('short Mleft = (pe_col_id) * %sGAP_SCORE;\n' % (var_prefix))
  code.append('for (int la_counter = 0; la_counter < %sLOCAL_ACCUM_NUM; la_counter++){\n' % (var_prefix))
  if vsa['COL_IL_FACTOR'] == 1:
    code.append('  //for (int local_reg_id = 0; local_reg_id < %sLOCAL_REG_NUM; local_reg_id++){\n' % (var_prefix))
  else:
    code.append('  for (int local_reg_id = 0; local_reg_id < %sLOCAL_REG_NUM; local_reg_id++){\n' % (var_prefix))
  code.append('  #pragma HLS PIPELINE II=1\n')
  code.append('    %sData0PEChannelType fifo0_in_data;\n' % (var_prefix))
  code.append('    fifo0_in_data = fifo0_local.read();\n')
  code.append('    ap_uint<8> op0_data = fifo0_in_data.data;\n')
  code.append('    char op0_u = Reinterpret<U1_data_t0>(op0_data);\n')

  code.append('    %sData1PEChannelType fifo1_in_data;\n' % (var_prefix))
  code.append('    fifo1_in_data = fifo1_local.read();\n')
  code.append('    ap_uint<8> op1_data = fifo1_in_data.data;\n')
  code.append('    char op1_u = Reinterpret<U1_data_t1>(op1_data);\n')

  code.append('    bool init = fifo0_in_data.new_pair;\n')
  code.append('    bool last = fifo1_in_data.last_pair;\n')
  code.append('    short M_prev = M;\n')
  code.append('    short Mleft_prev = Mleft;\n')

  code.append('    Mleft = fifo_Mleft_local_in.read();\n')

  code.append('    short score;\n')
  code.append('    if (op0_u == op1_u){\n')
  code.append('      score = %sMATCH_SCORE;\n' % (var_prefix))
  code.append('    } else {\n')
  code.append('      score = %sMISMATCH_SCORE;\n' % (var_prefix))
  code.append('    }\n')
  code.append('    short up_left = Mleft_prev + score;\n')
  code.append('    short up = M + %sGAP_SCORE;\n' % (var_prefix))
  code.append('    short left = Mleft + %sGAP_SCORE;\n' % (var_prefix))

  code.append('    short max1 = (up > left)? up : left;\n')
  code.append('    short max = (up_left > max1)? up_left : max1;\n')
  code.append('    M = max;\n')
  code.append('    fifo_Mleft_local_out.write(M);\n')

  code.append('    char ptr;\n')
  code.append('    if (max == left){\n')
  code.append('      ptr = %sSKIPB;\n' % (var_prefix))
  code.append('    } else if (max == up){\n')
  code.append('      ptr = %sSKIPA;\n' % (var_prefix))
  code.append('    } else {\n')
  code.append('      ptr = %sALIGN;\n' % (var_prefix))
  code.append('    }\n')
  code.append('    fifo2_local.write(%sData2PEChannelType(ptr));\n' % (var_prefix))
  if vsa['COL_IL_FACTOR'] == 1:
    code.append('  //}\n')
  else:
    code.append('  }\n')
  code.append('}\n')

  vsa['COMPUTE_CODE']['MIDDLE'] = code

  code = []
  code.append('short M = (pe_col_id + 1) * %sGAP_SCORE;\n' % (var_prefix))
  code.append('short Mleft = (pe_col_id) * %sGAP_SCORE;\n' % (var_prefix))
  code.append('for (int la_counter = 0; la_counter < %sLOCAL_ACCUM_NUM; la_counter++){\n' % (var_prefix))
  if vsa['COL_IL_FACTOR'] == 1:
    code.append('  //for (int local_reg_id = 0; local_reg_id < %sLOCAL_REG_NUM; local_reg_id++){\n' % (var_prefix))
  else:
    code.append('  for (int local_reg_id = 0; local_reg_id < %sLOCAL_REG_NUM; local_reg_id++){\n' % (var_prefix))
  code.append('  #pragma HLS PIPELINE II=1\n')
  code.append('    %sData0PEChannelType fifo0_in_data;\n' % (var_prefix))
  code.append('    fifo0_in_data = fifo0_local.read();\n')
  code.append('    ap_uint<8> op0_data = fifo0_in_data.data;\n')
  code.append('    char op0_u = Reinterpret<U1_data_t0>(op0_data);\n')

  code.append('    %sData1PEChannelType fifo1_in_data;\n' % (var_prefix))
  code.append('    fifo1_in_data = fifo1_local.read();\n')
  code.append('    ap_uint<8> op1_data = fifo1_in_data.data;\n')
  code.append('    char op1_u = Reinterpret<U1_data_t1>(op1_data);\n')

  code.append('    bool init = fifo0_in_data.new_pair;\n')
  code.append('    bool last = fifo1_in_data.last_pair;\n')
  code.append('    short M_prev = M;\n')
  code.append('    short Mleft_prev = Mleft;\n')

  code.append('    Mleft = fifo_Mleft_local_in.read();\n')

  code.append('    short score;\n')
  code.append('    if (op0_u == op1_u){\n')
  code.append('      score = %sMATCH_SCORE;\n' % (var_prefix))
  code.append('    } else {\n')
  code.append('      score = %sMISMATCH_SCORE;\n' % (var_prefix))
  code.append('    }\n')
  code.append('    short up_left = Mleft_prev + score;\n')
  code.append('    short up = M + %sGAP_SCORE;\n' % (var_prefix))
  code.append('    short left = Mleft + %sGAP_SCORE;\n' % (var_prefix))

  code.append('    short max1 = (up > left)? up : left;\n')
  code.append('    short max = (up_left > max1)? up_left : max1;\n')
  code.append('    M = max;\n')

  code.append('    char ptr;\n')
  code.append('    if (max == left){\n')
  code.append('      ptr = %sSKIPB;\n' % (var_prefix))
  code.append('    } else if (max == up){\n')
  code.append('      ptr = %sSKIPA;\n' % (var_prefix))
  code.append('    } else {\n')
  code.append('      ptr = %sALIGN;\n' % (var_prefix))
  code.append('    }\n')
  code.append('    fifo2_local.write(%sData2PEChannelType(ptr));\n' % (var_prefix))
  if vsa['COL_IL_FACTOR'] == 1:
    code.append('  //}\n')
  else:
    code.append('  }\n')
  code.append('}\n')

  vsa['COMPUTE_CODE']['LAST'] = code

def vsa_second_pass(vsa, config):
#  with open(input_file, 'r') as f:
#    features = json.loads(f.read())
#    for param in features['PARAMETERS']:
#      vsa['PARAMETERS'][param] = int(features['PARAMETERS'][param])
#
#    for iter_idx in range(len(features['ITERATORS'])):
#      vsa['ITERATORS'].append(features['ITERATORS'][iter_idx])
#
#    vsa['APP_NAME'] = features['APP_NAME']
#    vsa['TYPE'] = features['TYPE']
#    vsa['SA_ROWS'] = features['SA_ROWS']
#    vsa['SA_COLS'] = features['SA_COLS']
#    vsa['OP_CHANNEL_DIR'] = features['OP_CHANNEL_DIR']
#    vsa['RES_CHANNEL_DIR'] = features['RES_CHANNEL_DIR']
#    vsa['BUS_WIDTH'] = features['BUS_WIDTH']
#    vsa['SIMD_FACTOR'] = features['SIMD_FACTOR']
#    vsa['FC_SIMD_FACTOR'] = features['FC_SIMD_FACTOR']
#    vsa['FC_GROUP_FACTOR'] = features['FC_GROUP_FACTOR']
#    vsa['FC_SPLIT_FACTOR'] = features['FC_SPLIT_FACTOR']
#    vsa['KERNEL_ID'] = features['KERNEL_ID']
#    vsa['FIXED_EN'] = features['FIXED_EN']
#    vsa['IL_ENABLE'] = features['IL_ENABLE']
#    vsa['ROW_IL_FACTOR'] = features['ROW_IL_FACTOR']
#    vsa['COL_IL_FACTOR'] = features['COL_IL_FACTOR']

  # calculate
  vsa['OP_CHANNEL_NUM'] = len(vsa['OP_CHANNEL_DIR'])
  vsa['RES_CHANNEL_NUM'] = len(vsa['RES_CHANNEL_DIR'])
  vsa['CHANNEL_DEPTH'] = 2
  vsa['OP_PE_SIMD_WIDTH'] = []
  for idx in range(vsa['OP_CHANNEL_NUM']):
    vsa['OP_PE_SIMD_WIDTH'].append(vsa['SIMD_FACTOR'] * vsa['DATA_WIDTH'][idx])

  vsa['OP_ENGINE_NUM'] = []
  for idx in range(vsa['OP_CHANNEL_NUM']):
    ch_dir = vsa['OP_CHANNEL_DIR'][idx]
    if ch_dir == 'D' or ch_dir == 'U':
      engine_num = vsa['SA_COLS']
    else:
      engine_num = vsa['SA_ROWS']
    group_factor = vsa['FC_GROUP_FACTOR'][idx]
#    print(engine_num)
#    print(split_factor)
    engine_num = int(engine_num / group_factor)
    vsa['OP_ENGINE_NUM'].append(engine_num)

  vsa['RES_ENGINE_NUM'] = []
  for idx in range(vsa['RES_CHANNEL_NUM']):
    ch_dir = vsa['RES_CHANNEL_DIR'][idx]
    if ch_dir == 'D' or ch_dir == 'U':
      engine_num = vsa['SA_COLS']
    else:
      engine_num = vsa['SA_ROWS']
    group_factor = vsa['FC_GROUP_FACTOR'][idx + vsa['OP_CHANNEL_NUM']]
    engine_num = int(engine_num / group_factor)
    vsa['RES_ENGINE_NUM'].append(engine_num)

  # will be deprecated in the future
  if vsa['APP_NAME'] == 'mv':
    mv_pass(vsa, config)
  elif vsa['APP_NAME'] == 'mm':
    mm_pass(vsa, config)
  elif vsa['APP_NAME'] == 'cnn':
    cnn_pass(vsa, config)
  elif vsa['APP_NAME'] == 'nw':
    nw_pass(vsa, config)

  return vsa

def vsa_first_pass(input_file, vsa, config):
  output = []
  with open(input_file, 'r') as f:
    for i in f.readlines():
      output.append(i)
    for line_id in range(len(output)):
      line = output[line_id]
      line_split = line.split(' ')
      if line.find('#define') >= 0:
        if line.find('INIT_VALUE') >= 0:
          vsa['INIT_VALUE'] = float(line_split[2])
        elif line.find('MAC_STAT') >= 0:
          vsa['MAC_STAT'] = line_split[2].strip() + '\n'
        else:
          vsa['PARAMETERS'][line_split[1]] = int(line_split[2].strip())

      if line.find('typedef') >= 0:
        data_type = line_split[1].strip()
        vsa['DATA_TYPE'].append(data_type)
        vsa['DATA_WIDTH'].append(config['TYPE_WIDTH'][data_type])

      if line.find('input buffers') >= 0:
        code_block = []
        for cur_line_id in range(line_id + 1, len(output)):
          line = output[cur_line_id]
          line_split = line.split()
          if line.find('//') >= 0:
            break
#          print(line_split)
          vsa['OP_REF'].append(line_split[1].strip(';'))

      if line.find('output buffers') >= 0:
        code_block = []
        for cur_line_id in range(line_id + 1, len(output)):
          line = output[cur_line_id]
          line_split = line.split()
          if line.find('//') >= 0:
            break
          vsa['RES_REF'].append(line_split[1].strip(';'))

  for ref in vsa['OP_REF']:
    ref_split = ref.split('[')
    vsa['OP_NAME'].append(ref_split[0])
    vsa['OP_DIM'].append(int(len(ref_split) - 1))

  for ref in vsa['RES_REF']:
    ref_split = ref.split('[')
    vsa['RES_NAME'].append(ref_split[0])
    vsa['RES_DIM'].append(int(len(ref_split) - 1))

def vsa_init(input_file, config):
  with open(input_file, 'r') as f:
    vsa = json.loads(f.read())
  return vsa

def vsa_dump(output_file, vsa, config):
  with open(output_file, 'w') as f:
    json.dump(vsa, f, indent=2)

def run(input):
  pwd_dir = os.path.dirname(os.path.realpath(__file__))
#  print(pwd_dir)
  if not os.path.exists(pwd_dir + '/output'):
    os.makedirs(pwd_dir + '/output')

  type_width = {
      'float': 32,
      'int': 32,
      'char': 8
      }
  config = {}
  config['TYPE_WIDTH'] = type_width

  vsa = vsa_init(input, config)

#  vsa_first_pass(input, vsa, config)
  vsa = vsa_second_pass(vsa, config)

  vsa_dump(pwd_dir + '/output/design_desp.json', vsa, config)

if __name__ == "__main__":

  parser = argparse.ArgumentParser(description='Generate VSA descriptors for applications.')
  parser.add_argument('-i', '--input', metavar='INPUT', required=True, help='input file for the application to be analyzed')
#  parser.add_argument('-f', '--features', metavar='FEATURE', required=True, help='added architecture features from Optimizer')

  args = parser.parse_args()
  #print(args.input)

  run(args.input)
