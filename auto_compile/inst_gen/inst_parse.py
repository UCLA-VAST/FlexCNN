from math import ceil
import json
import argparse

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

  #model = open("./small.model", "r")
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
  line_id = 1
  for line_id in range(1, len(lines)):
    line = lines[line_id].strip('\n')
    content = line.split(";")
    if len(content) > 1:
      if content[1] == 'Conv2D' or content[1] == 'separable_conv':
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
  layer_configs = {}

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
      elif layer_type == "convb":
        layer_config['FILTER_S1'] = 1
        layer_config['FILTER_S2'] = filter_s
      elif layer_type == "max_pool":
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
    
 # for layer in layer_configs:
  #  print(layer, ":", layer_configs[layer])

  # calculate the region size
#  region0_layers = ["Conv2d_0", "Conv2d_1", "expand1_1st", "expand1_2nd", "expand2_1st", "expand2_2nd", "expand3_1st", \
#                    "expand3_2nd", "expand4_1st", "expand4_2nd", "expand5_1st"]
#  region1_layers = ["expand6_1st", "expand6_2nd", "expand7_1st", "expand7_2nd", "expand8_1st","expand8_2nd","expand9_1st", \
#                    "expand9_2nd", "expand10_1st", "expand10_2nd", "expand11_1st", "expand11_2nd", "expand12_1st", "expand12_2nd"]
#  region2_layers = ["MConv_Stage1_L1_1", "MConv_Stage1_L1_2", "MConv_Stage1_L1_3", "MConv_Stage1_L1_4", \
#                    "MConv_Stage1_L2_1", "MConv_Stage1_L2_2", "MConv_Stage1_L2_3", "MConv_Stage1_L2_4"]
#  region3_layers = ["MConv_Stage2_L1_1", "MConv_Stage2_L1_2", "MConv_Stage2_L1_3", "MConv_Stage2_L1_4", \
#                    "MConv_Stage2_L2_1", "MConv_Stage2_L2_2", "MConv_Stage2_L2_3", "MConv_Stage2_L2_4"]
#  region4_layers = ["MConv_Stage1_L1_5", "MConv_Stage1_L2_5", "expand5_2nd", "expand12_upsample"]
#  region5_layers = ["MConv_Stage1_L1_5", "MConv_Stage1_L2_5"]
#
#  region0_offset = in_out_offset
#  region0_size = 0
#  region1_size = 0
#  region2_size = 0
#  region3_size = 0
#  region4_size = 0
#  region5_size = 0
#
#  for layer_name in region0_layers:
#    region0_size += layer_cout_size_hw[layer_name]
#  region1_offset = region0_offset + region0_size
#  for layer_name in region1_layers:
#    region1_size += layer_cout_size_hw[layer_name]
#  region2_offset = region1_offset + region1_size
#  for layer_name in region2_layers:
#    region2_size += layer_cout_size_hw[layer_name]
#  region3_offset = region2_offset + region2_size
#  for layer_name in region3_layers:
#    region3_size += layer_cout_size_hw[layer_name]
#  region4_offset = region3_offset + region3_size
#  for layer_name in region4_layers:
#    region4_size += layer_cout_size_hw[layer_name]
#  region5_offset = region4_offset + region4_size
#  for layer_name in region5_layers:
#    region5_size += layer_cout_size_hw[layer_name]
#  region4_size += layer_cout_size_hw["MConv_Stage1_L1_5"]
#  region4_size += layer_cout_size_hw["MConv_Stage1_L2_5"]
#  region4_size += layer_cout_size_hw["Conv2d_3_pool"]
#  region4_size += layer_cout_size_hw["Conv2d_7"]
#  region4_size += layer_cout_size_hw["Conv2d_11"]

#  layer_name2 = 'MConv_Stage2_L1_5_4'
#  last_layer_size_diff = (layer_configs[layer_name2]['OUT_NUM_HW'] * layer_configs[layer_name2]['OUT_H_HW'] * layer_configs[layer_name2]['OUT_W_HW']) - (layer_configs[layer_name2]['OUT_NUM_HW'] * layer_configs[layer_name2]['OUT_H_HW'] * layer_configs[layer_name2]['OUT_W_HW'] / 16)
#
#  cin_size = region5_offset + region5_size + 2 * last_layer_size_diff
#  weight_size = weight_offset
#  bias_size = bias_offset
#
#  macros.write("#define CIN_SIZE " + str(int(cin_size)) + '\n')
#  macros.write("#define WEIGHT_SIZE " + str(int(weight_size)) + '\n')
#  macros.write("#define BIAS_SIZE " + str(int(bias_size)) + '\n')
#
#  # Pass3: To generate offsets
#  layer_output_size = []
#  layer_output_size_hw = []
#
#  # reinitialize all parameters
#  line_id = 1
#  cin_offset = 0
#  cout_offset = 0
#  weight_offset = 0
#  bias_offset = 0
#  prev_cin_offset = 0
#
#  vgg_layer_cnt = 0
#  mb_layer_cnt = 0
#  stage1_layer_cnt = 0
#  stage1_iter_cnt = 0
#  stage2_layer_cnt = 0
#  stage2_iter_cnt = 0
#  stage1_channel_cnt = 0
#  stage2_channel_cnt = 0
#
#  in_num_hw = 0
#  out_num_hw = 0
#  in_h_hw = 0
#  in_w_hw = 0
#  out_h_hw = 0
#  out_w_hw = 0
#  filter_s = 0
#
#  in_num = network_in_num
#  out_num = network_in_num
#  in_h = network_in_h
#  in_w = network_in_w
#  out_h = network_in_h
#  out_w = network_in_w
#
#  prev_cin_offset = 0
#  while line_id < len(lines):
#    line = lines[line_id].strip('\n')
#    content = line.split(",")
#    if current_model == "VGG":
#      layer_name = content[0]
#      nxt_filter_s = filter_list[vgg_layer_cnt + 1]
#      if layer_name == 'Conv2d_1':
#        nxt_filter_s = 1
#      prev_cin_offset = cin_offset
#
#      shifted_cout_offset = cout_offset + layer_configs[layer_name]['OUT_NUM_T'] * layer_configs[layer_name]['OUT_W_HW'] * int(nxt_filter_s / 2) + layer_configs[layer_name]['OUT_NUM_T'] * int(nxt_filter_s / 2) + in_out_offset
##      if layer_name == "Conv2d_7":
##        print(shifted_cout_offset)
#
#      layer_configs[layer_name]['SHIFTED_COUT_OFFSET'] = shifted_cout_offset
#      layer_configs[layer_name]['CIN_OFFSET'] = cin_offset
#      layer_configs[layer_name]['WEIGHT_OFFSET'] = weight_offset
#      layer_configs[layer_name]['BIAS_OFFSET'] = bias_offset
#      layer_configs[layer_name]['COUT_OFFSET'] = cout_offset
#      layer_configs[layer_name]['PREV_CIN_OFFSET'] = prev_cin_offset
#
#      cin_offset += layer_cin_size_hw[layer_name]
#      weight_offset += layer_weight_size_hw[layer_name]
#      bias_offset += layer_bias_size_hw[layer_name]
#      cout_offset += layer_cout_size_hw[layer_name]
#
#      vgg_layer_cnt = vgg_layer_cnt + 1
#  	  # store the start line number of stage 1
#      if vgg_layer_cnt == VGG_LAYERS:
#        current_model = "MobileNetV2"
#        stage1_line_id = line_id + 1
#      line_id = line_id + 1
#      
#    elif current_model == "MobileNetV2":
#      layer_name_orig = content[0]
#      if content[1] == "upsample":
#        layer_name = layer_name_orig
#      else:
#        layer_name = layer_name_orig + '_1st'
#      if layer_name == "expand12_upsample":
#        nxt_filter_s = filter_list[VGG_LAYERS + MobileNetV2_LAYERS]
#      else:
#        nxt_filter_s = filter_list[mb_layer_cnt + 1]
#        
#      if content[0] == 'expand6':
#        cur_filter_s = filter_list[VGG_LAYERS + MobileNetV2_LAYERS]
#      else:
#        cur_filter_s = 1
#
#      if layer_name == "expand5_2nd":
##        print(region4_offset, layer_cout_size_hw['MConv_Stage1_L1_5'], layer_cout_size_hw['MConv_Stage1_L2_5'], layer_cout_size_hw['Conv2d_3_pool'])
#        cout_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5'] - in_out_offset
##        print(cout_offset)
#      if layer_name == "expand6_1st":
#        #cin_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5']
#        cin_offset = shifted_cout_offset
#        cout_offset = region1_offset - in_out_offset
#      if layer_name == "expand6_2nd":
#        cin_offset = region1_offset
#      if layer_name == "expand12_upsample":
#        cin_offset = shifted_cout_offset
#        cout_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5'] + layer_cout_size_hw['expand5_2nd'] - in_out_offset
#
#      # Debug
##      if layer_name == "Conv2d_2":
##        print(cout_offset, layer_configs[layer_name]['OUT_W_HW'], nxt_filter_s)
#
#      if content[1] != "upsample":
#        shifted_cout_offset = cout_offset + layer_configs[layer_name]['OUT_NUM_T'] * layer_configs[layer_name]['OUT_W_HW'] * int(nxt_filter_s / 2) + layer_configs[layer_name]['OUT_NUM_T'] * int(nxt_filter_s / 2) + in_out_offset
#        #print(layer_name)
#        #print(cout_offset,layer_configs[layer_name]['OUT_NUM_T'], layer_configs[layer_name]['OUT_W_HW'], int(nxt_filter_s / 2),in_out_offset)
#  #      if layer_name == "Conv2d_7":
#  #        print(shifted_cout_offset)
#  
#        layer_configs[layer_name]['SHIFTED_COUT_OFFSET'] = shifted_cout_offset
#        layer_configs[layer_name]['CIN_OFFSET'] = cin_offset
#        layer_configs[layer_name]['WEIGHT_OFFSET'] = weight_offset
#        layer_configs[layer_name]['BIAS_OFFSET'] = bias_offset
#        layer_configs[layer_name]['COUT_OFFSET'] = cout_offset
#        
#        layer_configs[layer_name]['PREV_CIN_OFFSET'] = prev_cin_offset
#        
#        prev_cin_offset = cin_offset
#  
#        cin_offset += layer_cin_size_hw[layer_name]
#        weight_offset += layer_weight_size_hw[layer_name]
#        bias_offset += layer_bias_size_hw[layer_name]
#        cout_offset += layer_cout_size_hw[layer_name]
#      
#      ############ 2nd layer of mobilenetV2  ################
#      if content[1] == "upsample":
#        layer_name = layer_name_orig
#      else:
#        layer_name = layer_name_orig + '_2nd'
#      if layer_name == "expand5_2nd" or layer_name == "expand12_upsample":
#        nxt_filter_s = filter_list[VGG_LAYERS + MobileNetV2_LAYERS]
#      else:
#        nxt_filter_s = 1
#
#      if layer_name == "expand5_2nd":
##        print(region4_offset, layer_cout_size_hw['MConv_Stage1_L1_5'], layer_cout_size_hw['MConv_Stage1_L2_5'], layer_cout_size_hw['Conv2d_3_pool'])
#        cout_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5'] - in_out_offset
##        print(cout_offset)
#      if layer_name == "expand6_1st":
#        cin_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5']
#        cout_offset = region1_offset - in_out_offset
#      if layer_name == "expand6_2nd":
#        cin_offset = region1_offset
#      if layer_name == "expand12_upsample":
#        cout_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5'] + layer_cout_size_hw['expand5_2nd'] - in_out_offset
#
#      # Debug
##      if layer_name == "Conv2d_2":
##        print(cout_offset, layer_configs[layer_name]['OUT_W_HW'], nxt_filter_s)
#
#      shifted_cout_offset = cout_offset + layer_configs[layer_name]['OUT_NUM_T'] * layer_configs[layer_name]['OUT_W_HW'] * int(nxt_filter_s / 2) + layer_configs[layer_name]['OUT_NUM_T'] * int(nxt_filter_s / 2) + in_out_offset
##      if layer_name == "Conv2d_7":
##        print(shifted_cout_offset)
#
#      layer_configs[layer_name]['SHIFTED_COUT_OFFSET'] = shifted_cout_offset
#      layer_configs[layer_name]['CIN_OFFSET'] = cin_offset
#      layer_configs[layer_name]['WEIGHT_OFFSET'] = weight_offset
#      layer_configs[layer_name]['BIAS_OFFSET'] = bias_offset
#      layer_configs[layer_name]['COUT_OFFSET'] = cout_offset
#      
#      layer_configs[layer_name]['PREV_CIN_OFFSET'] = prev_cin_offset
#      
#      prev_cin_offset = cin_offset
#
#      cin_offset += layer_cin_size_hw[layer_name]
#      weight_offset += layer_weight_size_hw[layer_name]
#      bias_offset += layer_bias_size_hw[layer_name]
#      cout_offset += layer_cout_size_hw[layer_name]
#
#      mb_layer_cnt = mb_layer_cnt + 1
#  	  # store the start line number of stage 1
#      if mb_layer_cnt == MobileNetV2_LAYERS:
#        current_model = "STAGE1"
#        stage1_line_id = line_id + 1
#      line_id = line_id + 1
#    elif current_model == "STAGE1":
#      layer_name = content[0]
#
#      if layer_name == "MConv_Stage1_L1_5" or layer_name == "MConv_Stage1_L2_5":
#        nxt_filter_s = filter_list[VGG_LAYERS + MobileNetV2_LAYERS + STAGE1_LAYERS * 2]
#      else:
#        nxt_filter_s = filter_list[VGG_LAYERS + MobileNetV2_LAYERS + stage1_layer_cnt + stage1_channel_cnt * STAGE1_LAYERS + 1]
#
#      if layer_name == "MConv_Stage1_L1_1":
#        cin_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5']
#        cout_offset = region2_offset - in_out_offset
#      if layer_name == "MConv_Stage1_L1_2":
#        cin_offset = region2_offset
#      if layer_name == "MConv_Stage1_L1_5":
#        cout_offset = region4_offset - in_out_offset
#      if layer_name == "MConv_Stage1_L2_1":
#        cin_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5']
#        cout_offset = region2_offset + region2_size / 2 - in_out_offset
#      if layer_name == "MConv_Stage1_L2_2":
#        cin_offset = region2_offset + region2_size / 2
#      if layer_name == "MConv_Stage1_L2_5":
#        cout_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] - in_out_offset
#
#      shifted_cout_offset = cout_offset + layer_configs[layer_name]['OUT_NUM_T'] * layer_configs[layer_name]['OUT_W_HW'] * int(nxt_filter_s / 2) + layer_configs[layer_name]['OUT_NUM_T'] * int(nxt_filter_s / 2) + in_out_offset
#
#      layer_configs[layer_name]['SHIFTED_COUT_OFFSET'] = shifted_cout_offset
#      layer_configs[layer_name]['CIN_OFFSET'] = cin_offset
#      layer_configs[layer_name]['WEIGHT_OFFSET'] = weight_offset
#      layer_configs[layer_name]['BIAS_OFFSET'] = bias_offset
#      layer_configs[layer_name]['COUT_OFFSET'] = cout_offset
#      layer_configs[layer_name]['PREV_CIN_OFFSET'] = 0
#
#
#      cin_offset += layer_cin_size_hw[layer_name]
#      weight_offset += layer_weight_size_hw[layer_name]
#      bias_offset += layer_bias_size_hw[layer_name]
#      cout_offset += layer_cout_size_hw[layer_name]
#
#
#
#      stage1_layer_cnt = stage1_layer_cnt + 1
#      if stage1_layer_cnt == STAGE1_LAYERS:
#        stage1_layer_cnt = 0
#        stage1_channel_cnt = stage1_channel_cnt + 1
#        if stage1_channel_cnt == 2:
#          stage1_channel_cnt = 0
#          stage1_iter_cnt = stage1_iter_cnt + 1
#          if stage1_iter_cnt == STAGE1_ITER:
#            stage1_iter_cnt = 0
#            current_model = "STAGE2"
#            stage2_line_id = line_id + 1
#      line_id = line_id + 1
#    elif current_model == "STAGE2":
#      layer_name = content[0]
#      layer_name2 = layer_name + '_' + str(stage2_iter_cnt)
#      if layer_name == "MConv_Stage2_L1_5" or layer_name == "MConv_Stage2_L2_5":
#        nxt_filter_s = filter_list[VGG_LAYERS + MobileNetV2_LAYERS + STAGE1_LAYERS * 2]
#      else:
#        nxt_filter_s = filter_list[VGG_LAYERS + MobileNetV2_LAYERS + STAGE1_LAYERS + stage2_layer_cnt + stage2_channel_cnt * STAGE2_LAYERS + 1]
#
#      if stage2_iter_cnt % 2 == 0: # [2,4,6]
#        if layer_name == "MConv_Stage2_L1_1":
#          cin_offset = region4_offset
#          cout_offset = region3_offset - in_out_offset
#        if layer_name == "MConv_Stage2_L1_2":
#          cin_offset = region3_offset
#        if layer_name == "MConv_Stage2_L1_5":
##          cout_offset = region4_offset - in_out_offset
#          cout_offset = region5_offset - in_out_offset
#          if stage2_iter_cnt == STAGE2_ITER - 1:
#            macros.write("#define STAGE2L_OFFSET " + str(int(cout_offset + in_out_offset)) + '\n')
#        if layer_name == "MConv_Stage2_L2_1":
#          cin_offset = region4_offset
#          cout_offset = region3_offset + region3_size / 2 - in_out_offset
#        if layer_name == "MConv_Stage2_L2_2":
#          cin_offset = region3_offset + region3_size / 2
#        if layer_name == "MConv_Stage2_L2_5":
##          cout_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] - in_out_offset
#          if stage2_iter_cnt == 4:
#            cout_offset = region5_offset + (layer_configs[layer_name2]['OUT_NUM_HW'] * layer_configs[layer_name2]['OUT_H_HW'] * layer_configs[layer_name2]['OUT_W_HW']) - in_out_offset
#          else:
#            cout_offset = region5_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] - in_out_offset
#          if stage2_iter_cnt == STAGE2_ITER - 1:
#            macros.write("#define STAGE2R_OFFSET " + str(int(cout_offset + in_out_offset)) + '\n')
#
#      elif stage2_iter_cnt % 2 == 1: # [3,5]
#        if layer_name == "MConv_Stage2_L1_1":
##          cin_offset = region4_offset
#          cin_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5']
#          cout_offset = region3_offset - in_out_offset
#        if layer_name == "MConv_Stage2_L1_2":
#          cin_offset = region3_offset
#        if layer_name == "MConv_Stage2_L1_5":
#          cout_offset = region4_offset - in_out_offset
#          if stage2_iter_cnt == STAGE2_ITER - 1:
#            macros.write("#define STAGE2L_OFFSET " + str(int(cout_offset + in_out_offset)) + '\n')
#        if layer_name == "MConv_Stage2_L2_1":
##          cin_offset = region4_offset
#          cin_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] + layer_cout_size_hw['MConv_Stage1_L2_5']
#          cout_offset = region3_offset + region3_size / 2 - in_out_offset
#        if layer_name == "MConv_Stage2_L2_2":
#          cin_offset = region3_offset + region3_size / 2
#        if layer_name == "MConv_Stage2_L2_5":
#          cout_offset = region4_offset + layer_cout_size_hw['MConv_Stage1_L1_5'] - in_out_offset
#          if stage2_iter_cnt == STAGE2_ITER - 1:
#            macros.write("#define STAGE2R_OFFSET " + str(int(cout_offset + in_out_offset)) + '\n')
#
#      #print(layer_name)
#      shifted_cout_offset = cout_offset + layer_configs[layer_name2]['OUT_NUM_T'] * layer_configs[layer_name2]['OUT_W_HW'] * int(nxt_filter_s / 2) + layer_configs[layer_name2]['OUT_NUM_T'] * int(nxt_filter_s / 2) + in_out_offset
#
#
#      layer_configs[layer_name + '_' + str(stage2_iter_cnt)]['SHIFTED_COUT_OFFSET'] = shifted_cout_offset
#      layer_configs[layer_name + '_' + str(stage2_iter_cnt)]['CIN_OFFSET'] = cin_offset
#      layer_configs[layer_name + '_' + str(stage2_iter_cnt)]['WEIGHT_OFFSET'] = weight_offset
#      layer_configs[layer_name + '_' + str(stage2_iter_cnt)]['BIAS_OFFSET'] = bias_offset
#      layer_configs[layer_name + '_' + str(stage2_iter_cnt)]['COUT_OFFSET'] = cout_offset
#      layer_configs[layer_name + '_' + str(stage2_iter_cnt)]['PREV_CIN_OFFSET'] = 0
#
#
#
#      cin_offset += layer_cin_size_hw[layer_name]
#      weight_offset += layer_weight_size_hw[layer_name]
#      bias_offset += layer_bias_size_hw[layer_name]
#      cout_offset += layer_cout_size_hw[layer_name]
#
#      stage2_layer_cnt = stage2_layer_cnt + 1
#      if stage2_layer_cnt == STAGE2_LAYERS:
#        stage2_layer_cnt = 0
#        stage2_channel_cnt = stage2_channel_cnt + 1
#        if stage2_channel_cnt == 2:
#          stage2_channel_cnt = 0
#          stage2_iter_cnt = stage2_iter_cnt + 1
#          if stage2_iter_cnt == STAGE2_ITER:
#            stage2_iter_cnt = 0
#            current_model = "STAGE2"
#            break
#          else:
#            line_id = stage2_line_id - 1
#      line_id = line_id + 1
##    line_id = line_id + 1
#
#  macros.write("#define MAX_LAYER_BATCH " + str(int(max_layer_batch)) + '\n')
#  # Pass4: To print out insts
#  # reinitialize all parameters
#  line_id = 1
#  current_model = "VGG"
#
#  vgg_layer_cnt = 0
#  vgg_layer_cnt = 0
#  stage1_layer_cnt = 0
#  stage1_iter_cnt = 0
#  stage2_layer_cnt = 0
#  stage2_iter_cnt = 0
#  stage1_channel_cnt = 0
#  stage2_channel_cnt = 0
#  mb_layer_cnt = 0
#
#  while line_id < len(lines):
#    line = lines[line_id].strip('\n')
#    content = line.split(",")
#    if current_model == "VGG":
#      layer_name = content[0]
#
#      inst0 = [layer_configs[layer_name]['IN_NUM_HW'], layer_configs[layer_name]['OUT_NUM_HW'], layer_configs[layer_name]['IN_H_HW'], layer_configs[layer_name]['IN_W_HW'], layer_configs[layer_name]['OUT_H_HW'], layer_configs[layer_name]['OUT_W_HW']]
#      inst1 = [layer_configs[layer_name]['IN_NUM'], layer_configs[layer_name]['OUT_NUM'], layer_configs[layer_name]['IN_H'], layer_configs[layer_name]['IN_W'], layer_configs[layer_name]['OUT_H'], layer_configs[layer_name]['OUT_W']]
#      inst2 = [layer_configs[layer_name]['CIN_OFFSET'], layer_configs[layer_name]['WEIGHT_OFFSET'], layer_configs[layer_name]['BIAS_OFFSET'], layer_configs[layer_name]['SHIFTED_COUT_OFFSET'], layer_configs[layer_name]['FILTER_S1'], layer_configs[layer_name]['FILTER_S2'], layer_configs[layer_name]['STRIDE']]
#      inst3 = [layer_configs[layer_name]['LAYER_EN'], layer_configs[layer_name]['PREV_CIN_OFFSET'], layer_configs[layer_name]['IN_NUM_T'], layer_configs[layer_name]['OUT_NUM_T'], layer_configs[layer_name]['IN_H_T'], layer_configs[layer_name]['IN_W_T'], layer_configs[layer_name]['NXT_LAYER_BATCH']]
#      inst4 = [layer_configs[layer_name]['TASK_NUM1'], layer_configs[layer_name]['TASK_NUM2'], layer_configs[layer_name]['LOCAL_ACCUM_NUM'], layer_configs[layer_name]['LOCAL_REG_NUM'], layer_configs[layer_name]['ROW_IL_FACTOR'], layer_configs[layer_name]['COL_IL_FACTOR']]
#
#      insts.writelines(" ".join(str(int(e)) for e in inst0) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst1) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst2) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst3) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst4) + "\n")
#      insts.writelines("\n")
#
#      vgg_layer_cnt = vgg_layer_cnt + 1
#  	  # store the start line number of stage 1
#      if vgg_layer_cnt == VGG_LAYERS:
#        current_model = "MobileNetV2"
#        stage1_line_id = line_id + 1
#      line_id = line_id + 1
#    elif current_model == "MobileNetV2":
#      layer_name_orig = content[0]
#      if content[1] == "upsample":
#        layer_name = layer_name_orig
#      else:
#        layer_name = layer_name_orig + '_1st'
#      inst0 = [layer_configs[layer_name]['IN_NUM_HW'], layer_configs[layer_name]['OUT_NUM_HW'], layer_configs[layer_name]['IN_H_HW'], layer_configs[layer_name]['IN_W_HW'], layer_configs[layer_name]['OUT_H_HW'], layer_configs[layer_name]['OUT_W_HW']]
#      inst1 = [layer_configs[layer_name]['IN_NUM'], layer_configs[layer_name]['OUT_NUM'], layer_configs[layer_name]['IN_H'], layer_configs[layer_name]['IN_W'], layer_configs[layer_name]['OUT_H'], layer_configs[layer_name]['OUT_W']]
#      inst2 = [layer_configs[layer_name]['CIN_OFFSET'], layer_configs[layer_name]['WEIGHT_OFFSET'], layer_configs[layer_name]['BIAS_OFFSET'], layer_configs[layer_name]['SHIFTED_COUT_OFFSET'], layer_configs[layer_name]['FILTER_S1'], layer_configs[layer_name]['FILTER_S2'], layer_configs[layer_name]['STRIDE']]
#      inst3 = [layer_configs[layer_name]['LAYER_EN'], layer_configs[layer_name]['PREV_CIN_OFFSET'], layer_configs[layer_name]['IN_NUM_T'], layer_configs[layer_name]['OUT_NUM_T'], layer_configs[layer_name]['IN_H_T'], layer_configs[layer_name]['IN_W_T'], layer_configs[layer_name]['NXT_LAYER_BATCH']]
#      inst4 = [layer_configs[layer_name]['TASK_NUM1'], layer_configs[layer_name]['TASK_NUM2'], layer_configs[layer_name]['LOCAL_ACCUM_NUM'], layer_configs[layer_name]['LOCAL_REG_NUM'], layer_configs[layer_name]['ROW_IL_FACTOR'], layer_configs[layer_name]['COL_IL_FACTOR']]
#
#      insts.writelines(" ".join(str(int(e)) for e in inst0) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst1) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst2) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst3) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst4) + "\n")
#      insts.writelines("\n")
#      
#      if content[1] == "MobileNetV2":
#         layer_name = layer_name_orig + '_2nd'
#         inst0 = [layer_configs[layer_name]['IN_NUM_HW'], layer_configs[layer_name]['OUT_NUM_HW'], layer_configs[layer_name]['IN_H_HW'], layer_configs[layer_name]['IN_W_HW'], layer_configs[layer_name]['OUT_H_HW'], layer_configs[layer_name]['OUT_W_HW']]
#         inst1 = [layer_configs[layer_name]['IN_NUM'], layer_configs[layer_name]['OUT_NUM'], layer_configs[layer_name]['IN_H'], layer_configs[layer_name]['IN_W'], layer_configs[layer_name]['OUT_H'], layer_configs[layer_name]['OUT_W']]
#         inst2 = [layer_configs[layer_name]['CIN_OFFSET'], layer_configs[layer_name]['WEIGHT_OFFSET'], layer_configs[layer_name]['BIAS_OFFSET'], layer_configs[layer_name]['SHIFTED_COUT_OFFSET'], layer_configs[layer_name]['FILTER_S1'], layer_configs[layer_name]['FILTER_S2'], layer_configs[layer_name]['STRIDE']]
#         inst3 = [layer_configs[layer_name]['LAYER_EN'], layer_configs[layer_name]['PREV_CIN_OFFSET'], layer_configs[layer_name]['IN_NUM_T'], layer_configs[layer_name]['OUT_NUM_T'], layer_configs[layer_name]['IN_H_T'], layer_configs[layer_name]['IN_W_T'], layer_configs[layer_name]['NXT_LAYER_BATCH']]
#         inst4 = [layer_configs[layer_name]['TASK_NUM1'], layer_configs[layer_name]['TASK_NUM2'], layer_configs[layer_name]['LOCAL_ACCUM_NUM'], layer_configs[layer_name]['LOCAL_REG_NUM'], layer_configs[layer_name]['ROW_IL_FACTOR'], layer_configs[layer_name]['COL_IL_FACTOR']]
#   
#         insts.writelines(" ".join(str(int(e)) for e in inst0) + "\n")
#         insts.writelines(" ".join(str(int(e)) for e in inst1) + "\n")
#         insts.writelines(" ".join(str(int(e)) for e in inst2) + "\n")
#         insts.writelines(" ".join(str(int(e)) for e in inst3) + "\n")
#         insts.writelines(" ".join(str(int(e)) for e in inst4) + "\n")
#         insts.writelines("\n")
#
#      mb_layer_cnt = mb_layer_cnt + 1
#  	  # store the start line number of stage 1
#      if mb_layer_cnt == MobileNetV2_LAYERS:
#        current_model = "STAGE1"
#        stage1_line_id = line_id + 1
#      line_id = line_id + 1
#    elif current_model == "STAGE1":
#      layer_name = content[0]
#
#      inst0 = [layer_configs[layer_name]['IN_NUM_HW'], layer_configs[layer_name]['OUT_NUM_HW'], layer_configs[layer_name]['IN_H_HW'], layer_configs[layer_name]['IN_W_HW'], layer_configs[layer_name]['OUT_H_HW'], layer_configs[layer_name]['OUT_W_HW']]
#      inst1 = [layer_configs[layer_name]['IN_NUM'], layer_configs[layer_name]['OUT_NUM'], layer_configs[layer_name]['IN_H'], layer_configs[layer_name]['IN_W'], layer_configs[layer_name]['OUT_H'], layer_configs[layer_name]['OUT_W']]
#      inst2 = [layer_configs[layer_name]['CIN_OFFSET'], layer_configs[layer_name]['WEIGHT_OFFSET'], layer_configs[layer_name]['BIAS_OFFSET'], layer_configs[layer_name]['SHIFTED_COUT_OFFSET'], layer_configs[layer_name]['FILTER_S1'], layer_configs[layer_name]['FILTER_S2'], layer_configs[layer_name]['STRIDE']]
#      inst3 = [layer_configs[layer_name]['LAYER_EN'], layer_configs[layer_name]['PREV_CIN_OFFSET'], layer_configs[layer_name]['IN_NUM_T'], layer_configs[layer_name]['OUT_NUM_T'], layer_configs[layer_name]['IN_H_T'], layer_configs[layer_name]['IN_W_T'], layer_configs[layer_name]['NXT_LAYER_BATCH']]
#      inst4 = [layer_configs[layer_name]['TASK_NUM1'], layer_configs[layer_name]['TASK_NUM2'], layer_configs[layer_name]['LOCAL_ACCUM_NUM'], layer_configs[layer_name]['LOCAL_REG_NUM'], layer_configs[layer_name]['ROW_IL_FACTOR'], layer_configs[layer_name]['COL_IL_FACTOR']]
#
#      insts.writelines(" ".join(str(int(e)) for e in inst0) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst1) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst2) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst3) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst4) + "\n")
#      insts.writelines("\n")
#
## Change the execution order of two branches
#      stage1_layer_cnt = stage1_layer_cnt + 1
#      if stage1_layer_cnt == STAGE1_LAYERS:
#        stage1_layer_cnt = 0
#        stage1_channel_cnt = stage1_channel_cnt + 1
#        if stage1_channel_cnt == 2:
#          stage1_channel_cnt = 0
#          stage1_iter_cnt = stage1_iter_cnt + 1
#          if stage1_iter_cnt == STAGE1_ITER:
#            stage1_iter_cnt = 0
#            stage2_line_id = line_id + 1
#            current_model = "STAGE2"
#
#
#      line_id = stage1_line_id + stage1_channel_cnt * STAGE1_LAYERS + stage1_layer_cnt + stage1_iter_cnt * STAGE1_LAYERS * 2
#      if current_model == "STAGE2":
#        line_id = stage2_line_id
#
#    elif current_model == "STAGE2":
#      layer_name2 = content[0]
#      layer_name = layer_name2 + '_' + str(stage2_iter_cnt)
#      #print(layer_name)
#      
#      inst0 = [layer_configs[layer_name]['IN_NUM_HW'], layer_configs[layer_name]['OUT_NUM_HW'], layer_configs[layer_name]['IN_H_HW'], layer_configs[layer_name]['IN_W_HW'], layer_configs[layer_name]['OUT_H_HW'], layer_configs[layer_name]['OUT_W_HW']]
#      inst1 = [layer_configs[layer_name]['IN_NUM'], layer_configs[layer_name]['OUT_NUM'], layer_configs[layer_name]['IN_H'], layer_configs[layer_name]['IN_W'], layer_configs[layer_name]['OUT_H'], layer_configs[layer_name]['OUT_W']]
#      inst2 = [layer_configs[layer_name]['CIN_OFFSET'], layer_configs[layer_name]['WEIGHT_OFFSET'], layer_configs[layer_name]['BIAS_OFFSET'], layer_configs[layer_name]['SHIFTED_COUT_OFFSET'], layer_configs[layer_name]['FILTER_S1'], layer_configs[layer_name]['FILTER_S2'], layer_configs[layer_name]['STRIDE']]
#      #print(layer_name)
#      inst3 = [layer_configs[layer_name]['LAYER_EN'], layer_configs[layer_name]['PREV_CIN_OFFSET'], layer_configs[layer_name]['IN_NUM_T'], layer_configs[layer_name]['OUT_NUM_T'], layer_configs[layer_name]['IN_H_T'], layer_configs[layer_name]['IN_W_T'], layer_configs[layer_name]['NXT_LAYER_BATCH']]
#      inst4 = [layer_configs[layer_name]['TASK_NUM1'], layer_configs[layer_name]['TASK_NUM2'], layer_configs[layer_name]['LOCAL_ACCUM_NUM'], layer_configs[layer_name]['LOCAL_REG_NUM'], layer_configs[layer_name]['ROW_IL_FACTOR'], layer_configs[layer_name]['COL_IL_FACTOR']]
#
#      insts.writelines(" ".join(str(int(e)) for e in inst0) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst1) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst2) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst3) + "\n")
#      insts.writelines(" ".join(str(int(e)) for e in inst4) + "\n")
#      insts.writelines("\n")
#
## Change the execution order of two branches
#      stage2_layer_cnt = stage2_layer_cnt + 1
#      if stage2_layer_cnt == STAGE2_LAYERS:
#        stage2_layer_cnt = 0
#        stage2_channel_cnt = stage2_channel_cnt + 1
#        if stage2_channel_cnt == 2:
#          stage2_channel_cnt = 0
#          stage2_iter_cnt = stage2_iter_cnt + 1
#          if stage2_iter_cnt == STAGE2_ITER:
#            stage2_iter_cnt = 0
#            current_model = "STAGE2"
#            #stage2_line_id = line_id + 1
#            break
#
#
#      line_id = stage2_line_id + stage2_channel_cnt * STAGE2_LAYERS + stage2_layer_cnt
#
#
#  model.close()
#  insts.close()
#
#  macros.close()
#  weight_load.close()
#  bias_load.close()

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Data reorganization.')

  parser.add_argument('-t', '--tile', metavar='TILE', default='./tile.json', help='tiling configuration', dest='tile')
  parser.add_argument('-m', '--model', metavar='MODEL', default='./network_out.model', help='model description', dest='model')
  parser.add_argument('-i', '--input-config', metavar='INPUT_CONFIG', default='./input.json', help='input configuration', dest='input_config')
  parser.add_argument('-o', '--output-tensors', metavar='OUTPUT_TENSORS', default='Openpose/MConv_Stage6_L2_5_pointwise/BatchNorm/FusedBatchNorm, Openpose/MConv_Stage6_L1_5_pointwise/BatchNorm/FusedBatchNorm', help='output tensors', dest='output_tensors')
  
  

  args = parser.parse_args()
  run(args.tile, args.model, args.input_config, args.output_tensors)
