from math import ceil
import json
import argparse
from collections import OrderedDict
from collections import defaultdict

# Instruction Layout:
# inst0: in_num_hw | out_num_hw | in_h_hw | in_w_hw | out_h_hw | out_w_hw
# inst1: in_num | out_num | in_h | in_w | out_h | out_w
# inst2: cin_offset | weight_offset | bias_offset | cout_offset | filter_s1 | filter_s2 | stride
# inst3: layer_en: conv_1st_en, depth_conv_en, conv_en, relu_en, relu6_en, pool_en, up_sample_en, bias_en, inter_load_en, inter_write_en, batch_norm_en, load_prev_cin | prev_cin_offset | in_num_t, out_num_t | in_h_t | in_w_t | nxt_layer_batch
# Batchnorm = 1 -> batch normalization
# Batchnorm = 2 -> add bias

def run(f_tile, f_model, f_input_config, s_output_tensors):

  macros = open("./params.h", "w")

  model = open(f_model, "r")
  with open(f_tile, "r") as f:
    tile = json.loads(f.read())
  with open(f_input_config, "r") as f:
    input_config = json.loads(f.read())
    
  output_tensors_list = s_output_tensors.split(', ')

  # Please change the paramters below before running this script
  """
  Tiling Size
  """
  IN_NUM_T = tile["IN_NUM_T"]
  OUT_NUM_T = tile["OUT_NUM_T"]
  IN_H_T = tile["IN_H_T"]
  IN_W_T = tile["IN_W_T"]
  OUT_H_T = tile["OUT_H_T"]
  OUT_W_T = tile["OUT_W_T"]
  K_T = tile["K_T"]
  SA_ROWS = tile["SA_ROWS"]
  SA_COLS = tile["SA_COLS"]
  SA_SIMD = tile["SA_SIMD"]


  # input info
  network_in_num = input_config["IN_NUM"]
  network_in_h = input_config["IN_H"]
  network_in_w = input_config["IN_W"]

  # Please do not change the code below
  # write out macros
  macros.write("#define IN_NUM_T " + str(IN_NUM_T) + '\n')
  macros.write("#define OUT_NUM_T " + str(OUT_NUM_T) + '\n')
  macros.write("#define OUT_W_T2 " + str(24) + '\n')
  macros.write("#define IN_H_T " + str(IN_H_T) + '\n')
  macros.write("#define IN_W_T " + str(IN_W_T) + '\n')
  macros.write("#define OUT_H_T " + str(OUT_H_T) + '\n')
  macros.write("#define OUT_W_T " + str(OUT_W_T) + '\n')
  macros.write("#define K_T " + str(K_T) + '\n')
  macros.write("#define K_T_S " + str(5) + '\n') # kernel size for smoother

  insts = open("./network.insts", "w")
  weight_load = open("./weight_offset.dat", "w")
  bias_load = open("./bias_offset.dat", "w")

  lines = []
  layer_num = 0
  for i in model.readlines():
    line = i.strip('\n')
    content = line.split(",")
    if len(content) > 1:
      if content[1] != 'ConcatV2':
        layer_num+=1
    lines.append(i)
    
  macros.write("#define LAYER_NUM " + str(layer_num) + '\n')
  line_num = len(lines)


  """
  Offset Calculation
  cin_offset <- cin_offset + IN_NUM_HW(prev_layer)*IN_H_HW(prev_layer)*IN_W_HW(prev_layer)
  weight_offset <- weight_offset + OUT_NUM_HW(prev_layer)*IN_NUM_HW(prev_layer)*FILTER_S(prev_layer)*FILTER_S(prev_layer)
  bias_offset <- bias_offset + OUT_NUM_HW(prev_layer)
  cout_offset <- cout_offset + OUT_NUM_HW(prev_layer)*OUT_H_HW(prev_layer)*OUT_W_HW(prev_layer)
  """
  cin_offset = 0
  cout_offset = 0
  weight_offset = 0
  bias_offset = 0

  region0_offset = 0
  region1_offset = 0
  region2_offset = 0
  region3_offset = 0
  region4_offset = 0


  in_num_hw = 0
  out_num_hw = 0
  in_h_hw = 0
  in_w_hw = 0
  out_h_hw = 0
  out_w_hw = 0
  filter_s = 0

  in_num = network_in_num
  out_num = network_in_num
  in_h = network_in_h
  in_w = network_in_w
  out_h = network_in_h
  out_w = network_in_w
  
  expansion_factor = 1

  in_out_offset = 0
  line_id = 1
  
  # Pass 0: To learn about concat layers
  concat_layers_inp = {}
  concat_channels = {}
  concat_layers_list = {} # maps layers to the output of concat
  while line_id < len(lines):
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
      concat_layers_inp[content[0]] = inp_layers
      
    line_id += 1

  # Pass 1: To learn about the filter size, in_out_offset, in_h_t, in_w_t
  # We need a separate pass to learn about the filter size
  filter_list = []
  in_h_t_list = []
  in_w_t_list = []
  layer_config_dict = {}
  layer_id_dict = {}
  line_id = 1
  for line_id in range(1, len(lines)):
    line = lines[line_id].strip('\n')
    content = line.split(";")
    if len(content) > 1:
      if content[1] != 'ConcatV2':
        layer_config = {}
        filter_s = int(content[5])
        filter_list.append(filter_s)
        in_h_t = int(content[14])
        in_w_t = int(content[15])
        in_h_t_list.append(in_h_t)
        in_w_t_list.append(in_w_t)
        layer_config['FILTER'] = filter_s
        layer_config['HT'] = in_h_t
        layer_config['WT'] = in_w_t
        tensors = (content[0].strip('][')).split(', ')
        layer_name = (tensors[0]).strip("'")
        layer_config_dict[layer_name] = layer_config
        layer_id_dict[layer_name] = line_id
        line_id += 1
  
  next_layers = {}
  line_id = 1
  for line_id in range(1, len(lines)):
    line = lines[line_id].strip('\n')
    content = line.split(";")
    if len(content) > 1:
      if content[1] == 'ConcatV2':
        for i in range(4, len(content)):
          layers = []
          if content[i] in next_layers:
            layers = next_layers[content[i]]
          layers.append(content[0])
          next_layers[content[i]] = layers
      else:
        layers = []
        tensors = (content[0].strip('][')).split(', ')
        layer_name = (tensors[0]).strip("'")
        prev_layer_name = (tensors[1]).strip("'")
        if prev_layer_name in next_layers:
          layers = next_layers[prev_layer_name]
        layers.append(layer_name)
        next_layers[prev_layer_name] = layers
        

  max_in_w_t = max(in_w_t_list)
  max_in_h_t = max(in_h_t_list)
  macros.write("#define MAX_IN_W_T " + str(max_in_w_t) + "\n")
  macros.write("#define MAX_IN_H_T " + str(max_in_h_t) + "\n")
  
  # Get the in_out_offset
  line_id = 1
  layer_cnt = 0
  while line_id < len(lines):
    line = lines[line_id].strip('\n')
    content = line.split(";")
    if content[1] == "Conv2D" or content[1] == "separable_conv":
      if layer_cnt == 1:
        in_out_offset = in_num_hw * in_h_hw * in_w_hw
        macros.write("#define IN_OUT_OFFSET " + str(int(in_out_offset)) + '\n')

      relu_en = 0
      relu6_en = 0;
      pool_en = 0
      norm_en = 0
      add_en = 0
      if len(content) > 1 and (content[7] == "1"):
        relu_en = 1
      if len(content) > 1 and (content[8] == "1"):
        relu6_en = 1
      if len(content) > 1 and (content[9] == "1" or content[10] == "1"):
        norm_en = 1
      if len(content) > 1 and (content[11] == "1"):
        add_en = 1

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
      in_num_t = int(content[12])
      out_num_t = int(content[13])
      in_h_t = int(content[14])
      in_w_t = int(content[15])
      

      if stride == 2:
        out_h = int(ceil(float(in_h) / 2))
        out_w = int(ceil(float(in_w) / 2))
      else:
        out_h = in_h
        out_w = in_w

      cur_filter_s = filter_s
      nxt_filter_s = 0
      nxt_filter_s = filter_list[layer_cnt + 1]
      out_h_t = in_h_t_list[layer_cnt + 1]
      out_w_t = in_w_t_list[layer_cnt + 1]

      in_num_hw = int(ceil(float(in_num) / in_num_t) * in_num_t)
      out_num_hw = int(ceil(float(out_num) / out_num_t) * out_num_t)
      in_h_hw = int(ceil(float(in_h) / in_h_t) * in_h_t + (cur_filter_s - 1))
      in_w_hw = int(ceil(float(in_w) / in_w_t) * in_w_t + (cur_filter_s - 1))
      out_h_hw = int(ceil(float(out_h) / out_h_t) * out_h_t + (nxt_filter_s - 1))
      out_w_hw = int(ceil(float(out_w) / out_w_t) * out_w_t + (nxt_filter_s - 1))


      layer_cnt = layer_cnt + 1
      if layer_cnt == 2:
        break
    line_id = line_id + 1

#  print(in_out_offset)

  # reinitialize all parameters
  layer_cnt = 0

  in_num_hw = 0
  out_num_hw = 0
  in_h_hw = 0
  in_w_hw = 0
  out_h_hw = 0
  out_w_hw = 0
  filter_s = 0

  in_num = network_in_num
  out_num = network_in_num
  in_h = network_in_h
  in_w = network_in_w
  out_h = network_in_h
  out_w = network_in_w
  
  expansion_factor = 1

  # Pass 2: To learn about layer output size
  layer_cin_size = {}
  layer_cin_size_hw = {}
  layer_cout_size = {}
  layer_cout_size_hw = {}
  layer_cout_size_hw_concat = {}
  layer_weight_size = {}
  layer_weight_size_hw = {}
  layer_bias_size = {}
  layer_bias_size_hw = {}
      

  max_layer_batch = 1
  layer_configs = OrderedDict()

  #for line_id in range(0,len(lines)):
  line_id = 1
  while line_id < len(lines):
    line = lines[line_id].strip('\n')
    content = line.split(";")
    if len(content) > 2 and content[1] != 'ConcatV2':
      pool_en = 0
      up_sample_en = 0
      if len(content) > 1 and content[1] == "Pool":
        pool_en = 1
      if len(content) > 1 and content[1] == "upsample":
        up_sample_en = 1
            
      relu_en = 0
      relu6_en = 0;
      pool_en = 0
      norm_en = 0
      add_en = 0
      load_prev_cin = 0
      bias_en = 0
      if len(content) > 1 and (content[7] == "1"):
        relu_en = 1
      if len(content) > 1 and (content[8] == "1"):
        relu6_en = 1
      if len(content) > 1 and (content[9] == "1"):
        norm_en = 1
      if len(content) > 1 and (content[10] == "1"):
        bias_en = 1
      if len(content) > 1 and (content[11] == "1"):
        add_en = 1
  
      tensors = (content[0].strip('][')).split(', ')
      layer_name = (tensors[0]).strip("'")
      prev_layer_name = (tensors[1]).strip("'")
      layer_type = content[1]
  
      in_num = int(content[2])
      out_num = int(content[3])
      expansion_factor = int(content[4])
      filter_s = int(content[5])
      stride = int(content[6])
      in_h = out_h
      in_w = out_w
      in_num_t = int(content[12])
      out_num_t = int(content[13])
      in_h_t = int(content[14])
      in_w_t = int(content[15])
      
  
      if stride == 2:
        out_h = int(ceil(float(in_h) / 2))
        out_w = int(ceil(float(in_w) / 2))
      else:
        out_h = in_h
        out_w = in_w
        
      if layer_type == "upsample":
        out_h = in_h * 2
        out_w = in_w * 2
  
      cur_filter_s = 1
      if prev_layer_name in concat_layers_list:
        f_list = [max((layer_config_dict[next])['FILTER'] for next in next_layers[layer] if next in layer_config_dict) for layer in concat_layers_list[prev_layer_name] if layer in next_layers]
        cur_filter_s = max(max(f_list), filter_s)
      else:
        cur_filter_s = filter_s
        
      if prev_layer_name in concat_layers_inp:
        in_num_list = [(layer_config_dict[layer])['OUT_NUM_HW'] for layer in concat_layers_inp[prev_layer_name] if layer in layer_config_dict]
        in_num = sum(in_num_list)
        in_h_list = [(layer_config_dict[layer])['OUT_H'] for layer in concat_layers_inp[prev_layer_name] if layer in layer_config_dict]
        in_h = max(in_h_list)
        in_w_list = [(layer_config_dict[layer])['OUT_W'] for layer in concat_layers_inp[prev_layer_name] if layer in layer_config_dict]
        in_w = max(in_w_list)
        layer_config = {}
        if prev_layer_name not in layer_config_dict:
          layer_config['OUT_NUM_HW'] = in_num
          layer_config['OUT_H'] = in_h
          layer_config['OUT_W'] = in_w
          layer_config_dict[prev_layer_name] = layer_config 
        
      nxt_filter_s = 1
      out_h_t = in_h_t
      out_w_t = in_w_t
      if layer_name in concat_layers_list:
        f_list = [max((layer_config_dict[next])['FILTER'] for next in next_layers[layer] if next in layer_config_dict) for layer in concat_layers_list[layer_name] if layer in next_layers]
        if len(f_list) > 0: nxt_filter_s = max(f_list)
        ht_list = [max((layer_config_dict[next])['HT'] for next in next_layers[layer] if next in layer_config_dict) for layer in concat_layers_list[layer_name] if layer in next_layers]
        if len(ht_list) > 0: out_h_t = max(ht_list)
        wt_list = [max((layer_config_dict[next])['WT'] for next in next_layers[layer] if next in layer_config_dict) for layer in concat_layers_list[layer_name] if layer in next_layers]
        if len(wt_list) > 0: out_w_t = max(wt_list)
      else:
        if (layer_cnt + 1) < len(filter_list): nxt_filter_s = filter_list[layer_cnt + 1]
        if (layer_cnt + 1) < len(in_h_t_list): out_h_t = in_h_t_list[layer_cnt + 1]
        if (layer_cnt + 1) < len(in_w_t_list): out_w_t = in_w_t_list[layer_cnt + 1]
  
      in_num_hw = int(ceil(float(in_num) / in_num_t) * in_num_t)
      out_num_hw = int(ceil(float(out_num) / out_num_t) * out_num_t)
      in_h_hw = int(ceil(float(in_h) / in_h_t) * in_h_t + (cur_filter_s - 1))
      in_w_hw = int(ceil(float(in_w) / in_w_t) * in_w_t + (cur_filter_s - 1))
      out_h_hw = int(ceil(float(out_h) / out_h_t) * out_h_t + (nxt_filter_s - 1))
      out_w_hw = int(ceil(float(out_w) / out_w_t) * out_w_t + (nxt_filter_s - 1))
  
      task_num1 = int(ceil(float(in_num) / in_num_t) * ceil(float(out_num) / out_num_t) * ceil(float(in_h) / in_h_t) * ceil(float(in_w) / in_w_t))
      task_num2 = int(ceil(float(out_num) / out_num_t) * ceil(float(in_h) / in_h_t) * ceil(float(in_w) / in_w_t))
      if layer_type == "separable_conv":
        local_accum_num = int(in_num_t / SA_SIMD * 1 * 1)
      else:
        local_accum_num = int(in_num_t / SA_SIMD * filter_s * filter_s)
      local_reg_num = int((in_h_t / stride) * (in_w_t / SA_COLS / stride) * (out_num_t / SA_ROWS))
      row_il_factor = int(out_num_t / SA_ROWS)
      col_il_factor = int(in_w_t / SA_COLS / stride)
      
      if layer_name in concat_layers_list:
        layer_config = {}
        if layer_name in layer_config_dict:
          layer_config = layer_config_dict[layer_name]
        layer_config['OUT_H_HW'] = out_h_hw
        layer_config['OUT_W_HW'] = out_w_hw
        layer_config['OUT_H'] = out_h
        layer_config['OUT_W'] = out_w
        layer_config['OUT_NUM_HW'] = out_num_hw
        layer_config['OUT_NUM'] = out_num
        layer_config_dict[layer_name] = layer_config
  
  
      # calculate cin, cout, weight, bias size
      cin_size = in_num * in_h * in_w
      cin_size_hw = in_num_hw * in_h_hw * in_w_hw
  
      cout_size = out_num * out_h * out_w
      cout_size_hw = out_num_hw * out_h_hw * out_w_hw
      cout_size_hw_concat = out_num * out_h_hw * out_w_hw
  
      depth_norm_en = 0
      
      if layer_type == "separable_conv":
        weight_size = in_num * filter_s * filter_s + in_num * out_num * 1 * 1
        weight_size_hw = in_num_hw * filter_s * filter_s + in_num_hw *out_num_hw * 1 * 1
        bias_size = (out_num + in_num)
        bias_size_hw = (out_num_hw + in_num_hw)
        if norm_en:
          bias_size *= 2
          bias_size_hw *= 2
      elif layer_type == "Conv2D":
        weight_size = in_num * out_num * filter_s * filter_s
        weight_size_hw = in_num_hw * out_num_hw * filter_s * filter_s
        bias_size = out_num
        bias_size_hw = out_num_hw
        if norm_en:
          bias_size *= 2
          bias_size_hw *= 2
      else: # maxpool and upsample
        weight_size = 0
        weight_size_hw = 0
        bias_size = 0
        bias_size_hw = 0
  
      weight_load_size = weight_size_hw
      bias_load_size = bias_size_hw
      weight_load.write(str(weight_offset) + " " + str(weight_load_size) + '\n')
      bias_load.write(str(bias_offset) + " " + str(bias_load_size) + '\n')
  
      weight_offset = weight_offset + weight_load_size
      bias_offset = bias_offset + bias_load_size
  
      if layer_cnt == 0:
        macros.write("#define LAYER1_IN_NUM " + str(in_num) + '\n')
        macros.write("#define LAYER1_OUT_NUM " + str(out_num) + '\n')
        macros.write("#define LAYER1_IN_NUM_T " + str(in_num_t) + '\n')
        macros.write("#define LAYER1_OUT_NUM_T " + str(out_num_t) + '\n')
        macros.write("#define LAYER1_IN_H " + str(in_h) + '\n')
        macros.write("#define LAYER1_IN_W " + str(in_w) + '\n')
        macros.write("#define LAYER1_OUT_H " + str(out_h) + '\n')
        macros.write("#define LAYER1_OUT_W " + str(out_w) + '\n')
        macros.write("#define LAYER1_IN_NUM_HW " + str(int(in_num_hw)) + '\n')
        macros.write("#define LAYER1_OUT_NUM_HW " + str(int(out_num_hw)) + '\n')
        macros.write("#define LAYER1_IN_H_HW " + str(int(in_h_hw)) + '\n')
        macros.write("#define LAYER1_IN_W_HW " + str(int(in_w_hw)) + '\n')
        macros.write("#define LAYER1_OUT_H_HW " + str(int(out_h_hw)) + '\n')
        macros.write("#define LAYER1_OUT_W_HW " + str(int(out_w_hw)) + '\n')
  #        macros.write("#define LAYER1_K " + str(filter_s) + '\n')
        macros.write("#define LAYER1_K " + str(nxt_filter_s) + '\n')
        macros.write("#define LAYER1_POOL " + str(pool_en) + '\n')
  
      layer_cin_size[layer_name] = cin_size
      layer_cin_size_hw[layer_name] = cin_size_hw
  
      layer_cout_size[layer_name] = cout_size
      layer_cout_size_hw[layer_name] = cout_size_hw
      layer_cout_size_hw_concat[layer_name] = cout_size_hw_concat
  
      layer_weight_size[layer_name] = weight_size
      layer_weight_size_hw[layer_name] = weight_size_hw
  
      layer_bias_size[layer_name] = bias_size
      layer_bias_size_hw[layer_name] = bias_size_hw
  
      layer_config = {}
      layer_config['IN_NUM'] = in_num
      layer_config['OUT_NUM'] = out_num
      layer_config['IN_H'] = in_h
      layer_config['IN_W'] = in_w
      layer_config['OUT_H'] = out_h
      layer_config['OUT_W'] = out_w
      layer_config['IN_NUM_HW'] = in_num_hw
      layer_config['OUT_NUM_HW'] = out_num_hw
      layer_config['IN_H_HW'] = in_h_hw
      layer_config['IN_W_HW'] = in_w_hw
      layer_config['OUT_H_HW'] = out_h_hw
      layer_config['OUT_W_HW'] = out_w_hw
      if layer_type == "separable_conv":
        layer_config['FILTER_S1'] = filter_s
        layer_config['FILTER_S2'] = 1
      elif layer_type == "Conv2D":
        layer_config['FILTER_S1'] = 1
        layer_config['FILTER_S2'] = filter_s
      else:
        layer_config['FILTER_S1'] = 1
        layer_config['FILTER_S2'] = 1
      layer_config['STRIDE'] = stride
      layer_config['IN_NUM_T'] = in_num_t
      layer_config['OUT_NUM_T'] = out_num_t
      layer_config['IN_H_T'] = in_h_t
      layer_config['IN_W_T'] = in_w_t
      layer_config['NXT_LAYER_BATCH'] = 1
      layer_config['TASK_NUM1'] = task_num1
      layer_config['TASK_NUM2'] = task_num2
      layer_config['LOCAL_ACCUM_NUM'] = local_accum_num
      layer_config['LOCAL_REG_NUM'] = local_reg_num
      layer_config['ROW_IL_FACTOR'] = row_il_factor
      layer_config['COL_IL_FACTOR'] = col_il_factor
  
      batch_norm_depth = 0
      conv_1st_en = 0
      if layer_type == "separable_conv":
        depth_conv_en = 1
        depth_norm_en = 1
        conv_en = 1
        up_sample_en = 0
        if norm_en == 1:
          batch_norm_depth = 1
      elif layer_type == "Conv2D":
        depth_conv_en = 0
        conv_en = 1
        up_sample_en = 0
      elif layer_type == "Pool":
        depth_conv_en = 0
        conv_en = 0
        up_sample_en = 0
      if layer_type == "upsample":
        depth_conv_en = 0
        conv_en = 0
        conv_1st_en = 0
        up_sample_en = 1
  
      inter_load_en = 0
      inter_write_en = 0
      load_prev_cin = add_en
  
      layer_en = conv_1st_en + (depth_conv_en << 1) + (conv_en << 2) + (relu_en << 3) + (relu6_en << 4) + (pool_en << 5) + (up_sample_en << 6) + (bias_en << 7) + (inter_load_en << 8) + (inter_write_en << 9) + (norm_en << 10) + (load_prev_cin << 11) + (depth_norm_en << 12)
        
  
      layer_config['LAYER_EN'] = layer_en
      layer_configs[layer_name] = layer_config
      
      if layer_name == output_tensors_list[0]:
        macros.write("#define STAGE2L_OUT_NUM " + str(out_num) + '\n')
        macros.write("#define STAGE2L_OUT_NUM_T " + str(out_num_t) + '\n')
        macros.write("#define STAGE2L_OUT_H " + str(out_h) + '\n')
        macros.write("#define STAGE2L_OUT_W " + str(out_w) + '\n')
        macros.write("#define STAGE2L_OUT_NUM_HW " + str(int(out_num_hw)) + '\n')
        macros.write("#define STAGE2L_OUT_H_HW " + str(int(out_h_hw)) + '\n')
        macros.write("#define STAGE2L_OUT_W_HW " + str(int(out_w_hw)) + '\n')
        macros.write("#define STAGE2L_K " + str(int(nxt_filter_s)) + '\n')

      elif layer_name == output_tensors_list[1]:
        macros.write("#define STAGE2R_OUT_NUM " + str(out_num) + '\n')
        macros.write("#define STAGE2R_OUT_NUM_T " + str(out_num_t) + '\n')
        macros.write("#define STAGE2R_OUT_H " + str(out_h) + '\n')
        macros.write("#define STAGE2R_OUT_W " + str(out_w) + '\n')
        macros.write("#define STAGE2R_OUT_NUM_HW " + str(int(out_num_hw)) + '\n')
        macros.write("#define STAGE2R_OUT_H_HW " + str(int(out_h_hw)) + '\n')
        macros.write("#define STAGE2R_OUT_W_HW " + str(int(out_w_hw)) + '\n')
        macros.write("#define STAGE2R_K " + str(int(nxt_filter_s)) + '\n')
  
      layer_cnt = layer_cnt + 1
      
    

    line_id = line_id + 1
    
  #for layer in layer_configs:
   # print(layer, ":", layer_configs[layer])
    
  regions = OrderedDict()
  regions_normal = OrderedDict()
  regions_concat = OrderedDict()
  regions_concat_reorg = OrderedDict()
  region_id_normal = 0
  region_id_concat = 0
  prev_layer = None
  tmp_concat_layers = []
  ind = 0
  for layer in layer_configs:
    if layer not in concat_layers_list: 
      region_layers = []
      if regions_normal.get(region_id_normal) != None:
        region_layers = regions_normal[region_id_normal]
      region_layers.append(layer)
      regions_normal[region_id_normal] = region_layers
    else:
      if prev_layer not in concat_layers_list:
        region_id_normal += 1
      if layer not in tmp_concat_layers:
        tmp_concat_layers.append(layer)
        cnct_out = (concat_layers_list[layer])[0]
        region_layers = []
        for inp in concat_layers_inp[cnct_out]:
          if inp not in tmp_concat_layers:
            tmp_concat_layers.append(inp)
          if inp in concat_layers_inp:
            for inp2 in concat_layers_inp[inp]:
              if inp2 not in tmp_concat_layers:
                tmp_concat_layers.append(inp2)
              region_layers.append(inp2)
          else:
           region_layers.append(inp)
        regions_concat[region_id_concat] = [l_name for _, l_name in sorted(zip([layer_id_dict[id_l] for id_l in region_layers], region_layers))]
        region_id_concat += 1
    prev_layer = layer
  
  round_robin_id = 0
  id_concat = region_id_concat
  region_id_concat = 0
  add_both_round_robin = False
  mapped_layers = {}
  mapped_layers_reverse = defaultdict(list)
  round_robin_ind = [0,0]
  first_layer_region = {}
  mapped_region = {}
  
  for i in concat_layers_inp:
    mapped_region[concat_layers_inp[i][0]] = 0
    if len(concat_layers_inp[i]) > 1:
      mapped_region[concat_layers_inp[i][1]] = 0
  for c_id, c_list in regions_concat.items():
    add = True
    for c2_id, c2_list in regions_concat.items():
      if c2_id > c_id:
        if set(c_list).issubset(set(c2_list)): # if the layers are going to be added, skip them
          add = False
          break
    
    if add:
      for c2_id, c2_list in regions_concat_reorg.items():
        if set(c_list) & set(c2_list): # if the lists have tensors in common, merge them
          if len(c_list) != len(c2_list):
            print("******************************")
            print("Error! This type of concatenation is not supported. Add your concatenation behavior to the instruction generator.")
            print("Checkout the paper for the supported concatenation behavior. The supported way is either the concat layers don't have intersection or concat the same number of layers")
            print("Error occured at", c_list, "and", c2_list)
            print("******************************")
          else:
            round_robin_id = (round_robin_id + 1) % 2
            if not add_both_round_robin and round_robin_id == 1:
              round_robin_ind[0] = region_id_concat - 1
              common = [x for x in c_list if x in c2_list]
              #print(common)
              for l in c_list:
                if l not in common:
                  first_layer_region[l] = common[0]
              non_common = [x for x in c_list if x not in c2_list]
              regions_concat_reorg[region_id_concat] = non_common
              for layer_ in non_common:
                mapped_region[layer_] = round_robin_id
              #print(non_common)
              round_robin_ind[1] = region_id_concat
              region_id_concat += 1
              add_both_round_robin = True
             
            layers = regions_concat_reorg[round_robin_ind[round_robin_id]]
            #print(c_id, c_list)
            #print(round_robin_ind[round_robin_id], round_robin_id, layers)
            non_common1 = [x for x in c_list if x not in layers]
            non_common1 = [x for x in non_common1 if x not in regions_concat_reorg[round_robin_ind[0]]]
            non_common2 = [x for x in layers if x not in c_list]
            
            if non_common2 != []:
              for ind, layer_name in enumerate(non_common1):
                if layer_cout_size_hw[layer_name] == layer_cout_size_hw[non_common2[ind]]:
                  mapped_layers[layer_name] = non_common2[ind]
                  mapped_layers_reverse[non_common2[ind]].append(layer_name)
                  
                else: 
                  print("******************************")
                  print("Error! This type of concatenation is not supported. Add your concatenation behavior to the instruction generator.")
                  print("In the current configuration of the round robin format layers should have the same size. For more info checkout the paper.")
                  print("Error occured at", layer_name, "and", layers[ind])
                  print("******************************")
                  
              
            add = False
            
            break
            
    if add and c_id == len(regions_concat)-1:
      layers = regions_concat_reorg[round_robin_ind[1]]
      non_common1 = [x for x in c_list if x not in layers]
      non_common1 = sorted([x for x in non_common1 if x not in regions_concat_reorg[round_robin_ind[0]]])
      non_common2 = sorted([x for x in layers if x not in c_list])
      
      if non_common2 != []:
        for ind, layer_name in enumerate(non_common1):
          if layer_cout_size_hw[layer_name] <= layer_cout_size_hw[non_common2[ind]]:
            mapped_layers[layer_name] = non_common2[ind]
          
        if len(non_common2) > len(non_common1):
          new_region = [non_common2[i] for i in range(len(non_common1), len(non_common2))]
          regions_concat_reorg[region_id_concat] = new_region
          region_id_concat += 1
            
      add = False
      
    if add:
      regions_concat_reorg[region_id_concat] = c_list
      region_id_concat += 1
          
   
  for layer_ in mapped_layers:
    mapped_region[layer_] = mapped_region[mapped_layers[layer_]]
   
    
#  for r in regions_concat_reorg:
#    print(r, ":", regions_concat_reorg[r]) 
#    
#  for i, r in mapped_layers.items():
#    print(i, r)

  
  ind_region = 0 
  for ind in regions_normal:
    regions[ind_region] = regions_normal[ind]
    ind_region += 1
  
  for ind in reversed(regions_concat_reorg):
    regions[ind_region] = regions_concat_reorg[ind]
    ind_region += 1

#  for r in regions:
#    print(r, ":", regions[r])

  regions_size = []
  regions_offset = []
  cin_size = 0
  offset = 0
  size = 0
  for ind, r in regions.items():
    offset += size
    size = 0
    for layer in r:
      layer_config = layer_configs[layer]
      layer_config['REGION'] = ind
      layer_configs[layer] = layer_config
      size += layer_cout_size_hw[layer]
      
    cin_size += size
    regions_size.append(size)
    regions_offset.append(offset)

  weight_size = weight_offset
  bias_size = bias_offset

  macros.write("#define CIN_SIZE " + str(int(cin_size)) + '\n')
  macros.write("#define WEIGHT_SIZE " + str(int(weight_size)) + '\n')
  macros.write("#define BIAS_SIZE " + str(int(bias_size)) + '\n')

  # Pass3: To generate offsets
  layer_output_size = []
  layer_output_size_hw = []

  # reinitialize all parameters
  line_id = 1
  cin_offset = 0
  cout_offset = 0
  weight_offset = 0
  bias_offset = 0
  prev_cin_offset = 0

  in_num_hw = 0
  out_num_hw = 0
  in_h_hw = 0
  in_w_hw = 0
  out_h_hw = 0
  out_w_hw = 0
  filter_s = 0

  in_num = network_in_num
  out_num = network_in_num
  in_h = network_in_h
  in_w = network_in_w
  out_h = network_in_h
  out_w = network_in_w
  


  prev_cin_offset = 0
  layer_cnt = 0
  while line_id < len(lines):
    line = lines[line_id].strip('\n')
    content = line.split(";")
    if len(content) > 2 and content[1] != 'ConcatV2':
      tensors = (content[0].strip('][')).split(', ')
      layer_name = (tensors[0]).strip("'")
      prev_layer_name = (tensors[1]).strip("'")
      layer_type = content[1]
      mapped_layer_name = layer_name
      if layer_name in mapped_layers:
        mapped_layer_name = mapped_layers[layer_name]
#        layer_cnt = layer_cnt + 1
#        line_id = line_id + 1
#        continue
      cur_filter_s = 1
      if prev_layer_name in concat_layers_list:
        f_list = [max((layer_config_dict[next])['FILTER'] for next in next_layers[layer] if next in layer_config_dict and 'FILTER' in layer_config_dict[next]) for layer in concat_layers_list[prev_layer_name] if layer in next_layers]
        cur_filter_s = max(max(f_list), filter_s)
      else:
        cur_filter_s = filter_s
        
      if prev_layer_name in concat_layers_inp:
        in_num_list = [(layer_config_dict[layer])['OUT_NUM_HW'] for layer in concat_layers_inp[prev_layer_name] if layer in layer_config_dict]
        in_num = sum(in_num_list)
        in_h_list = [(layer_config_dict[layer])['OUT_H'] for layer in concat_layers_inp[prev_layer_name] if layer in layer_config_dict]
        in_h = max(in_h_list)
        in_w_list = [(layer_config_dict[layer])['OUT_W'] for layer in concat_layers_inp[prev_layer_name] if layer in layer_config_dict]
        in_w = max(in_w_list)
        layer_config = {}
        if prev_layer_name not in layer_config_dict:
          layer_config['OUT_NUM_HW'] = in_num
          layer_config['OUT_H'] = in_h
          layer_config['OUT_W'] = in_w
          layer_config_dict[prev_layer_name] = layer_config 
        
      nxt_filter_s = 1
      out_h_t = in_h_t
      out_w_t = in_w_t
      if layer_name in concat_layers_list:
        f_list = [max((layer_config_dict[next])['FILTER'] for next in next_layers[layer] if next in layer_config_dict and 'FILTER' in layer_config_dict[next]) for layer in concat_layers_list[layer_name] if layer in next_layers]
        if len(f_list) > 0: nxt_filter_s = max(f_list)
      else:
        if (layer_cnt + 1) < len(filter_list): nxt_filter_s = filter_list[layer_cnt + 1]
      
      layer_region = layer_configs[mapped_layer_name]['REGION']
      cout_offset = regions_offset[layer_region]
      for layer in regions[layer_region]:  
#        if layer_name == "MobilenetV2/expanded_conv_5/add":
#          print(layer_region, layer)
        if layer == layer_name:
          break
        cout_offset += layer_cout_size_hw[layer]
      
      shifted_cout_offset = cout_offset + layer_configs[layer_name]['OUT_NUM_T'] * layer_configs[layer_name]['OUT_W_HW'] * int(nxt_filter_s / 2) + layer_configs[layer_name]['OUT_NUM_T'] * int(nxt_filter_s / 2) + in_out_offset
      
      
      if layer_cnt == 0:
        cin_offset = 0
      else:
        if prev_layer_name in layer_configs or layer_name in mapped_layers:
          if 'COUT_OFFSET' in layer_configs[prev_layer_name]:
            #cin_offset = layer_configs[prev_layer_name]['SHIFTED_COUT_OFFSET']
            cin_offset = layer_configs[prev_layer_name]['COUT_OFFSET'] + in_out_offset
            #if layer_name == 'Openpose/MConv_Stage3_L1_1_pointwise/Relu':
            #  print(layer_name, prev_layer_name, cin_offset, layer_configs[layer_name]['IN_NUM_HW'], layer_configs[layer_name]['IN_H_HW'], layer_configs[layer_name]['IN_W_HW'])
        else: # previous layer is a concatenation layer  
          input_layers_list = []
          for l in concat_layers_inp[prev_layer_name]:
            if l not in concat_layers_inp:
              input_layers_list.append(l)
            else: 
               for l_inside in concat_layers_inp[l]:
                input_layers_list.append(l_inside)
          
          if mapped_region[concat_layers_inp[prev_layer_name][0]] == 0:
            sorted_prev_layers = [l_name for _, l_name in sorted(zip([layer_id_dict[id_l] for id_l in input_layers_list], input_layers_list))]
            layer_name_1st_inp = (sorted_prev_layers)[0]
          else: 
            layer_name_1st_inp = concat_layers_inp[prev_layer_name][0]
          
          if layer_name_1st_inp in mapped_layers:
            layer_name_1st_inp = mapped_layers[layer_name_1st_inp]
          layer_region = layer_configs[layer_name_1st_inp]['REGION']
          if layer_name_1st_inp in first_layer_region:
            first_l = first_layer_region[layer_name_1st_inp]
            #cin_offset = regions_offset[layer_region-1] + layer_configs[first_l]['OUT_NUM_T'] * layer_configs[first_l]['OUT_W_HW'] * int(cur_filter_s / 2) + layer_configs[first_l]['OUT_NUM_T'] * int(cur_filter_s / 2) + in_out_offset
            cin_offset = regions_offset[layer_region-1] + in_out_offset
            for layer in regions[layer_region-1]:  
              if layer == first_l:
                break
              cin_offset += layer_cout_size_hw[layer]
          else:
            #cin_offset = regions_offset[layer_region] + layer_configs[layer_name_1st_inp]['OUT_NUM_T'] * layer_configs[layer_name_1st_inp]['OUT_W_HW'] * int(cur_filter_s / 2) + layer_configs[layer_name_1st_inp]['OUT_NUM_T'] * int(cur_filter_s / 2) + in_out_offset
            cin_offset = regions_offset[layer_region] + in_out_offset
          for layer in regions[layer_region]:  
            if layer == layer_name_1st_inp:
              break
            cin_offset += layer_cout_size_hw[layer] 
      
        
          
      if layer_name not in mapped_layers:
        layer_configs[layer_name]['SHIFTED_COUT_OFFSET'] = shifted_cout_offset
        layer_configs[layer_name]['COUT_OFFSET'] = cout_offset
      
      layer_configs[layer_name]['CIN_OFFSET'] = cin_offset
      layer_configs[layer_name]['WEIGHT_OFFSET'] = weight_offset
      layer_configs[layer_name]['BIAS_OFFSET'] = bias_offset
      layer_configs[layer_name]['PREV_CIN_OFFSET'] = prev_cin_offset
  
      prev_cin_offset = cin_offset
      #cin_offset += layer_cin_size_hw[layer_name]
      weight_offset += layer_weight_size_hw[layer_name]
      bias_offset += layer_bias_size_hw[layer_name]
      #cout_offset += layer_cout_size_hw[layer_name]
      
  
      layer_cnt = layer_cnt + 1
    line_id = line_id + 1
      


  macros.write("#define MAX_LAYER_BATCH " + str(int(max_layer_batch)) + '\n')
  # Pass4: To print out insts
  # reinitialize all parameters
  line_id = 1

  while line_id < len(lines):
    line = lines[line_id].strip('\n')
    content = line.split(";")
    if len(content) > 2 and content[1] != 'ConcatV2':
      tensors = (content[0].strip('][')).split(', ')
      layer_name = (tensors[0]).strip("'")
      prev_layer_name = (tensors[1]).strip("'")
      actual_layer_name = layer_name
      mapped_layer_name = layer_name
      
      if layer_name in mapped_layers:
        #print(layer_name, mapped_layers[layer_name])
        mapped_layer_name = mapped_layers[layer_name]
      
      inst0 = [layer_configs[layer_name]['IN_NUM_HW'], layer_configs[layer_name]['OUT_NUM_HW'], layer_configs[layer_name]['IN_H_HW'], layer_configs[layer_name]['IN_W_HW'], layer_configs[layer_name]['OUT_H_HW'], layer_configs[layer_name]['OUT_W_HW']]
      inst1 = [layer_configs[layer_name]['IN_NUM'], layer_configs[layer_name]['OUT_NUM'], layer_configs[layer_name]['IN_H'], layer_configs[layer_name]['IN_W'], layer_configs[layer_name]['OUT_H'], layer_configs[layer_name]['OUT_W']]
      inst2 = [layer_configs[layer_name]['CIN_OFFSET'], layer_configs[layer_name]['WEIGHT_OFFSET'], layer_configs[layer_name]['BIAS_OFFSET'], layer_configs[mapped_layer_name]['SHIFTED_COUT_OFFSET'], layer_configs[layer_name]['FILTER_S1'], layer_configs[layer_name]['FILTER_S2'], layer_configs[layer_name]['STRIDE']]
      inst3 = [layer_configs[layer_name]['LAYER_EN'], layer_configs[layer_name]['PREV_CIN_OFFSET'], layer_configs[layer_name]['IN_NUM_T'], layer_configs[layer_name]['OUT_NUM_T'], layer_configs[layer_name]['IN_H_T'], layer_configs[layer_name]['IN_W_T'], layer_configs[layer_name]['NXT_LAYER_BATCH']]
      inst4 = [layer_configs[layer_name]['TASK_NUM1'], layer_configs[layer_name]['TASK_NUM2'], layer_configs[layer_name]['LOCAL_ACCUM_NUM'], layer_configs[layer_name]['LOCAL_REG_NUM'], layer_configs[layer_name]['ROW_IL_FACTOR'], layer_configs[layer_name]['COL_IL_FACTOR']]
      
      if "Stage6_L1_5" in actual_layer_name:
        macros.write("#define STAGE2R_OFFSET " + str(int(layer_configs[mapped_layer_name]['COUT_OFFSET'] + in_out_offset)) + '\n')
        
      if "Stage6_L2_5" in actual_layer_name:
        macros.write("#define STAGE2L_OFFSET " + str(int(layer_configs[mapped_layer_name]['COUT_OFFSET'] + in_out_offset)) + '\n')
        
  
      insts.writelines(" ".join(str(int(e)) for e in inst0) + "\n")
      insts.writelines(" ".join(str(int(e)) for e in inst1) + "\n")
      insts.writelines(" ".join(str(int(e)) for e in inst2) + "\n")
      insts.writelines(" ".join(str(int(e)) for e in inst3) + "\n")
      insts.writelines(" ".join(str(int(e)) for e in inst4) + "\n")
      insts.writelines("\n")

    line_id = line_id + 1
    

  model.close()
  insts.close()

  macros.close()
  weight_load.close()
  bias_load.close()

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Data reorganization.')
  """
    Pass the following command line arguments or change the default value
    
      -t : The name of the json file containing the maximum tiling factors and the systolic array size
      -m : The generated file from DSE
      -i : The name of the json file containing format of the image
      -o : The name of the output tensors
  """

  parser.add_argument('-t', '--tile', metavar='TILE', default='./tile.json', help='tiling configuration', dest='tile')
  parser.add_argument('-m', '--model', metavar='MODEL', default='./network_out.model', help='model description', dest='model')
  parser.add_argument('-i', '--input-config', metavar='INPUT_CONFIG', default='./input.json', help='input configuration', dest='input_config')
  parser.add_argument('-o', '--output-tensors', metavar='OUTPUT_TENSORS', default='Openpose/MConv_Stage6_L2_5_pointwise/BatchNorm/FusedBatchNorm, Openpose/MConv_Stage6_L1_5_pointwise/BatchNorm/FusedBatchNorm', help='output tensors', dest='output_tensors')
  
  

  args = parser.parse_args()
  run(args.tile, args.model, args.input_config, args.output_tensors)
