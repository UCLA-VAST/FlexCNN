from re import T
import numpy as np
import copy
from collections import OrderedDict

def getBurstLength(curr_tiles, next_tiles):
	curr_h, curr_w, curr_d = curr_tiles
	next_h, next_w, next_d = next_tiles
	burst_len = 0
	if next_h <= curr_h and next_w <= curr_w and next_d <= curr_d:
		burst_len = next_h * next_w * next_d
	if next_h > curr_h:
		burst_len = curr_h * next_w * next_d
	if next_w > curr_w:
		burst_len = curr_w * next_d
	if next_d > curr_d:
		burst_len = curr_d
	return burst_len

def effective_dram_est(port_width, burst_len, fre):
	dram_latency = 250
	eff_bw = port_width * burst_len / 8 / ((dram_latency + burst_len) / (fre * 1e6)) / 1e9
	eff_port_width = port_width * burst_len / (dram_latency + burst_len)
	return eff_bw, eff_port_width

def cin_load_est(params):
	in_dataLayout = params['IN_DATA_LAYOUT']
	in_num_t = params['LAYER_IN_NUM_T']
	in_h_t = params['LAYER_IN_H_T']
	in_w_t = params['LAYER_IN_W_T']
	lane = params['LANE']
	fre = params['FRE']
	dw = params['DATA_W0']
	port_width = params['BUS_W']
	padding_h = params['PAD_H']
	padding_w = params['PAD_W']

	burst_len = 0
	if in_dataLayout == 1:
		burst_len = (in_w_t + padding_w) * in_num_t / (port_width / dw)
	elif in_dataLayout == 2:
		burst_len = in_h_t * in_w_t * in_num_t / (port_width / dw)

	eff_bw, eff_port_width = effective_dram_est(port_width, burst_len, fre)
	load_phase_latency  = in_num_t * (in_h_t + padding_h) * (in_w_t + padding_w) / (eff_port_width / dw)
	write_phase_latency = in_num_t * (in_h_t + padding_h) * (in_w_t + padding_w) / lane

	bottleneck = ''
	if write_phase_latency == max(load_phase_latency, write_phase_latency):
		bottleneck = 'fifo'
	elif load_phase_latency == max(load_phase_latency, write_phase_latency):
		bottleneck = 'dram'

	return max(load_phase_latency, write_phase_latency), bottleneck

def weight_load_est(params):
	in_num_t = params['LAYER_IN_NUM_T']
	out_num_t = params['LAYER_OUT_NUM_T']
	fh1 = 0
	fw1 = 0
	fh2 = params['FILTER_H']
	fw2 = params['FILTER_W']
	lane = params['LANE']
	fre = params['FRE']
	dw1 = params['DATA_W0']
	dw2 = params['DATA_W1']
	dw3 = params['DATA_W2']
	port_width = params['BUS_W']
	depth_en = 0
	point_en = 1
	bias_en = 0
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

def conv_est(params):
	in_num = params['LAYER_IN_NUM']
	in_num_t = params['LAYER_IN_NUM_T']
	out_num_t = params['LAYER_OUT_NUM_T']
	in_h_t = params['LAYER_IN_H_T']
	in_w_t = params['LAYER_IN_W_T']
	out_h_t = in_h_t
	out_w_t = in_w_t
	fh = params['FILTER_H']
	fw = params['FILTER_W']
	padding_h = params['PAD_H']
	padding_w = params['PAD_W']
	lane = params['LANE']
	sa_rows = params['SA_ROWS']
	sa_cols = params['SA_COLS']
	sa_lane = params['SA_SIMD']

	cin_load = in_num_t * (in_h_t + padding_h) * (in_w_t + padding_w) / lane
	weight_load = in_num_t * out_num_t * fh * fw / lane
	load_phase_latency = max(cin_load, weight_load)
	compute_phase_latency = in_num_t * out_num_t * (out_h_t) * (out_w_t) * fh * fw / sa_rows / sa_cols / sa_lane # update for DConv
	compute_drain_latency = out_num_t * out_w_t / sa_cols * out_h_t / np.ceil(in_num / in_num_t)
	cout_write = out_num_t * out_h_t * out_w_t / np.ceil(in_num / in_num_t) / lane
	write_phase_latency = cout_write

	bottleneck = ''
	if cin_load == max(load_phase_latency, compute_phase_latency, compute_drain_latency, write_phase_latency):
		bottleneck = 'cin'
	elif weight_load == max(load_phase_latency, compute_phase_latency, compute_drain_latency, write_phase_latency):
		bottleneck = 'weight'
	elif compute_phase_latency == max(load_phase_latency, compute_phase_latency, compute_drain_latency, write_phase_latency):
		bottleneck = 'compute'
	elif compute_drain_latency == max(load_phase_latency, compute_phase_latency, compute_drain_latency, write_phase_latency):
		bottleneck = 'compute_drain'
	elif write_phase_latency == max(load_phase_latency, compute_phase_latency, compute_drain_latency, write_phase_latency):
		bottleneck = 'cout'
	return max(load_phase_latency, compute_phase_latency, compute_drain_latency, write_phase_latency), bottleneck

def relu_est(num_tiles, num_t, h_t, w_t, lane, after_conv):
	if after_conv:
		return num_t * h_t * w_t / lane / num_tiles
	else:
		return num_t * h_t * w_t / lane

def pool_est(num_tiles, num_t, h_t, w_t, lane, after_conv):
	if after_conv:
		return num_t * h_t * w_t / lane / num_tiles
	else:
		return num_t * h_t * w_t / lane


def cout_write_est(params, num_tiles):
	out_num_t = params['LAYER_OUT_NUM_T']
	in_h_t = params['LAYER_IN_H_T']
	in_w_t = params['LAYER_IN_W_T']
	out_h_t = in_h_t*params['UPSAMPLE_FACTOR']//params['DOWNSAMPLE_FACTOR']
	out_w_t = in_w_t*params['UPSAMPLE_FACTOR']//params['DOWNSAMPLE_FACTOR']
	if out_h_t == 0 or out_w_t == 0:
		print(f'in_h_t: {in_h_t}, in_w_t: {in_w_t}, out_h_t: {out_h_t}, out_w_t: {out_w_t}, upsample_factor: {params["UPSAMPLE_FACTOR"]}, downsample_factor: {params["DOWNSAMPLE_FACTOR"]}')
		raise ValueError('out_h_t or out_w_t is 0')
	
	lane = params['LANE']
	port_width = params['BUS_W']
	dw = params['DATA_W0']
	out_dataLayout = params['OUT_DATA_LAYOUT']
	fre = params['FRE']
	load_phase_latency = out_num_t * out_h_t * out_w_t / lane / num_tiles
	if out_dataLayout == 1:
		burst_len = (out_w_t) * out_num_t / (port_width / dw)
	elif out_dataLayout == 2:
		burst_len = (out_h_t) * (out_w_t) * out_num_t / (port_width / dw)

	eff_bw, eff_port_width = effective_dram_est(port_width, burst_len, fre)
	write_phase_latency = out_num_t * out_h_t * out_w_t / num_tiles / (eff_port_width / dw)
	bottleneck = ''
	if load_phase_latency == max(load_phase_latency, write_phase_latency):
		bottleneck = 'fifo'
	elif write_phase_latency == max(load_phase_latency, write_phase_latency):
		bottleneck = 'dram'

	return max(load_phase_latency, write_phase_latency), bottleneck


def layer_latency_est(params):
	#generated by greedy search in model_est
	in_num_t = params['LAYER_IN_NUM_T']
	out_num_t = params['LAYER_OUT_NUM_T']
	in_h_t = params['LAYER_IN_H_T']
	in_w_t = params['LAYER_IN_W_T']

	#hardware candidates
	sa_rows = params['SA_ROWS']
	sa_cols = params['SA_COLS']
	sa_simd = params['SA_SIMD']

	inst_in_num = params['LAYER_IN_NUM']
	inst_out_num = params['LAYER_OUT_NUM']
	inst_in_h = params['LAYER_IN_H']
	inst_in_w = params['LAYER_IN_W']

	conv_type = params['CONV_TYPE']
	conv_en = True if conv_type in ['NConv', 'DConv', 'TConv'] else False
	in_num_tiles = np.ceil(inst_in_num / in_num_t)
	out_num_tiles = np.ceil(inst_out_num / out_num_t)
	in_h_tiles = np.ceil(inst_in_h / in_h_t)
	in_w_tiles = np.ceil(inst_in_w / in_w_t)

	if conv_en:
		cin_load_latency, cin_bottleneck = cin_load_est(params)
		weight_load_latency = weight_load_est(params)
		conv_latency, conv_bottleneck = conv_est(params)
		cout_write_latency, cout_bottleneck = cout_write_est(params, in_num_tiles)
		total_iter = in_num_tiles*out_num_tiles*in_h_tiles*in_w_tiles
	else:
		cin_load_latency, cin_bottleneck = cin_load_est(params)
		weight_load_latency = 0
		conv_latency, conv_bottleneck = 0, ''
		cout_write_latency, cout_bottleneck = cout_write_est(params, in_num_tiles)
		total_iter = in_num_tiles*in_h_tiles*in_w_tiles

	stage_latency = max(cin_load_latency, weight_load_latency, conv_latency, cout_write_latency)
	extra_latency = max(cin_load_latency, weight_load_latency) + cout_write_latency + conv_latency # the data drain latency is omitted
	layer_latency = extra_latency + stage_latency * total_iter

	bottleneck = ''
	if cin_load_latency == stage_latency:
		bottleneck = 'cin_load'+'('+cin_bottleneck+')'
	elif weight_load_latency == stage_latency:
		bottleneck = 'weight_load'
	elif cout_write_latency == stage_latency:
		bottleneck = 'cout_write'+'('+cout_bottleneck+')'
	elif conv_latency == stage_latency:
		bottleneck = 'conv'+'('+conv_bottleneck+')'
	else:
		bottleneck = 'unknown'

	peak_perf = sa_cols*sa_rows*sa_simd #accelerator peak performance MACs/cycle
	layer_macs = 0
	layer_ideal_latency = 0
	if conv_en:
		layer_macs = params['MACS']
		layer_ideal_latency = layer_macs/peak_perf
		dsp_efficiency = (100*layer_ideal_latency/layer_latency, bottleneck)
	else:
		layer_ideal_latency = 0
		dsp_efficiency = (0, bottleneck)

	return layer_latency, dsp_efficiency


def model_latency_est(params, layer_configs, layers_order, dynamic_tiling_level):
	# deep copy params
	hw_sw_params = copy.deepcopy(params)
	latency = 0
	model_fail = 0
	layers_num = len(layers_order)
	layer_in_num_t_list = [None]*layers_num
	layer_out_num_t_list = [None]*layers_num
	layer_in_h_t_list = [None]*layers_num
	layer_in_w_t_list = [None]*layers_num
	layer_dsp_eff_list = [None]*layers_num

	# ordered dict
	layer_opt_params = OrderedDict()
	for layer_name in layers_order:
		layer_opt_param = {}
		layer_opt_param['LAYER_IN_NUM_T'] = 0
		layer_opt_param['LAYER_OUT_NUM_T'] = 0
		layer_opt_param['LAYER_IN_H_T'] = 0
		layer_opt_param['LAYER_IN_W_T'] = 0
		layer_opt_param['LAYER_DSP_EFF'] = 0
		layer_opt_params[layer_name] = layer_opt_param

	visited_layers = {layer_name:False for layer_name in layers_order}
	# print('----------------------------------------------------------------------------------------')
	for layer_name in layers_order:
		layer_config = layer_configs[layer_name]
		hw_sw_params.update(layer_config)
		in_num_hw = hw_sw_params['LAYER_IN_NUM_T']
		out_num_hw = hw_sw_params['LAYER_OUT_NUM_T']
		in_h_hw = hw_sw_params['LAYER_IN_H_T']
		in_w_hw = hw_sw_params['LAYER_IN_W_T']
		out_h_hw = hw_sw_params['LAYER_OUT_H_T']
		out_w_hw = hw_sw_params['LAYER_OUT_W_T']
		downsample_factor = layer_config['DOWNSAMPLE_FACTOR']
		upsample_factor = layer_config['UPSAMPLE_FACTOR']

		sa_cols = hw_sw_params['SA_COLS']
		sa_rows = hw_sw_params['SA_ROWS']
		sa_simd = hw_sw_params['SA_SIMD']

		layer_id = layer_config['LAYER_ID']
		layer_in_num = int(np.ceil(layer_config['LAYER_IN_NUM']/sa_simd)*sa_simd)
		layer_out_num = int(np.ceil(layer_config['LAYER_OUT_NUM']/sa_rows)*sa_rows)
		layer_in_h = layer_config['LAYER_IN_H']
		layer_in_w = layer_config['LAYER_IN_W']

		layer_in_dependency = layer_config['IN_NUM_DEPEND']
		layer_out_dependency = layer_config['OUT_NUM_DEPEND']
		layer_in_w_dependency = layer_config['IN_W_T_DEPEND']
		

		if dynamic_tiling_level == 0 or dynamic_tiling_level == 1:
			layer_in_h_t_candidates = [in_h_hw]
			layer_in_w_t_candidates = [in_w_hw]
		else:
			layer_in_h_t_candidates = list(filter(lambda x : layer_in_h % x == 0 and x % 2 == 0, range(1, layer_in_h + 1)))
			flag = True
			for depend_layer_name in layer_in_w_dependency:
				depend_layer_config = layer_configs[depend_layer_name]
				if visited_layers[depend_layer_name]:
					depend_layer_downsample_factor = (depend_layer_config['LAYER_IN_H']/layer_in_h)
					layer_in_w_t_candidates = [int(layer_opt_params[depend_layer_name]['LAYER_IN_W_T']//depend_layer_downsample_factor)]
					flag = False
			if flag:
				layer_in_w_t_candidates = list(filter(lambda x : x % sa_cols == 0 and layer_in_w % x == 0 and x % 2 == 0, range(1, layer_in_w + 1)))
		if dynamic_tiling_level == 0:
			layer_in_num_t_candidates = [in_num_hw]
			layer_out_num_t_candidates = [out_num_hw]
		else:

			#TODO: the candidate choice needs to be improved
			layer_in_num_t_candidates = list(filter(lambda x : x % sa_simd == 0, range(8, max(16, min(in_num_hw, layer_in_num)) + 1)))
			layer_out_num_t_candidates = list(filter(lambda x : x % sa_rows == 0 and x % sa_simd == 0, range(16, max(16,layer_out_num) + 1)))
			
			for in_depend_pair in layer_in_dependency:
				in_or_out = in_depend_pair[0]
				depend_layer_name = in_depend_pair[1]
				if visited_layers[depend_layer_name]:
					if in_or_out == 'in':
						layer_in_num_t_candidates = [layer_opt_params[depend_layer_name]['LAYER_IN_NUM_T']]
						# if (filter_size-downsample_factor)==0:
						# 	layer_in_h_t_candidates = [layer_opt_params[depend_layer_name]['LAYER_IN_H_T']//downsample_factor]
						# 	layer_in_w_t_candidates = [layer_opt_params[depend_layer_name]['LAYER_IN_W_T']//downsample_factor]
					else:
						layer_in_num_t_candidates = [layer_opt_params[depend_layer_name]['LAYER_OUT_NUM_T']]
						# if (filter_size-downsample_factor)==0:
						# 	layer_in_h_t_candidates = [layer_opt_params[depend_layer_name]['LAYER_IN_H_T']//downsample_factor]
						# 	layer_in_w_t_candidates = [layer_opt_params[depend_layer_name]['LAYER_IN_W_T']//downsample_factor]
					break

			for out_depend_pair in layer_out_dependency:
				in_or_out = out_depend_pair[0]
				depend_layer_name = out_depend_pair[1]
				if visited_layers[depend_layer_name]:
					if in_or_out == 'in':
						layer_out_num_t_candidates = [layer_opt_params[depend_layer_name]['LAYER_IN_NUM_T']]
					else:
						layer_out_num_t_candidates = [layer_opt_params[depend_layer_name]['LAYER_OUT_NUM_T']]
					break

		visited_layers[layer_name] = True

		if len(layer_in_num_t_candidates) == 0 or len(layer_out_num_t_candidates) == 0 or len(layer_in_h_t_candidates) == 0 or len(layer_in_w_t_candidates) == 0 \
			or 0 in layer_in_num_t_candidates or 0 in layer_out_num_t_candidates or 0 in layer_in_h_t_candidates or 0 in layer_in_w_t_candidates \
			or 1 in layer_in_w_t_candidates or 1 in layer_in_h_t_candidates:
			model_fail = 1
			return np.inf, None

		in_buf_size = in_num_hw * in_h_hw * in_w_hw
		out_buf_size = out_num_hw * out_h_hw * out_w_hw
		opt_layer_latency = np.inf
		opt_layer_eff = -np.inf
		opt_layer_in_num_t = 0
		opt_layer_in_h_t = 0
		opt_layer_in_w_t = 0
		opt_layer_out_num_t = 0
		for layer_in_num_t in layer_in_num_t_candidates:
			for layer_out_num_t in layer_out_num_t_candidates:
				for layer_in_h_t in layer_in_h_t_candidates:
					for layer_in_w_t in layer_in_w_t_candidates:
						layer_out_h_t = layer_in_h_t*upsample_factor
						layer_out_w_t = layer_in_w_t*upsample_factor
						if layer_in_num_t * layer_in_h_t * layer_in_w_t > in_buf_size:
							continue
						if layer_out_num_t * layer_out_h_t * layer_out_w_t > out_buf_size:
							continue

						hw_sw_params['LAYER_IN_NUM_T'] = layer_in_num_t
						hw_sw_params['LAYER_OUT_NUM_T'] = layer_out_num_t
						hw_sw_params['LAYER_IN_H_T'] = layer_in_h_t
						hw_sw_params['LAYER_IN_W_T'] = int(layer_in_w_t)

						layer_latency, layer_dsp_eff = layer_latency_est(hw_sw_params)
						if layer_latency < opt_layer_latency:
							opt_layer_latency = layer_latency
							opt_layer_eff = layer_dsp_eff
							opt_layer_in_num_t = layer_in_num_t
							opt_layer_out_num_t = layer_out_num_t
							opt_layer_in_h_t = layer_in_h_t
							opt_layer_in_w_t = layer_in_w_t

		layer_opt_params[layer_name]['LAYER_IN_NUM_T'] = opt_layer_in_num_t
		layer_opt_params[layer_name]['LAYER_OUT_NUM_T'] = opt_layer_out_num_t
		layer_opt_params[layer_name]['LAYER_IN_H_T'] = opt_layer_in_h_t
		layer_opt_params[layer_name]['LAYER_IN_W_T'] = opt_layer_in_w_t
		layer_opt_params[layer_name]['LAYER_DSP_EFF'] = opt_layer_eff
		layer_in_num_t_list[layer_id] = opt_layer_in_num_t
		layer_out_num_t_list[layer_id] = opt_layer_out_num_t
		layer_in_h_t_list[layer_id] = opt_layer_in_h_t
		layer_in_w_t_list[layer_id] = opt_layer_in_w_t
		layer_dsp_eff_list[layer_id] = opt_layer_eff
		latency += opt_layer_latency

	hw_sw_params['LAYER_IN_NUM_T_LIST'] = layer_in_num_t_list
	hw_sw_params['LAYER_OUT_NUM_T_LIST'] = layer_out_num_t_list
	hw_sw_params['LAYER_IN_H_T_LIST'] = layer_in_h_t_list
	hw_sw_params['LAYER_IN_W_T_LIST'] = layer_in_w_t_list
	hw_sw_params['LAYER_DSP_EFF_LIST'] = layer_dsp_eff_list
	hw_sw_params['MODEL_FAIL'] = model_fail
	return latency, hw_sw_params
















	# 	main_input_name = layer_config['MAIN_INPUT'][0]
	# 	if main_input_name in layers_order:
	# 		main_input_layer = layer_configs[main_input_name]
	# 	else:
	# 		main_input_name = 'error'
	# 		main_input_layer = -1

	# 	main_output_visited = False
	# 	main_output_layer = None
	# 	for main_output_name in layer_config['MAIN_OUTPUT']:
	# 		if visited_layers[main_output_name]:
	# 			main_output_visited = True
	# 			main_output_layer = layer_configs[main_output_name]
	# 			break

	# 	if len(layer_config['SECONDARY_INPUT']) > 0:
	# 		secondary_input_name = layer_config['SECONDARY_INPUT'][0]
	# 		secondary_input_layer = layer_configs[secondary_input_name]
	# 	else:
	# 		secondary_input_name = 'error'
	# 		secondary_input_layer = -1

	# 	secondary_output_visited = False
	# 	secondary_output_layer = None
	# 	for secondary_output_name in layer_config['SECONDARY_OUTPUT']:
	# 		if visited_layers[secondary_output_name]:
	# 			secondary_output_visited = True
	# 			secondary_output_layer = layer_configs[secondary_output_name]
	# 			break

	# 	if dynamic_tiling_level == 0:
	# 		layer_in_num_t_candidates = [in_num_hw]
	# 		layer_out_num_t_candidates = [out_num_hw]
	# 	else:

	# 		#if prev is visited, in_num_t must be same as prev layer out_num_t
	# 		if main_input_name != 'error' and visited_layers[main_input_name] == True:
	# 			layer_in_num_t_candidates = [main_input_layer['LAYER_OUT_NUM_T']]
	# 		else:
	# 			layer_in_num_t_candidates = list(filter(lambda x : x % sa_simd == 0, range(1, min(in_num_hw, layer_in_num) + 1)))
			
	# 		if secondary_input_name != 'error' and visited_layers[secondary_input_name] == True:
	# 			layer_out_num_t_candidates = [secondary_input_layer['LAYER_OUT_NUM_T']]
	# 		elif main_output_visited:
	# 			layer_out_num_t_candidates = [main_output_layer['LAYER_IN_NUM_T']]
	# 		elif secondary_output_visited:
	# 			layer_out_num_t_candidates = [secondary_output_layer['LAYER_IN_OUT_T']]
	# 		else:
	# 			layer_out_num_t_candidates = [5]#list(filter(lambda x : x % sa_simd == 0, range(1, min(out_num_hw, layer_out_num) + 1)))

	# 	if dynamic_tiling_level == 0 or dynamic_tiling_level == 1:
	# 		layer_in_h_t_candidates = [in_h_hw]
	# 		layer_in_w_t_candidates = [in_w_hw]
	# 	else:
	# 		layer_in_h_t_candidates = [8]
	# 		layer_in_w_t_candidates = list(filter(lambda x : x % sa_cols == 0 and layer_in_w % x == 0, range(1, min(in_w_hw, layer_in_w) + 1)))
		
	# 	# print('layer_in_num_t_candidates:', layer_in_num_t_candidates)
	# 	print('layer_out_num_t_candidates:', layer_out_num_t_candidates)
	# 	# print('layer_in_h_t_candidates:', layer_in_h_t_candidates)
	# 	# print('layer_in_w_t_candidates:', layer_in_w_t_candidates)
	# 	# exit()
	# # 	# print(layer_in_w_t_candidates)
	# # 	opt_layer_latency = np.inf
	# 	# for layer_in_h_t in layer_in_h_t_candidates:
	# 	# 	for layer_in_w_t in layer_in_w_t_candidates:
	# 	# 			for layer_in_num_t in layer_in_num_t_candidates:
	# 	# 				for layer_out_num_t in layer_out_num_t_candidates:
	# 	# 					layer_params['LAYER_IN_NUM_T'] = layer_in_num_t
	# 	# 					layer_params['LAYER_OUT_NUM_T'] = layer_out_num_t
	# 	# 					layer_params['LAYER_IN_H_T'] = layer_in_h_t
	# 	# 					layer_params['LAYER_IN_W_T'] = layer_in_w_t
	# 	# 					layer_params['LAYER_OUT_H_T'] = int(layer_in_h_t*exp_factor/stride)
	# 	# 					layer_params['LAYER_OUT_W_T'] = int(layer_in_w_t*exp_factor/stride)
	# 	# 					layer_latency = layer_latency_est(layer_params)
	# 	# 					if layer_latency < opt_layer_latency:
	# 	# 						opt_layer_latency = layer_latency
	# 	# 						opt_layer_in_num_t = layer_in_num_t
	# 	# 						opt_layer_out_num_t = layer_out_num_t
	# 	# 						opt_layer_in_h_t = layer_in_h_t
	# 	# 						opt_layer_in_w_t = layer_in_w_t

	# # 	layer_opt_params[layer_name]['LAYER_IN_NUM_T'] = opt_layer_in_num_t
	# # 	layer_opt_params[layer_name]['LAYER_OUT_NUM_T'] = opt_layer_out_num_t
	# # 	layer_opt_params[layer_name]['LAYER_IN_H_T'] = opt_layer_in_h_t
	# # 	layer_opt_params[layer_name]['LAYER_IN_W_T'] = opt_layer_in_w_t
	# # 	layer_in_num_t_list[layer_id] = opt_layer_in_num_t
	# # 	layer_out_num_t_list[layer_id] = opt_layer_out_num_t
	# # 	layer_in_h_t_list[layer_id] = opt_layer_in_h_t
	# # 	layer_in_w_t_list[layer_id] = opt_layer_in_w_t
	# # 	latency += opt_layer_latency
	
	# # params['LAYER_IN_NUM_T_LIST'] = layer_in_num_t_list
	# # params['LAYER_OUT_NUM_T_LIST'] = layer_out_num_t_list
	# # params['LAYER_IN_H_T_LIST'] = layer_in_h_t_list
	# # params['LAYER_IN_W_T_LIST'] = layer_in_w_t_list
	# # in_num_t = params['LAYER_IN_NUM_T']
	# # out_num_t = params['LAYER_OUT_NUM_T']
	# # in_h_t = params['LAYER_IN_H_T']
	# # in_w_t = params['LAYER_IN_W_T']
	# exit()
	# return 0

# def layer_latency_est(params, test_en=False):
# 	#generated by greedy search in model_est
# 	in_num_t = params['LAYER_IN_NUM_T']
# 	out_num_t = params['LAYER_OUT_NUM_T']
# 	in_h_t = params['LAYER_IN_H_T']
# 	in_w_t = params['LAYER_IN_W_T']
# 	out_h_t = in_h_t*params['UPSAMPLE_FACTOR']
# 	out_w_t = in_w_t*params['UPSAMPLE_FACTOR']

# 	#hardware candidates
# 	sa_rows = params['SA_ROWS']
# 	sa_cols = params['SA_COLS']
# 	sa_simd = params['SA_SIMD']
# 	lane = params['LANE']
# 	fre = params['FRE']
# 	dw0 = params['DATA_W0']
# 	dw1 = params['DATA_W1']
# 	dw2 = params['DATA_W2']
# 	port_width = params['BUS_W']

# 	#data from extract_info (model)
# 	inst_in_num = params['LAYER_IN_NUM']
# 	# if inst_in_num==0:
# 	# 	return 1000, (0,'')
# 	inst_out_num = params['LAYER_OUT_NUM']
# 	inst_in_h = params['LAYER_IN_H']
# 	inst_in_w = params['LAYER_IN_W']
# 	inst_out_h = params['LAYER_OUT_H']
# 	inst_out_w = params['LAYER_OUT_W']

# 	BatchNormalization_latency = []
# 	LeakyRelu_latency = []
# 	Conv_latency = []
# 	Add_latency = []
# 	MaxPool_latency = []

# 	in_num_tiles = np.ceil(inst_in_num / in_num_t)
# 	out_num_tiles = np.ceil(inst_out_num / out_num_t)
# 	in_h_tiles = np.ceil(inst_in_h / in_h_t)
# 	in_w_tiles = np.ceil(inst_in_w / in_w_t)
	
# 	# print(in_num_tiles, out_num_tiles, in_h_tiles, in_w_tiles)
# 	conv_en = 0
# 	conv_idx = 0
# 	cin_kernel_h = 1
# 	cin_kernel_w = 1
# 	conv_kernel_h = 1
# 	conv_kernel_w = 1
# 	stride = 1
# 	for op in params['ops']:
# 		op_params = op['params']
# 		if op_params['en']:

# 			if op['name'] == 'Conv':
# 				conv_en = 1
# 				conv_idx = params['ops'].index(op)
# 				tstride = op_params['tstride']
# 				cin_kernel_h = op_params['kernel_h']
# 				cin_kernel_w = op_params['kernel_w']
# 				conv_kernel_h = op_params['kernel_h']
# 				conv_kernel_w = op_params['kernel_w']

# 			elif  op['name'] == 'ConvTranspose':
# 				conv_en = 1
# 				conv_idx = params['ops'].index(op)
# 				tstride = op_params['tstride']
# 				cin_kernel_h = op_params['kernel_h']
# 				cin_kernel_w = op_params['kernel_w']
# 				conv_kernel_h = op_params['kernel_h']
# 				conv_kernel_w = op_params['kernel_w']
# 				cin_kernel_h = int(np.ceil(cin_kernel_h/tstride))
# 				cin_kernel_w = int(np.ceil(cin_kernel_w/tstride))
			
# 			elif op['name'] == 'MaxPool':
# 				cin_kernel_h = op_params['kernel_h']
# 				cin_kernel_w = op_params['kernel_w']
# 				conv_kernel_h = 1
# 				conv_kernel_w = 1

	
# 	cin_load_latency, cin_bottleneck = cin_load_est(in_num_t, in_h_t, in_w_t, cin_kernel_h, cin_kernel_w, lane, dw0, port_width, fre, test_en)
# 	weight_load_latency = weight_load_est(in_num_t, out_num_t, 1, 1, conv_kernel_h, conv_kernel_w, lane, dw0, dw1, dw2, port_width, conv_en, 0, 0, fre)
# 	if cin_kernel_h==1 and cin_kernel_w==1:
# 		cin_load_latency = 0
# 	# print(in_num_t, out_num_t, in_h_t, in_w_t, out_h_t, out_w_t)
# 	# conv_en = 0
# 	after_conv = False
# 	for idx, op in enumerate(params['ops']):
# 		op_params = op['params']
# 		if op['name'] == 'Conv' or op['name'] == 'ConvTranspose':
# 			after_conv = True
# 		if op_params['en']:
# 			if op['name'] == 'BatchNormalization':
# 				if after_conv:
# 					op_num_t = out_num_t
# 					op_h_t = out_h_t
# 					op_w_t = out_w_t
# 				else:
# 					op_num_t = in_num_t
# 					op_h_t = in_h_t
# 					op_w_t = in_w_t
# 				# print(op['name'],op_num_t, op_h_t, op_w_t)
# 				BatchNormalization_latency.append(0)
# 			elif op['name'] == 'LeakyRelu':
# 				if after_conv:
# 					op_num_t = out_num_t
# 					op_h_t = out_h_t
# 					op_w_t = out_w_t
# 				else:
# 					op_num_t = in_num_t
# 					op_h_t = in_h_t
# 					op_w_t = in_w_t
# 				# print(op['name'],op_num_t, op_h_t, op_w_t)
# 				latency = relu_est(in_num_tiles, op_num_t, op_h_t, op_w_t, lane, after_conv)
# 				LeakyRelu_latency.append(0)
# 			elif op['name'] == 'Conv' or op['name'] == 'ConvTranspose':
# 				op_in_num_t = in_num_t
# 				op_out_num_t = out_num_t
# 				op_in_h_t = in_h_t
# 				op_in_w_t = in_w_t
# 				op_out_h_t = out_h_t
# 				op_out_w_t = out_w_t
# 				op_in_num = op_params['in_channels']
# 				op_out_num = op_params['out_channels']
# 				op_in_h = op_params['in_h']
# 				op_in_w = op_params['in_w']
# 				conv_kernel_h = op_params['kernel_h']
# 				conv_kernel_w = op_params['kernel_w']
# 				stride = op_params['stride']
# 				tstride = op_params['tstride']
# 				in_h_t *= tstride
# 				in_w_t *= tstride
# 				out_h_t *= tstride
# 				out_w_t *= tstride
# 				# print(op['name'],op_in_num_t, op_out_num_t, op_in_h_t, op_in_w_t, op_out_h_t, op_out_w_t)

# 				latency, conv_bottleneck = conv_est(op_in_num, op_in_num_t, op_out_num_t, op_in_h_t, op_in_w_t, op_out_h_t, op_out_w_t, conv_kernel_h, conv_kernel_w, stride, tstride, lane, sa_rows, sa_cols, sa_simd)
# 				Conv_latency.append(latency)
# 			elif op['name'] == 'Add':
# 				if after_conv:
# 					op_num_t = out_num_t
# 					op_h_t = out_h_t
# 					op_w_t = out_w_t
# 				else:
# 					op_num_t = in_num_t
# 					op_h_t = in_h_t
# 					op_w_t = in_w_t
# 				# print(op['name'],op_num_t, op_h_t, op_w_t)
# 				Add_latency.append(0)
# 			elif op['name'] == 'MaxPool':
# 				stride = op_params['stride']
# 				if after_conv:
# 					op_num_t = out_num_t
# 					op_h_t = out_h_t//stride
# 					op_w_t = out_w_t//stride
# 				else:
# 					op_num_t = in_num_t
# 					op_h_t = in_h_t//stride
# 					op_w_t = in_w_t//stride
# 				in_h_t //= stride
# 				in_w_t //= stride
# 				out_h_t //= stride
# 				out_w_t //= stride
# 				# print(op['name'],op_num_t, op_h_t, op_w_t)
# 				latency = pool_est(in_num_tiles, op_num_t, op_h_t, op_w_t, lane, after_conv)
# 				MaxPool_latency.append(latency)
# 	# print(in_num_t, out_num_t, in_h_t, in_w_t, out_h_t, out_w_t)
# 	# print()
# 	cout_write_latency, cout_bottleneck = cout_write_est(in_num_tiles, out_num_t, out_h_t, out_w_t, stride, lane, dw0, port_width, fre, test_en)
# 	if cin_kernel_h==1 and cin_kernel_w==1:
# 		cout_write_latency = 0
# 	# if(params['LAYER_ID'] == 3):
# 	# 	print('cin_load_latency:', cin_load_latency)
# 	# 	print('BatchNormalization_latency:', BatchNormalization_latency)
# 	# 	print('LeakyRelu_latency:', LeakyRelu_latency)
# 	# 	print('Conv_latency:', Conv_latency)
# 	# 	print('Add_latency:', Add_latency)
# 	# 	print('MaxPool_latency:', MaxPool_latency)
# 	# 	print('cout_write_latency:', cout_write_latency)

# 	cin_load_latency = [cin_load_latency]
# 	weight_load_latency = [weight_load_latency]
# 	cout_write_latency = [cout_write_latency]
	

# 	stage_latency = max(cin_load_latency + \
# 									 		weight_load_latency + \
# 											BatchNormalization_latency + \
# 											LeakyRelu_latency + \
# 											Conv_latency + \
# 											Add_latency + \
# 											MaxPool_latency + \
# 										 	cout_write_latency)

# 	if conv_en:
# 		total_iter = in_num_tiles*out_num_tiles*in_h_tiles*in_w_tiles
# 	else:
# 		total_iter = in_num_tiles*in_h_tiles*in_w_tiles
# 	if conv_en:
# 		conv_latency = Conv_latency[0]
# 	else:
# 		conv_latency = 0
# 	extra_latency = max(cin_load_latency + weight_load_latency) + cout_write_latency[0] + conv_latency# the data drain latency is omitted
# 	layer_latency = extra_latency + stage_latency * total_iter

# 	peak_perf = sa_cols*sa_rows*sa_simd #accelerator peak performance MACs/cycle
# 	layer_macs = 0
# 	layer_ideal_latency = 0
# 	if conv_en == 1:
# 		layer_macs = params['MACS']
# 		layer_ideal_latency = layer_macs/peak_perf
	
# 	bottleneck = ''
# 	if cin_load_latency[0] == stage_latency:
# 		bottleneck = 'cin_load'+'('+cin_bottleneck+')'
# 	elif weight_load_latency[0] == stage_latency:
# 		bottleneck = 'weight_load'
# 	elif cout_write_latency[0] == stage_latency:
# 		bottleneck = 'cout_write'+'('+cout_bottleneck+')'
# 	elif conv_latency == stage_latency:
# 		bottleneck = 'conv'+'('+conv_bottleneck+')'
# 	# else:
# 	# 	bottleneck = 'unknown'
# 	dsp_efficiency = (100*layer_ideal_latency/layer_latency, bottleneck)
# 	# print('{} {} {} {}'.format(in_num_t, out_num_t, in_h_t, in_w_t), layer_latency/234e6)
# 	return layer_latency, dsp_efficiency