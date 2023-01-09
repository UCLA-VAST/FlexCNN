import numpy as np
import json
import argparse
import copy
import multiprocessing
import subprocess
import time
import pandas as pd
import math

def list_split(ori_list, split_num):
  chunk_size = int(np.ceil(float(len(ori_list)) / split_num))
  chunks = [ori_list[i: i + min(chunk_size, len(ori_list) - i)] for i in range(0, len(ori_list), chunk_size)]
  return chunks
  
def effective_dram_est(port_width, burst_len, fre):
  dram_latency = 250
  eff_bw = port_width * burst_len / 8 / ((dram_latency + burst_len) / (fre * 1e6)) / 1e9
  eff_port_width = port_width * burst_len / (dram_latency + burst_len)
  return eff_bw, eff_port_width

def cin_load_est(in_num_t, in_h_t, in_w_t, fh, fw, lane, dw, port_width, fre):
  burst_len = 0
  if fh == 1:
    burst_len = in_h_t * in_w_t * in_num_t / (port_width / dw)
  else:
    burst_len = (in_w_t + fw - 1) * in_num_t / (port_width / dw)
  eff_bw, eff_port_width = effective_dram_est(port_width, burst_len, fre)
  load_phase_latency = in_num_t * (fh - 1 + in_h_t) * (fw - 1 + in_w_t) / (eff_port_width / dw)
  write_phase_latency = in_num_t * (fh - 1 + in_h_t) * (fw - 1 + in_w_t) / lane
  return max(load_phase_latency, write_phase_latency)

def weight_load_est(in_num_t, out_num_t, fh1, fw1, fh2, fw2, lane, dw1, dw2, dw3, port_width, depth_en, point_en, bias_en, fre):
  burst_len1 = in_num_t * fh1 * fw1 / (port_width / dw1)
  eff_bw1, eff_port_width1 = effective_dram_est(port_width, burst_len1, fre)
  burst_len2 = in_num_t * out_num_t * fh2 * fw2 / (port_width / dw2)
  eff_bw2, eff_port_width2 = effective_dram_est(port_width, burst_len2, fre)
  burst_len3 = out_num_t / (port_width / dw3)
  eff_bw3, eff_port_width3 = effective_dram_est(port_width, burst_len3, fre)

  load_phase_latency = 0
  write_phase_latency = 0
  if depth_en == 1:
    load_phase_latency += in_num_t * fh1 * fw1 / (eff_port_width1 / dw1)
  if point_en == 1:
    load_phase_latency += in_num_t * out_num_t * fh2 * fw2 / (eff_port_width2 / dw2)
  if bias_en == 1:
    load_phase_latency += out_num_t / (eff_port_width3 / dw3)

  if depth_en == 1:
    write_phase_latency = max(write_phase_latency, in_num_t * fh1 * fw1 / lane)
  if point_en == 1:
    write_phase_latency = max(write_phase_latency, in_num_t * out_num_t * fh2 * fw2 / lane)
  if bias_en == 1:
    write_phase_latency = max(write_phase_latency, out_num_t / lane)

  return load_phase_latency + write_phase_latency

def inter_load_est(in_num_t, in_h_t, in_w_t, fh, fw, lane):
  return in_num_t * (fh - 1 + in_h_t) * (fw - 1 + in_w_t) / lane

def depth_conv_est(in_num_t, in_h_t, in_w_t, fh, fw, lane):
  return in_num_t * (fh - 1 + in_h_t) * (fw - 1 + in_w_t) / lane

def point_conv_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, fh1, fw1, fh2, fw2, lane, sa_rows, sa_cols, sa_lane):
  cin_load = in_num_t * (fh1 - 1 + out_h_t) * (fw1 - 1 + out_w_t) / lane
  weight_load = in_num_t * out_num_t * fh2 * fw2 / lane
  load_phase_latency = max(cin_load, weight_load)
  compute_phase_latency = in_num_t * out_num_t * out_h_t * out_w_t * fh2 * fw2 / sa_rows / sa_cols / sa_lane
  compute_drain_latency = out_num_t * out_w_t / sa_cols * out_h_t / np.ceil(in_num / in_num_t)
  cout_write = out_num_t * out_h_t * out_w_t / np.ceil(in_num / in_num_t) / lane
  write_phase_latency = cout_write
  return max(load_phase_latency, compute_phase_latency, compute_drain_latency, write_phase_latency)

def relu_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, lane):
  return out_num_t * out_h_t * out_w_t / lane / np.ceil(in_num / in_num_t)

def pool_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, lane):
  return out_num_t * out_h_t * out_w_t / lane / np.ceil(in_num / in_num_t)

def inter_write_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, lane):
  return out_num_t * out_h_t * out_w_t / lane / np.ceil(in_num / in_num_t)

def cout_write_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, stride, lane, dw, port_width, fre):
  load_phase_latency = out_num_t * out_h_t * out_w_t / lane / np.ceil(in_num / in_num_t)
  burst_len = out_w_t / stride * out_num_t / (port_width / dw)
  eff_bw, eff_port_width = effective_dram_est(port_width, burst_len, fre)
  write_phase_latency = out_num_t * out_h_t / stride * out_w_t / stride / np.ceil(in_num / in_num_t) / (eff_port_width / dw)
  return max(load_phase_latency, write_phase_latency)

def layer_latency_est(params):
  in_num = params['LAYER_IN_NUM']
  out_num = params['LAYER_OUT_NUM']
  in_h = params['LAYER_IN_H']
  in_w = params['LAYER_IN_W']
  in_num_t = params['LAYER_IN_NUM_T']
  out_num_t = params['LAYER_OUT_NUM_T']
  in_h_t = params['LAYER_IN_H_T']
  in_w_t = params['LAYER_IN_W_T']
  out_h_t = params['LAYER_OUT_H_T']
  out_w_t = params['LAYER_OUT_W_T']
  filter_s1 = params['LAYER_FILTER_S1']
  filter_s2 = params['LAYER_FILTER_S2']
  lane = params['SIMD_FACTOR']
  dw0 = params['DATA_W0']
  dw1 = params['DATA_W1']
  dw2 = params['DATA_W2']
  port_width = params['BUS_W']
  depth_conv_en = params['DEPTH_CONV_EN']
  point_conv_en = params['POINT_CONV_EN']
  bias_en = params['BIAS_EN']
  max_pool_en = params['MAX_POOL_EN']
  sa_rows = params['SA_ROWS']
  sa_cols = params['SA_COLS']
  sa_lane = params['SA_SIMD_LANE']
  stride = params['LAYER_STRIDE']
  fre = params['FRE']

  cin_load_latency = cin_load_est(in_num_t, in_h_t, in_w_t, max(filter_s1, filter_s2), max(filter_s1, filter_s2), lane, dw0, port_width, fre)
  weight_load_latency = weight_load_est(in_num_t, out_num_t, filter_s1, filter_s1, filter_s2, filter_s2, lane, dw0, dw1, dw2, port_width, depth_conv_en, point_conv_en, bias_en, fre)
  inter_load_latency = inter_load_est(in_num_t, in_h_t, in_w_t, max(filter_s1, filter_s2), max(filter_s1, filter_s2), lane)
  if depth_conv_en == 1:
    depth_conv_latency = depth_conv_est(in_num_t, in_h_t, in_w_t, filter_s1, filter_s1, lane)
  else:
    depth_conv_latency = 0
  if point_conv_en == 1:
    point_conv_latency = point_conv_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, filter_s1, filter_s1, filter_s2, filter_s2, lane, sa_rows, sa_cols, sa_lane)
  else:
    point_conv_latency = 0
  relu_latency = relu_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, lane)
  if max_pool_en == 1:
    pool_latency = pool_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, lane)
  else:
    pool_latency = 0
  inter_write_latency = inter_write_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, lane)
  cout_write_latency = cout_write_est(in_num, in_num_t, out_num_t, out_h_t, out_w_t, stride, lane, dw0, port_width, fre)

#  print("latency_breakdown: ", cin_load_latency, weight_load_latency, inter_load_latency, depth_conv_latency, point_conv_latency, relu_latency, pool_latency, inter_write_latency, cout_write_latency)
  stage_latency = max(cin_load_latency, weight_load_latency, inter_load_latency, depth_conv_latency, point_conv_latency, relu_latency, pool_latency, inter_write_latency, cout_write_latency)
  total_iter = np.ceil(in_num / in_num_t) * np.ceil(out_num / out_num_t) * np.ceil(in_h / in_h_t) * np.ceil(in_w / in_w_t)
#  print(in_num, out_num, in_h, in_w, in_num_t, out_num_t, in_h_t, in_w_t)
#  print("stage latency, total iter: ", stage_latency, total_iter)
  extra_latency = max(cin_load_latency, weight_load_latency) + cout_write_latency + point_conv_latency # the data drain latency is omitted
  total_latency = extra_latency + stage_latency * total_iter

#  dep_latency = max(cin_load_latency, weight_load_latency) + max(depth_conv_latency, point_conv_latency, relu_latency, pool_latency) + cout_write_latency
#  total_latency = max(stage_latency * total_iter, dep_latency)

  return total_latency

'''
sweep each layer, pick up the optimal in_num_t/out_num_t, in_h_t, in_w_t
SA_config is a list of [SA_ROWS, SA_COLS, SA_SIMD]
'''
def model_latency_est(params, layer_configs, dynamic_tiling_level, concat_layers_list, total_layer, SA_config):

  vgg_layer_cnt = 0
  stage1_layer_cnt = 0
  stage1_iter_cnt = 0
  stage2_layer_cnt = 0
  stage2_iter_cnt = 0
  stage1_channel_cnt = 0
  stage2_channel_cnt = 0

  latency = 0
  layer_id = 0
  layer_out_num_t_prev = 0
  layer_in_num_t_list = []
  layer_out_num_t_list = []
  layer_in_h_t_list = []
  layer_in_w_t_list = []
  concat_h_t = 0
  concat_w_t = 0
  first_concat = True
  while layer_id < total_layer:
    layer_params = params.copy()
    layer_config = layer_configs[layer_id]
    prev_layer_config = layer_configs[layer_id-1]
    if (layer_id + 1 < total_layer):
      next_layer_config = layer_configs[layer_id+1]
    layer_params.update(layer_config)
    # search for better in_num_t and out_num_t
    in_num = layer_config['LAYER_IN_NUM']
    out_num = layer_config['LAYER_OUT_NUM']
    in_num_t = params['LAYER_IN_NUM_T']
    out_num_t = params['LAYER_OUT_NUM_T']
    in_h_t = params['LAYER_IN_H_T']
    in_w_t = params['LAYER_IN_W_T']
    sa_cols = params['SA_COLS']
    layer_name = layer_config['LAYER_NAME']

    if dynamic_tiling_level == 0:
      layer_in_num_t_candidates = [in_num_t]
      layer_out_num_t_candidates = [out_num_t]
    else:
      if layer_id == 0:
        layer_in_num_t_candidates = list(filter(lambda x : x % SA_config[2] == 0, range(1, in_num_t + 1)))
      elif prev_layer_config['LAYER_NAME'] in concat_layers_list:
        layer_in_num_t_candidates = [concat_num_t]
      else:
        layer_in_num_t_candidates = [layer_out_num_t_prev]

      if layer_name in concat_layers_list:
        layer_out_num_t_candidates = [concat_num_t]
      else:
        layer_out_num_t_candidates = list(filter(lambda x : x % SA_config[0] == 0, range(1, out_num_t + 1)))
        

    
    if dynamic_tiling_level == 0 or dynamic_tiling_level == 1:
      layer_in_h_t_candidates = [in_h_t]
      layer_in_w_t_candidates = [in_w_t]
    else:
      #layer_in_h_t_candidates = list(filter(lambda x : x % 2 == 0, range(1, in_h_t + 1)))
      if layer_name in concat_layers_list:
        layer_in_h_t_candidates = [concat_h_t]
        layer_in_w_t_candidates = [concat_w_t]
      else:
        layer_in_h_t_candidates = [12]
        layer_in_w_t_candidates = list(filter(lambda x : x % sa_cols == 0 and layer_config['LAYER_IN_W'] % x == 0, range(1, in_w_t + 1)))
    
    
    opt_layer_latency = np.inf
    for layer_in_num_t in layer_in_num_t_candidates:
      for layer_out_num_t in layer_out_num_t_candidates:
        for layer_in_h_t in layer_in_h_t_candidates:
          for layer_in_w_t in layer_in_w_t_candidates:
            layer_params['LAYER_IN_NUM_T'] = layer_in_num_t
            layer_params['LAYER_OUT_NUM_T'] = layer_out_num_t
            layer_params['LAYER_IN_H_T'] = layer_in_h_t
            layer_params['LAYER_IN_W_T'] = layer_in_w_t
            layer_params['LAYER_OUT_H_T'] = layer_in_h_t
            layer_params['LAYER_OUT_W_T'] = layer_in_w_t
#             print(vgg_layer_cnt, layer_in_num_t, layer_out_num_t)
            layer_latency = layer_latency_est(layer_params)
            if layer_latency < opt_layer_latency:
              opt_layer_latency = layer_latency
              opt_layer_in_num_t = layer_in_num_t
              opt_layer_out_num_t = layer_out_num_t
              opt_layer_in_h_t = layer_in_h_t
              opt_layer_in_w_t = layer_in_w_t

    #print(layer_id, opt_layer_latency, opt_layer_in_num_t, opt_layer_out_num_t, opt_layer_in_h_t, opt_layer_in_w_t)
    layer_in_num_t_list.append(opt_layer_in_num_t)
    layer_out_num_t_list.append(opt_layer_out_num_t)
    layer_in_h_t_list.append(opt_layer_in_h_t)
    layer_in_w_t_list.append(opt_layer_in_w_t)
    latency += opt_layer_latency
    layer_out_num_t_prev = opt_layer_out_num_t

    if first_concat and next_layer_config['LAYER_NAME'] in concat_layers_list:
      first_concat = False
      concat_num_t = opt_layer_out_num_t
      concat_w_t = opt_layer_in_w_t
      concat_h_t = opt_layer_in_h_t
#        print(concat_num_t)

      
    layer_id = layer_id + 1

  params['LAYER_IN_NUM_T_LIST'] = layer_in_num_t_list
  params['LAYER_OUT_NUM_T_LIST'] = layer_out_num_t_list
  params['LAYER_IN_H_T_LIST'] = layer_in_h_t_list
  params['LAYER_IN_W_T_LIST'] = layer_in_w_t_list
  in_num_t = params['LAYER_IN_NUM_T']
  out_num_t = params['LAYER_OUT_NUM_T']
  in_h_t = params['LAYER_IN_H_T']
  in_w_t = params['LAYER_IN_W_T']
  #if in_num_t == 64 and out_num_t == 64 and in_h_t == 12 and in_w_t == 96:
  #  print(latency)
  return latency, params

def BRAM_SDP_predict_HLS(dw, s):
  if dw > 18:
    alpha = np.ceil(dw / 36)
    BRAM = alpha * np.ceil(s / dw / 512)
  else:
    alpha = np.ceil(dw / 18)
    BRAM = alpha * np.ceil(s / dw / 1024)
  return BRAM

def res_est(params):
  SIMD_LANE = params['SIMD_FACTOR']
  SA_ROWS = params['SA_ROWS']
  SA_COLS = params['SA_COLS']
  SA_SIMD_LANE = params['SA_SIMD_LANE']
  LAYER_IN_NUM_T = params['LAYER_IN_NUM_T']
  LAYER_OUT_NUM_T = params['LAYER_OUT_NUM_T']
  LAYER_IN_H_T = params['LAYER_IN_H_T']
  LAYER_IN_W_T = params['LAYER_IN_W_T']
  LAYER_OUT_H_T = params['LAYER_OUT_H_T']
  LAYER_OUT_W_T = params['LAYER_OUT_W_T']
  LAYER_K_T = params['K_T']

  # estimate DSPs
  if params['DATA_T0'] == "float":
    DSP_per_MAC = 5
  elif params['DATA_T0'] == "ap_fixed<16>":
    DSP_per_MAC = 1
  # depth_conv
  depth_conv_DSP = (3 * 3 * SIMD_LANE + 1 * 1 * SIMD_LANE) * DSP_per_MAC
  # point_conv
  point_conv_DSP = SA_ROWS * SA_COLS * SA_SIMD_LANE * DSP_per_MAC
  DSP = depth_conv_DSP + point_conv_DSP

  # estimate BRAMs
  # cin_load
  cin_load_BRAM = BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W0'] * LAYER_IN_NUM_T * (LAYER_IN_H_T + LAYER_K_T - 1) * (LAYER_IN_W_T + LAYER_K_T - 1)) * 2
  # weight_load
  weight_load_BRAM = BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W1'] * LAYER_IN_NUM_T * LAYER_K_T * LAYER_K_T) + BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W1'] * LAYER_IN_NUM_T * LAYER_OUT_NUM_T * LAYER_K_T * LAYER_K_T) + BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W2'] * LAYER_OUT_NUM_T)
  # point_conv
  ROW_IL_FACTOR = LAYER_OUT_NUM_T / SA_ROWS
  COL_IL_FACTOR = LAYER_OUT_W_T / SA_COLS
  LOCAL_REG_NUM = LAYER_OUT_H_T * ROW_IL_FACTOR * COL_IL_FACTOR
  point_conv_BRAM = \
    BRAM_SDP_predict_HLS(params['DATA_W0'] * SIMD_LANE, LAYER_IN_NUM_T * (LAYER_IN_H_T + LAYER_K_T - 1) * (LAYER_IN_W_T + LAYER_K_T - 1) * params['DATA_W0']) + \
    BRAM_SDP_predict_HLS(params['DATA_W0'] * SIMD_LANE, LAYER_IN_NUM_T * (LAYER_IN_H_T + LAYER_K_T - 1) * (COL_IL_FACTOR + LAYER_K_T - 1) * params['DATA_W0']) * 2 * SA_COLS + \
    BRAM_SDP_predict_HLS(params['DATA_W1'] * SIMD_LANE, LAYER_IN_NUM_T * ROW_IL_FACTOR * LAYER_K_T * LAYER_K_T * params['DATA_W1']) * 2 * SA_ROWS + \
    BRAM_SDP_predict_HLS(params['DATA_W0'], LAYER_OUT_NUM_T * LAYER_OUT_H_T * COL_IL_FACTOR * params['DATA_W0'] / SIMD_LANE) * SIMD_LANE * 2 * SA_COLS + \
    BRAM_SDP_predict_HLS(params['DATA_W0'], LOCAL_REG_NUM * params['DATA_W0']) * 3 * SA_ROWS * SA_COLS
  # cout_write
  cout_write_BRAM = BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W0'] * LAYER_OUT_H_T * LAYER_OUT_W_T * LAYER_OUT_NUM_T) * 2

  BRAM18K = cin_load_BRAM + weight_load_BRAM + point_conv_BRAM + cout_write_BRAM

  return DSP, BRAM18K

def run(f_model, f_input_config, f_board, parallel_en, systolic_en, dynamic_tiling_level):
  print("*************************************************")
  # record start time
  global_timer_start = time.time()

  model = open(f_model, "r")
  with open(f_input_config, "r") as f:
    input_config = json.loads(f.read())
  with open(f_board, "r") as f:
    board_info = json.loads(f.read())

  config = {}
  config['BOARD'] = board_info
  config['DYNAMIC_TILING_LEVEL'] = dynamic_tiling_level
  print('Dynamic tiling level: ', dynamic_tiling_level)

  params = {}
  """
  Data Precision
  """
  params['DATA_W0'] = 32
  params['DATA_W1'] = 32
  params['DATA_W2'] = 32
  params['BUS_W'] = 512
  params['DATA_T0'] = "float"
  params['DATA_T1'] = "float"
  params['DATA_T2'] = "float"
  """
  Tiling Size
  """
  K_T = 3
  params['K_T'] = K_T


  # input info
  network_in_num = input_config["IN_NUM"]
  network_in_h = input_config["IN_H"]
  network_in_w = input_config["IN_W"]

  # get the maximal channel number throughout the network, get the layer configurations
  network_channel_max = network_in_num
  lines = []
  for i in model.readlines():
    lines.append(i)
  line_num = len(lines)

  layer_configs = []
  line_id = 1

  in_num = network_in_num
  out_num = network_in_num
  in_h = network_in_h
  in_w = network_in_w
  out_h = network_in_h
  out_w = network_in_w
  
  concat_layers = {}
  concat_channels = {}
  concat_layers_list = {}
  while line_id < len(lines):
    ## get the concat layers and their input
    line = lines[line_id].strip('\n')
    content = line.split(";")
    if content[1] == 'ConcatV2':
      chan = (content[2].strip('][')).split(', ')
      channels = [int(c) for c in chan]
      inp_layers = []
      for i in range(4, len(content)):
        inp_layers.append(content[i])
        if content[i] not in concat_layers_list:
          l = [content[0]]
          concat_layers_list[content[i]] = l
        else:
          l = concat_layers_list[content[i]]
          l.append(content[0])
          concat_layers_list[content[i]] = l
      concat_channels[content[0]] = channels
      concat_layers[content[0]] = inp_layers
      
    line_id += 1
  
  layer_channels = {}
  layer_h = {}
  layer_w = {}
  total_layer = 0
  line_id = 0
  while line_id < len(lines):
    ## extract the information of each layer
    line = lines[line_id].strip('\n')
    content = line.split(";")
    if len(content) < 2:
      line_id = line_id + 1
      continue
    if content[1] == 'ConcatV2':
      line_id = line_id + 1
      continue
    if content[1] == 'upsample':
      line_id = line_id + 1
      tensors = (content[0].strip('][')).split(', ')
      layer_name = (tensors[0]).strip("'")
      layer_channels[layer_name] = out_num
      layer_h[layer_name] = in_h
      layer_w[layer_name] = in_w
      in_h *= 2
      in_w *= 2
      out_h *= 2
      out_w *= 2
      out_num = int(content[3])
      continue
    total_layer += 1
    network_channel_max = max(network_channel_max, int(content[2]))
    
    relu_en = 0
    pool_en = 0
    bias_en = 0
    if len(content) > 1 and (content[7] == "1" or content[8] == "1"):
      relu_en = 1
    if len(content) > 1 and (content[9] == "1" or content[10] == "1"):
      bias_en = 1
    if len(content) > 1 and content[1] == "Pool":
      pool_en = 1
    tensors = (content[0].strip('][')).split(', ')
    layer_name = (tensors[0]).strip("'")
    layer_type = content[1]

    in_num = int(content[2])
    out_num = int(content[3])
    expansion_factor = int(content[4])
    filter_s = int(content[5])
    stride = int(content[6])
    in_h = out_h
    in_w = out_w

    if stride == 2:
      out_h = int(np.ceil(float(in_h) / 2))
      out_w = int(np.ceil(float(in_w) / 2))
    else:
      out_h = in_h
      out_w = in_w

    layer_channels[layer_name] = out_num
    layer_h[layer_name] = out_h
    layer_w[layer_name] = out_w
    

    if layer_type == "separable_conv":
      depth_conv_en = 1
      point_conv_en = 1
    elif layer_type == "Conv2D":
      depth_conv_en = 0
      point_conv_en = 1
    elif layer_type == "Pool":
      depth_conv_en = 0
      point_conv_en = 0

    layer_config = {}
    layer_config['LAYER_NAME'] = layer_name
    layer_config['LAYER_IN_NUM'] = in_num
    layer_config['LAYER_OUT_NUM'] = out_num
    layer_config['LAYER_IN_H'] = in_h
    layer_config['LAYER_IN_W'] = in_w
    if layer_type == 'separable_conv':
      layer_config['LAYER_FILTER_S1'] = filter_s
      layer_config['LAYER_FILTER_S2'] = 1
    elif layer_type == 'Conv2D':
      layer_config['LAYER_FILTER_S1'] = 1
      layer_config['LAYER_FILTER_S2'] = filter_s
    elif layer_type == 'Pool':
      layer_config['LAYER_FILTER_S1'] = 1
      layer_config['LAYER_FILTER_S2'] = 1
    layer_config['LAYER_STRIDE'] = stride
    layer_config['DEPTH_CONV_EN'] = depth_conv_en
    layer_config['POINT_CONV_EN'] = point_conv_en
    layer_config['BIAS_EN'] = bias_en
    layer_config['MAX_POOL_EN'] = pool_en
    layer_configs.append(layer_config)
    
    line_id = line_id + 1

  # Start the design space exploration
  # It works in a greedy fashion, as we will minimize the latency layer by layer.
  opt_latency = np.inf
  opt_DSP = np.inf
  opt_BRAM18K = np.inf
  opt_params = {}

  params_list = []
  # Construct the list of all different tiling factors
  for IN_H_T in list(filter(lambda x : network_in_h % x == 0 and x % 2 == 0, range(1, int(network_in_h / 16) + 1))): # upper_bound
    for IN_W_T in list(filter(lambda x : network_in_w % x == 0 and x % 2 == 0, range(1, 96 + 1))): # upper_bound
      for IN_NUM_T in list(filter(lambda x : x % 8 == 0, range(1, 64 + 1))): # upper_bound
        for SIMD_LANE in list(filter(lambda x : IN_NUM_T % x == 0 and x % 8 == 0, range(1, min(IN_NUM_T, 8) + 1))):
#  for IN_H_T in [12]: # upper_bound
#    for IN_W_T in [96]: # upper_bound
#      for IN_NUM_T in [64]: # upper_bound
#        for SIMD_LANE in [8]:
#          debug_cnt += 1
#          print(debug_cnt)
#          print(IN_NUM_T, IN_W_T, IN_H_T, SIMD_LANE)
          params['LAYER_IN_H_T'] = IN_H_T
          params['LAYER_IN_W_T'] = IN_W_T
          params['LAYER_OUT_H_T'] = IN_H_T
          params['LAYER_OUT_W_T'] = IN_W_T
          params['LAYER_IN_NUM_T'] = IN_NUM_T
          params['LAYER_OUT_NUM_T'] = IN_NUM_T
          params['SIMD_FACTOR'] = SIMD_LANE
          tmp_params = dict(params)
          params_list.append(tmp_params)

  num = 0
  for params_t in params_list:
    params = dict(params_t)
    IN_NUM_T = params['LAYER_IN_NUM_T']
    IN_H_T = params['LAYER_IN_H_T']
    IN_W_T = params['LAYER_IN_W_T']
    SIMD_LANE = params['SIMD_FACTOR']
#    print(IN_NUM_T, IN_W_T, SIMD_LANE)
    for SA_ROWS in list(filter(lambda x : IN_NUM_T % x == 0, range(1, IN_NUM_T + 1))):
      for SA_COLS in list(filter(lambda x : IN_W_T % x == 0, range(1, IN_W_T + 1))):
        for SA_SIMD_LANE in list(filter(lambda x : SIMD_LANE % x == 0, range(1, SIMD_LANE + 1))):
          num += 1
          
  layer_configs_pd = pd.DataFrame(layer_configs)
  layer_configs_pd.to_csv("output_layer.csv")
  
  params_list_pd = pd.DataFrame(params_list)
  params_list_pd.to_csv("params.csv")
  
  
  #return
  
  if parallel_en is True:
    num_processes = int(multiprocessing.cpu_count() * 0.75)
  else:
    num_processes = 1
  print('Parallelizing using %d processes...' % (num_processes))

  chunks = list_split(params_list, num_processes)
  pool = multiprocessing.Pool(processes = num_processes)
  results = pool.starmap(param_sweep, [(chunk, config, layer_configs, concat_layers_list, total_layer, systolic_en) for chunk in chunks])
#  result = param_sweep(params_list, config, model_config, layer_configs)

  print('Aggregating results...')
  for result in results:
    cur_latency = result['opt_latency']
    cur_DSP = result['opt_DSP']
    cur_BRAM18K = result['opt_BRAM18K']
    cur_params = result['opt_params']
    #print("update", cur_latency, opt_latency)
    if cur_latency < opt_latency:
      opt_latency = cur_latency
      opt_DSP = cur_DSP
      opt_BRAM18K = cur_BRAM18K
      opt_params = cur_params
    elif cur_latency == opt_latency:
      if cur_DSP < opt_DSP or (cur_DSP == opt_DSP and cur_BRAM18K < opt_BRAM18K):
        opt_latency = cur_latency
        opt_DSP = cur_DSP
        opt_BRAM18K = cur_BRAM18K
        opt_params = cur_params

#  opt_latency = result['opt_latency']
#  opt_DSP = result['opt_DSP']
#  opt_BRAM18K = result['opt_BRAM18K']
#  opt_params = result['opt_params']
  
  #print("finish", cur_latency, opt_latency)

# print out results
  print("*************************************************")
#  print("opt latency @(%d MHz): " % (opt_params['FRE']), opt_latency)
  opt_time = opt_latency / (opt_params['FRE'] * 1e6)
  opt_fps = 1 / opt_time
  print("opt latency (s) @%dMHz: " % (opt_params['FRE']), opt_time)
  print("opt FPS: ", opt_fps)
  opt_BRAM18K_util = opt_BRAM18K / board_info['BRAM18K'] * 100
  opt_DSP_util = opt_DSP / board_info['DSP'] * 100
  print("opt BRAM18K: %d (%d%%)" % (opt_BRAM18K, opt_BRAM18K_util))
  print("opt DSP: %d (%d%%)" % (opt_DSP, opt_DSP_util))
  with open('opt_params.json', 'w') as f:
    json.dump(opt_params, f, indent = 2)
    
  wt = opt_params["LAYER_IN_W_T_LIST"]
  ht = opt_params["LAYER_IN_H_T_LIST"]
  nt = opt_params["LAYER_IN_NUM_T_LIST"]
  mt = opt_params["LAYER_OUT_NUM_T_LIST"]
  
  layer_id = 0
  model_out = open("network_out.model", "w")
  for i in lines:
    line = i.strip('\n')
    content = line.split(";")
    if len(content) < 2:
      new_line = line + ";InNumTile;OutNumTile;HeightTile;WidthTile"
      model_out.write(new_line + '\n')
    elif content[1] == 'ConcatV2':
      model_out.write(line + '\n')
    elif content[1] == 'upsample':
      tensors = (content[0].strip('][')).split(', ')
      layer_name = (tensors[0]).strip("'")
      out_num = layer_channels[layer_name]
      in_h = layer_h[layer_name]
      in_w = layer_w[layer_name]
      new_line = line + ";" + str(out_num) + ";" + str(out_num) + ";" + str(in_h) + ";" + str(in_w) 
      model_out.write(new_line + '\n')
    else:
      new_line = line + ";" + str(nt[layer_id]) + ";" + str(mt[layer_id]) + ";" + str(ht[layer_id]) + ";" + str(wt[layer_id]) 
      model_out.write(new_line + '\n')
      layer_id += 1

  model.close()

  print("*************************************************")
  global_timer_end = time.time()
  print('Total elapsed time (s): %.3f' % (global_timer_end - global_timer_start))
  print("*************************************************")

def param_sweep(params_list, config, layer_configs, concat_layers_list, total_layer, systolic_en):
  opt_latency = np.inf
  opt_DSP = np.inf
  opt_BRAM18K = np.inf
  opt_params = {}

  for params_t in params_list:
    params = dict(params_t)
    IN_NUM_T = params['LAYER_IN_NUM_T']
    IN_H_T = params['LAYER_IN_H_T']
    IN_W_T = params['LAYER_IN_W_T']
    SIMD_LANE = params['SIMD_FACTOR']
    
    ###################################################
    ## Search through different systolic array sizes ##
    ###################################################
    # Turn it off if you want to go with a predefined systolic array size
    for SA_ROWS in (list(filter(lambda x : IN_NUM_T % x == 0, range(1, IN_NUM_T + 1))) if systolic_en else [8]):
      for SA_COLS in (list(filter(lambda x : IN_W_T % x == 0, range(1, IN_W_T + 1))) if systolic_en else [8]):
        for SA_SIMD_LANE in (list(filter(lambda x : SIMD_LANE % x == 0, range(1, SIMD_LANE + 1))) if systolic_en else [8]):
          params['LAYER_IN_H_T'] = IN_H_T
          params['LAYER_IN_W_T'] = IN_W_T
          params['LAYER_OUT_H_T'] = IN_H_T
          params['LAYER_OUT_W_T'] = IN_W_T
          params['LAYER_IN_NUM_T'] = IN_NUM_T
          params['LAYER_OUT_NUM_T'] = IN_NUM_T
          params['SIMD_LANE'] = SIMD_LANE
          params['SA_ROWS'] = SA_ROWS
          params['SA_COLS'] = SA_COLS
          params['SA_SIMD_LANE'] = SA_SIMD_LANE
          # resource estimation
          DSP, BRAM18K = res_est(params)
          # hw pruning
          if IN_W_T % SA_COLS != 0 or IN_NUM_T % SA_SIMD_LANE != 0 or IN_NUM_T % SA_ROWS != 0:
            continue
          # resource pruning
          if DSP > config['BOARD']['DSP_THRES'] * config['BOARD']['DSP']:
            continue
          if BRAM18K > config['BOARD']['BRAM18K_THRES'] * config['BOARD']['BRAM18K']:
            continue

          # frequency adjustment
          # as the resource utilization will affect the frequency, we will adjust freqeuncy here using a simple step-wise function
          if DSP / config['BOARD']['DSP'] > 0.8 or BRAM18K / config['BOARD']['BRAM18K'] > 0.8:
            params['FRE'] = 180
          else:
            params['FRE'] = 250

#          if (IN_NUM_T == 32) and (IN_W_T == 2) and (SIMD_LANE == 2):
#            if (SA_ROWS == 1) and (SA_COLS == 1) and ((SA_SIMD_LANE == 2) or (SA_SIMD_LANE == 1)):
#              print(params)

          SA_CONFIG = [SA_ROWS, SA_COLS, SA_SIMD_LANE]
          # latency estimation
          latency, params = model_latency_est(params, layer_configs, config['DYNAMIC_TILING_LEVEL'], concat_layers_list, total_layer, SA_CONFIG)

#          if (IN_NUM_T == 32) and (IN_W_T == 2) and (SIMD_LANE == 2):
#            print(latency, SA_ROWS, SA_COLS, SA_SIMD_LANE)
#            if (SA_ROWS == 1) and (SA_COLS == 1) and ((SA_SIMD_LANE == 2) or (SA_SIMD_LANE == 1)):
#              print(params)

          cur_fps = 250 * 1e6 * (1 / latency)
          opt_fps = 250 * 1e6 * (1 / opt_latency)
          
#          print(cur_fps)
          if cur_fps - opt_fps >= 0.5:
#            print("updated FPS (%.2f -> %.2f)" % (opt_fps, cur_fps))
#            if IN_NUM_T == 32 and IN_H_T == 2 and SIMD_LANE == 2:
#                print(params)
            opt_latency = latency
            opt_DSP = DSP
            opt_BRAM18K = BRAM18K
            opt_params['LAYER_IN_H_T'] = params['LAYER_IN_H_T']
            opt_params['LAYER_IN_W_T'] = params['LAYER_IN_W_T']
            opt_params['LAYER_OUT_H_T'] = params['LAYER_OUT_H_T']
            opt_params['LAYER_OUT_W_T'] = params['LAYER_OUT_W_T']
            opt_params['LAYER_IN_NUM_T'] = params['LAYER_IN_NUM_T']
            opt_params['LAYER_OUT_NUM_T'] = params['LAYER_OUT_NUM_T']
            opt_params['SIMD_LANE'] = params['SIMD_LANE']
            opt_params['SA_ROWS'] = params['SA_ROWS']
            opt_params['SA_COLS'] = params['SA_COLS']
            opt_params['SA_SIMD_LANE'] = params['SA_SIMD_LANE']
            opt_params['LAYER_IN_NUM_T_LIST'] = list(params['LAYER_IN_NUM_T_LIST'])
            opt_params['LAYER_OUT_NUM_T_LIST'] = list(params['LAYER_OUT_NUM_T_LIST'])
            opt_params['LAYER_IN_H_T_LIST'] = list(params['LAYER_IN_H_T_LIST'])
            opt_params['LAYER_IN_W_T_LIST'] = list(params['LAYER_IN_W_T_LIST'])
            opt_params['FRE'] = params['FRE']

  res = {}
  res['opt_latency'] = opt_latency
  res['opt_DSP'] = opt_DSP
  res['opt_BRAM18K'] = opt_BRAM18K
  res['opt_params'] = opt_params
  
  
  return res

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Design space exploration.')
  """
    Pass the following command line arguments or change the default value
    
      -m         : The generated file from protobuf_translation
      -i         : The name of the json file containing format of the image
      -b         : The name of the json file containing the number of resources of the target FPGA board
      --parallel : (True/False) Specify if you want to run the multi-threaded version of this code or not
      --systolic : (True/False) Specify whether you want to search for the shape of systolic array or not
      -dt        : The dynamic tiling level you want to have (0: Disabled
                                                              1: Only number of channels will be dynamic
                                                              2: All the dimensions will be dynamic)
  """
  

  parser.add_argument('-m', '--model', metavar='MODEL', default='./network.model', help='model description', dest='model')
  parser.add_argument('-i', '--input-config', metavar='INPUT_CONFIG', default='./input.json', help='input configuration', dest='input_config')
  parser.add_argument('-b', '--board', metavar='BOARD', default='./vu9p.json', help='FPGA board information', dest='board')
  parser.add_argument('--parallel', help='multi-threading parallelization', default=True, action='store_false', dest='parallel')
  parser.add_argument('--systolic', help='systolic-array-search', default=True, action='store_false', dest='systolic')
  parser.add_argument('-dt', '--dynamic-tiling', metavar='DYNAMIC_TILING', help='dynamic tiling level (0:disabled, 1:channel 2:height/width)', required=False, type=int, default=2, dest='dynamic_tiling')

  args = parser.parse_args()
  run(args.model, args.input_config, args.board, args.parallel, args.systolic, args.dynamic_tiling)
