import numpy as np
import json
import argparse
import copy
import multiprocessing
import subprocess
import time
import pandas as pd
import math
import networkx as nx
import sys
from latency_est import model_latency_est, layer_latency_est
from resource_est import res_est
from utils import sort_by_key_and_return_list, sort_by_second_element, get_first_element, list_split, sort_by_key, print_layers_details, print_result
import random
from collections import OrderedDict

def get_layers_configs(f_model, arch_modules):
	
	#get CNN layer configs
	model = open(f_model, "r")
	modelLines = model.readlines()
	input_shape = eval(modelLines[0])

	# input info
	network_in_num = input_shape["IN_NUM"]
	network_in_h = input_shape["IN_H"]
	network_in_w = input_shape["IN_W"]

	in_h = network_in_h
	in_w = network_in_w
	layer_configs = OrderedDict()
	layer_1_info = eval(modelLines[1])
	input_name = layer_1_info['main_input'][0]
	for i in range(1, len(modelLines)):
		line = modelLines[i]
		layer_info = eval(line)
		layer_name = layer_info['name']
		main_input = layer_info['main_input']
		secondary_input = layer_info['secondary_input']
		downsample_factor = layer_info['downsample_factor']
		upsample_factor = layer_info['upsample_factor']
		in_num = layer_info['in_num']
		out_num = layer_info['out_num']

		min_prev_h = np.inf
		min_prev_w = np.inf

		for prev_name in main_input:
			if prev_name != input_name:
				prev_layer_config = layer_configs[prev_name]
				if prev_layer_config['LAYER_OUT_H'] < min_prev_h:
					min_prev_h = prev_layer_config['LAYER_OUT_H']
				if prev_layer_config['LAYER_OUT_W'] < min_prev_w:
					min_prev_w = prev_layer_config['LAYER_OUT_W']

		#infer remaining network data
		in_h = network_in_h if input_name in main_input  else min_prev_h
		in_w = network_in_w if input_name in main_input  else min_prev_w
		out_h = int(math.ceil(in_h*upsample_factor/downsample_factor))
		out_w = int(math.ceil(in_w*upsample_factor/downsample_factor))
		layer_config = {}

		layer_config['LAYER_NAME'] = layer_name
		layer_config['OUTPUT_NAME'] = layer_info['output_name']
		layer_config['LAYER_ID'] = i-1
		layer_config['LAYER_IN_NUM'] = in_num
		layer_config['LAYER_OUT_NUM'] = out_num
		layer_config['LAYER_IN_H'] = in_h
		layer_config['LAYER_IN_W'] = in_w
		layer_config['LAYER_OUT_H'] = out_h
		layer_config['LAYER_OUT_W'] = out_w
		layer_config['UPSAMPLE_FACTOR'] = upsample_factor
		layer_config['DOWNSAMPLE_FACTOR'] = downsample_factor
		layer_config['MAIN_INPUT'] = main_input
		layer_config['SECONDARY_INPUT'] = secondary_input
		layer_config['MAIN_OUTPUT'] = []
		layer_config['SECONDARY_OUTPUT'] = []

		for idx, en in enumerate(layer_info['en']):
			layer_config[arch_modules[idx]+'_en'] = en

		filter_h = 0
		filter_w = 0
		stride = 0
		layer_config['ops'] = layer_info['ops']
		curr_in_h = in_h
		curr_in_w = in_w
		inst_macs = 0
		inst_macs_with_zeros = 0
		n_conv_macs = 0
		d_conv_macs = 0
		t_conv_macs = 0
		a_conv_macs = 0
		inst_gops = 0
		add_gops = 0
		conv_type = ''
		for op in layer_config['ops']:
			op_params = op['params']
			op_upsample_factor = op_params['upsample_factor']
			op_downsample_factor = op_params['downsample_factor']
			op_params['in_h'] = curr_in_h
			op_params['in_w'] = curr_in_w
			op_params['out_h'] = int(op_params['in_h']*op_upsample_factor/op_downsample_factor)
			op_params['out_w'] = int(op_params['in_w']*op_upsample_factor/op_downsample_factor)
			curr_in_h = op_params['out_h']
			curr_in_w = op_params['out_w']
			if op_params['en']:
				if op['name'] == 'Conv':
					filter_h = op_params['kernel_h']
					filter_w = op_params['kernel_w']
					inst_macs += op_params['in_channels']*op_params['out_channels']*op_params['kernel_h']*op_params['kernel_w']*op_params['out_h']*op_params['out_w']
					inst_gops += 2*inst_macs
					tstride = op_params['tstride']
					stride = op_params['stride']
					dilation_rate = op_params['dilation_rate']
					dilated_kernel_h = dilation_rate*op_params['kernel_h']-dilation_rate+1
					dilated_kernel_w = dilation_rate*op_params['kernel_w']-dilation_rate+1
					padding_h = dilated_kernel_h - stride
					padding_w = dilated_kernel_w - stride
					inst_macs_with_zeros += op_params['in_channels']*op_params['out_channels']*dilated_kernel_h*dilated_kernel_w*op_params['out_h']*op_params['out_w']
					if op_params['type'] == 'DConv':
						conv_type = 'DConv'
						d_conv_macs += op_params['in_channels']*op_params['out_channels']*op_params['kernel_h']*op_params['kernel_w']*op_params['out_h']*op_params['out_w']
					elif op_params['type'] == 'NConv':
						conv_type = 'NConv'
						if op_params['kernel_h'] == op_params['kernel_w']:
							n_conv_macs += op_params['in_channels']*op_params['out_channels']*op_params['kernel_h']*op_params['kernel_w']*op_params['out_h']*op_params['out_w']
						else:
							a_conv_macs += op_params['in_channels']*op_params['out_channels']*op_params['kernel_h']*op_params['kernel_w']*op_params['out_h']*op_params['out_w']
					break
				elif op['name'] == 'ConvTranspose':
					conv_type = 'TConv'
					inst_macs += op_params['in_channels']*op_params['out_channels']*op_params['kernel_h']*op_params['kernel_w']*op_params['in_h']*op_params['in_w']
					inst_gops += 2*inst_macs
					tstride = op_params['tstride']
					filter_h = op_params['kernel_h'] 
					filter_w = op_params['kernel_w']
					stride = op_params['stride']
					padding_h = filter_h - tstride
					padding_w = filter_w - tstride
					inst_macs_with_zeros += op_params['in_channels']*op_params['out_channels']*op_params['kernel_h']*op_params['kernel_w']*op_params['out_h']*op_params['out_w']
					t_conv_macs += op_params['in_channels']*op_params['out_channels']*op_params['kernel_h']*op_params['kernel_w']*op_params['in_h']*op_params['in_w']
					break
				elif op['name'] == 'Add':
					add_gops += op_params['out_channels']*op_params['out_h']*op_params['out_w']
					padding_h = 0
					padding_w = 0
					break
				elif 'kernel_h' in op_params and 'kernel_w' in op_params and 'stride' in op_params:
					filter_h = 0#op_params['kernel_h']
					filter_w = 0#op_params['kernel_w']
					stride = op_params['stride']
					padding_h = filter_h - stride
					padding_w = filter_w - stride
					break

		layer_config['CONV_TYPE'] = conv_type
		layer_config['MACS'] = inst_macs
		layer_config['MACS_ZEROS'] = inst_macs_with_zeros
		layer_config['NCONV_MACS'] = n_conv_macs
		layer_config['DCONV_MACS'] = d_conv_macs
		layer_config['TCONV_MACS'] = t_conv_macs
		layer_config['ACONV_MACS'] = a_conv_macs
		layer_config['ADD_GOPS'] = add_gops
		layer_config['FILTER_H'] = filter_h
		layer_config['FILTER_W'] = filter_w
		layer_config['PAD_H'] = padding_h
		layer_config['PAD_W'] = padding_w
		layer_config['IN_DATA_LAYOUT'] = 2 if (((filter_h-stride) == 0 and (filter_w-stride) == 0) or (filter_h==tstride and filter_w==tstride)) else 1
		layer_config['OUT_DATA_LAYOUT'] = 1
		layer_config['IN_W_T_DEPEND'] = []
		layer_config['IN_NUM_DEPEND'] = []
		layer_config['OUT_NUM_DEPEND'] = []
		layer_configs[layer_name] = layer_config
	
		if(main_input[0] in list(layer_configs.keys())):
			layer_configs[main_input[0]]['MAIN_OUTPUT'].append(layer_name)
			# layer_configs[main_input[0]]['OUT_DATA_LAYOUT'].append(layer_config['IN_DATA_LAYOUT'])
			# filter_h = 0
			# filter_w = 0
			# stride = 0
			# for op in layer_configs[main_input[0]]['ops']:
			# 	op_params = op['params']
			# 	if op_params['en']:
			# 		if 'kernel_h' in op_params and 'kernel_w' in op_params and 'stride' in op_params:
			# 			filter_h = op_params['kernel_h']
			# 			filter_w = op_params['kernel_w']
			# 			stride = op_params['stride']
			# 			if filter_h - stride == 0 and filter_w - stride == 0:
			# 				layer_configs[main_input[0]]['OUT_DATA_LAYOUT'] = 1
			# 			else:
			# 				layer_configs[main_input[0]]['OUT_DATA_LAYOUT'] = 2
		if len(secondary_input) > 0:
			if(secondary_input[0] in list(layer_configs.keys())):
				layer_configs[secondary_input[0]]['SECONDARY_OUTPUT'].append(layer_name)
				# print(secondary_input[0])
				# if 'add' in secondary_input[0]:
				# 	layer_configs[secondary_input[0]]['OUT_DATA_LAYOUT'].append(2)
				# else:
				# 	layer_configs[secondary_input[0]]['OUT_DATA_LAYOUT'].append(layer_config['IN_DATA_LAYOUT'])


	in_num_depend_v = []
	out_num_depend_v = []
	for layer_name in layer_configs:
		layer_config = layer_configs[layer_name]
		main_output_layer_names = layer_config['MAIN_OUTPUT']
		for main_output_name in main_output_layer_names:
			main_output_config = layer_configs[main_output_name]
			in_num_depend_v.append(('in', main_output_config['LAYER_NAME']))
			out_num_depend_v.append(('out', layer_config['LAYER_NAME']))
		if(layer_config['MaxPool_en'] == 1):
			in_num_depend_v.append(('in', layer_config['LAYER_NAME']))
			out_num_depend_v.append(('out', layer_config['LAYER_NAME']))
		if(len(layer_config['SECONDARY_INPUT'])>0):
			in_num_depend_v.append(('out', layer_config['SECONDARY_INPUT'][0]))
			out_num_depend_v.append(('out', layer_config['LAYER_NAME']))

	# # in_w_t dependency
	# in_w_t_depend_v = []
	# out_w_t_depend_v = []
	for idx, layer_name in enumerate(layer_configs):
		layer_config = layer_configs[layer_name]
		downsample_factor = layer_config['DOWNSAMPLE_FACTOR']
		main_output_name = 0
		if(len(layer_config['MAIN_OUTPUT'])>0):
			for main_output_name in layer_config['MAIN_OUTPUT']:
			# main_output_name = layer_config['MAIN_OUTPUT'][0]
				main_output_config = layer_configs[main_output_name]
				main_output_layout = main_output_config['IN_DATA_LAYOUT']
				# main_output_downsample_factor = main_output_config['DOWNSAMPLE_FACTOR']
				# print(main_output_name, main_output_downsample_factor)
				if(main_output_layout == 2):
					# print(idx, layer_config['MAIN_OUTPUT'], main_output_config['DATA_LAYOUT'])
					main_output_config['IN_W_T_DEPEND'].append(layer_name)

		secondary_input_name = 0
		if(len(layer_config['SECONDARY_OUTPUT'])>0):
			for secondary_output_name in layer_config['SECONDARY_OUTPUT']:
			# secondary_output_name = layer_config['SECONDARY_OUTPUT'][0] 
				secondary_output_config = layer_configs[secondary_output_name]
				secondary_output_layout = secondary_output_config['IN_DATA_LAYOUT']
				# secondary_output_downsample_factor = secondary_output_config['DOWNSAMPLE_FACTOR']
				# print(secondary_output_name, secondary_output_downsample_factor)
				if(secondary_output_layout == 2):
					secondary_output_config['IN_W_T_DEPEND'].append(layer_name)
	
	layers_order = list(layer_configs.keys())

	G = nx.Graph()
	for layer_name in layer_configs:
		layer_config = layer_configs[layer_name]
		G.add_node(layer_name)
		for in_w_t_depend in layer_config['IN_W_T_DEPEND']:
			G.add_edge(in_w_t_depend, layer_name)
	w_t_components = list(nx.connected_components(G))
	for component in w_t_components:
		# sort component by layers order
		component = sorted(component, key=lambda x: layers_order.index(x))

	for layer_name in layer_configs:
		layer_config = layer_configs[layer_name]
		for component in w_t_components:
			# sort component by layers order
			component = sorted(component, key=lambda x: layers_order.index(x))
			if layer_name in component:
				layer_config['IN_W_T_DEPEND'] = list(component)
				break
			
	if 'UNet' in f_model or 'VGG16' in f_model:
		for layer_name in layer_configs:
			layer_config = layer_configs[layer_name]
			layer_config['IN_W_T_DEPEND'] = []
	G = nx.Graph()
	G.add_edges_from(zip(out_num_depend_v, in_num_depend_v))
	# get disconnected components
	num_t_components = list(nx.connected_components(G))
	# sort list
	num_t_components = sorted(num_t_components)
	direction_order = ['out', 'in']
	# for component in num_t_components:
	# 	component = list(component)
	# 	# sort component by layers order
	# 	component = sorted(component, key=lambda x: layers_order.index(x[1]))
	# 	# sort component by direction
	# 	component = sorted(component, key=lambda x: direction_order.index(x[0]))

	for layer_name in layer_configs:
		layer_config = layer_configs[layer_name]
		layer_config['IN_NUM_DEPEND'] = []
		layer_config['OUT_NUM_DEPEND'] = []
		for component in num_t_components:
			component = list(component)
			# sort component by layers order
			component = sorted(component, key=lambda x: layers_order.index(x[1]))
			# sort component by direction
			component = sorted(component, key=lambda x: direction_order.index(x[0]))
			if ('in', layer_name) in component:
				layer_config['IN_NUM_DEPEND'] = list(component)
			if ('out', layer_name) in component:
				layer_config['OUT_NUM_DEPEND'] = list(component)
	# for layer_name in layer_configs:
	# 	layer_config = layer_configs[layer_name]
	# 	print(layer_config['IN_NUM_DEPEND'])
	# 	print(layer_config['OUT_NUM_DEPEND'])
	# 	print(layer_config['IN_W_T_DEPEND'])
	# exit()
	return layer_configs, input_shape, 0, 0

'''
sweep each layer, pick up the optimal in_num_t/out_num_t, in_h_t, in_w_t
SA_config is a list of [SA_ROWS, SA_COLS, SA_SIMD]
'''

def run(f_model_in, f_model_out, f_board, f_arch_in, f_arch_out, parallel_en, systolic_en, dynamic_tiling_level, solver_en, f_output, dw, test):
	dse_report_file = open(f_output+'.txt', 'w')
	detailed_dse_report_file = open(f_output + '_detailed.csv', 'w')
	# print("************************ "+str(test)+" *************************")

	# record start time
	global_timer_start = time.time()

	with open(f_board, "r") as f:
		board_info = json.loads(f.read())

	config = {}
	config['BOARD'] = board_info
	config['DYNAMIC_TILING_LEVEL'] = dynamic_tiling_level
	# print('Dynamic tiling level: ', dynamic_tiling_level)

	#get hw architecture modules
	arch_modules = eval(open(f_arch_in, "r").read())

	layer_configs, input_shape, num_t_components, w_t_components = get_layers_configs(f_model_in, arch_modules)
	for layer_name in layer_configs:
		layer_config = layer_configs[layer_name]
		in_num = layer_config['LAYER_IN_NUM']
		out_num = layer_config['LAYER_OUT_NUM']
		in_h = layer_config['LAYER_IN_H']
		in_w = layer_config['LAYER_IN_W']
		out_h = layer_config['LAYER_OUT_H']
		out_w = layer_config['LAYER_OUT_W']
		fh = layer_config['FILTER_H']
		fw = layer_config['FILTER_W']
		in_dl = layer_config['IN_DATA_LAYOUT']
		in_num_depend = layer_config['IN_NUM_DEPEND']
		out_num_depend = layer_config['OUT_NUM_DEPEND']
		in_w_depend = layer_config['IN_W_T_DEPEND']
		# print(in_num_depend)
		# print(out_num_depend)
		# print(in_w_depend)
		# out_dl = layer_config['OUT_DATA_LAYOUT']
		# print(in_num, out_num, in_h, in_w, out_h, out_w, fh, fw, in_dl)
		# print(in_dl)
		# print()

	# exit()
	layers_order = list(layer_configs.keys())

	# for layer_name in layers_order:
	# 	layer_config = layer_configs[layer_name]
	# 	print(layer_config['LAYER_IN_NUM'], layer_config['LAYER_OUT_NUM'], layer_config['LAYER_IN_H'], layer_config['LAYER_IN_W'], layer_config['LAYER_OUT_H'], layer_config['LAYER_OUT_W'])
	# exit()
	total_macs = sum([layer_configs[name]['MACS'] for name in layer_configs])
	total_macs_with_zeros = sum([layer_configs[name]['MACS_ZEROS'] for name in layer_configs])
	n_conv_macs = sum([layer_configs[name]['NCONV_MACS'] for name in layer_configs])
	d_conv_macs = sum([layer_configs[name]['DCONV_MACS'] for name in layer_configs])
	t_conv_macs = sum([layer_configs[name]['TCONV_MACS'] for name in layer_configs])
	a_conv_macs = sum([layer_configs[name]['ACONV_MACS'] for name in layer_configs])
	add_gops = sum([layer_configs[name]['ADD_GOPS'] for name in layer_configs])
	# print('Total MACs: ', total_macs)
	print('Total GOPs: ', total_macs*2/1e9)
	# print('Paper GOPs: ', 1403508771/1e9)
	# print('Total MACs with zeros: ', total_macs_with_zeros)
	# print('Total GOPs with zeros: ', total_macs_with_zeros*2)
	# print('Total MACs: ', total_macs, file=dse_report_file)
	# print('Total GOPs: ', total_macs*2, file=dse_report_file)
	# print('NCONV GOPs: ', n_conv_macs*2/1e9)
	# print('DCONV GOPs: ', d_conv_macs*2/1e9)
	# print('TCONV GOPs: ', t_conv_macs*2/1e9)
	# print('ACONV GOPs: ', a_conv_macs*2/1e9)
	# print('CONV GOPs: ', (n_conv_macs*2 + d_conv_macs*2 + t_conv_macs*2 + a_conv_macs*2)/1e9)
	# print('ADD GOPs: ', total_macs*2/1e9 + add_gops/1e9)
	# exit()

	#shuffle the order of layers
	# sorted = sort_by_key_and_return_list(layer_configs, 'LAYER_ID', reverse=False)
	# for layer in sorted:
	# 	layers_order.append(layer['LAYER_NAME'])
	# exit()
	max_in_h = max(layer_configs[layer_name]['LAYER_IN_H'] for layer_name in layer_configs)
	max_in_w = max(layer_configs[layer_name]['LAYER_IN_W'] for layer_name in layer_configs)
	max_in_num = max(layer_configs[layer_name]['LAYER_IN_NUM'] for layer_name in layer_configs)
	max_out_num = max(layer_configs[layer_name]['LAYER_OUT_NUM'] for layer_name in layer_configs)
	max_upsample_factor = max(layer_configs[layer_name]['UPSAMPLE_FACTOR'] for layer_name in layer_configs)
	max_downsample_factor = max(layer_configs[layer_name]['DOWNSAMPLE_FACTOR'] for layer_name in layer_configs)

	# Start the design space exploration
	# It works in a greedy fashion, as we will minimize the latency layer by layer.
	opt_latency = np.inf
	opt_DSP = np.inf
	opt_BRAM18K = np.inf
	opt_dsp_eff = 0
	opt_params = {}


	params = {}
	
	hw_params = {}
	"""
	Data Precision
	"""
	if dw==32:
		hw_params['DATA_W0'] = 32
		hw_params['DATA_W1'] = 32
		hw_params['DATA_W2'] = 32
		hw_params['BUS_W'] = 512
		hw_params['DATA_T0'] = "float"
		hw_params['DATA_T1'] = "float"
		hw_params['DATA_T2'] = "float"
	elif dw==16:
		hw_params['DATA_W0'] = 16
		hw_params['DATA_W1'] = 16
		hw_params['DATA_W2'] = 16
		hw_params['BUS_W'] = 512
		hw_params['DATA_T0'] = "ap_fixed<16,8>"
		hw_params['DATA_T1'] = "ap_fixed<16,8>"
		hw_params['DATA_T2'] = "ap_fixed<16,8>"
	elif dw==8:
		hw_params['DATA_W0'] = 8
		hw_params['DATA_W1'] = 8
		hw_params['DATA_W2'] = 8
		hw_params['BUS_W'] = 512
		hw_params['DATA_T0'] = "ap_fixed<8,4>"
		hw_params['DATA_T1'] = "ap_fixed<8,4>"
		hw_params['DATA_T2'] = "ap_fixed<8,4>"
	"""
	Tiling Size
	"""
	hw_params['K_T'] = 16

	#TODO: Check those parameters
	#Buffer size candidates
	in_h_params = list(filter(lambda x : max_in_h % x == 0 and x % 2 == 0, range(1, 32 + 1)))
	in_w_params = list(filter(lambda x : max_in_w % x == 0 and x % 2 == 0, range(1, max_in_w + 1)))
	in_num_params = list(filter(lambda x : max_in_num % x == 0 and x % 2 == 0, range(1, max_in_num + 1)))
	out_num_params = list(filter(lambda x : max_out_num % x == 0 and x % 2 == 0, range(1, max_out_num + 1)))
	lane_params = [2,4,8,16,32,64] if systolic_en else [8] #list(filter(lambda x : max_in_num % x == 0 and x % 8 == 0, range(1, min(max_in_num, 8) + 1)))

	params = {}
	
	params.update(hw_params)
	params_list = []
	print("*************************************************")
	print('Generating design space...')
	print("*************************************************")
	# 16, 32, 8, 144
	# Construct the list of all different tiling factors
	for IN_H_T in in_h_params if systolic_en else [8]:
		for IN_W_T in in_w_params if systolic_en else [144]:
			for IN_NUM_T in in_num_params if systolic_en else [32]:
				for OUT_NUM_T in out_num_params if systolic_en else [32]:
					for LANE in lane_params:
						###################################################
						## Search through different systolic array sizes ##
						###################################################
						# Turn it off if you want to go with a predefined systolic array size
						sa_row_candidates = (list(filter(lambda x : OUT_NUM_T % x == 0, range(4, OUT_NUM_T + 1))) if systolic_en else [16])
						sa_col_candidates = (list(filter(lambda x : IN_W_T % x == 0, range(4, IN_W_T + 1))) if systolic_en else [6])
						sa_simd_candidates = [LANE]#(list(filter(lambda x : IN_NUM_T % x == 0, range(4, IN_NUM_T + 1))) if systolic_en else [8])
						for SA_ROWS in sa_row_candidates:
							for SA_COLS in sa_col_candidates:
								for SA_SIMD in sa_simd_candidates:
									params['LAYER_IN_H_T'] = IN_H_T
									params['LAYER_IN_W_T'] = IN_W_T
									params['LAYER_OUT_H_T'] = IN_H_T*max_upsample_factor #TODO: Check this for upsampling and downsampling
									params['LAYER_OUT_W_T'] = IN_W_T*max_upsample_factor #TODO: Check this for upsampling and downsampling
									params['LAYER_IN_NUM_T'] = IN_NUM_T
									params['LAYER_OUT_NUM_T'] = OUT_NUM_T
									params['LANE'] = SA_SIMD
									params['SA_ROWS'] = SA_ROWS
									params['SA_COLS'] = SA_COLS
									params['SA_SIMD'] = SA_SIMD
									tmp_params = dict(params)
									params_list.append(tmp_params)
	print('Generated %d design points' % len(params_list), file=dse_report_file)
	print('Generated %d design points' % len(params_list))
	layer_configs_pd = pd.DataFrame(layer_configs)
	# layer_configs_pd.to_csv("output_layer.csv")
	# exit()
	params_list_pd = pd.DataFrame(params_list)
	# params_list_pd.to_csv("params.csv")

	#return
	
	if parallel_en is True:
		num_processes = int(multiprocessing.cpu_count() * 0.9)
	else:
		num_processes = 1
	print('Parallelizing using %d processes...' % (num_processes))

	results = []
	chunks = list_split(params_list, num_processes)
	pool = multiprocessing.Pool(processes = num_processes)

	results = pool.starmap(param_sweep, [(chunk, config, layer_configs, layers_order, num_t_components, w_t_components, systolic_en, solver_en, test) for chunk in chunks])
	results = list(np.concatenate(results).flat)
	

	print('Aggregating results...')
	
	sorted_by_latency_results = sort_by_key(results, 'opt_latency')
	print('Sorted by latency:', len(sorted_by_latency_results))
	for result in sorted_by_latency_results:
		result['DIM_SUM'] = result['opt_params']['SA_ROWS'] + result['opt_params']['SA_COLS'] + result['opt_params']['SA_SIMD']
	# split results into groups with the same latency
	groups = []
	curr_latency = sorted_by_latency_results[0]['opt_latency']
	curr_group = []
	for result in sorted_by_latency_results:
		if result['opt_latency'] == curr_latency:
			curr_group.append(result)
		else:
			groups.append(curr_group)
			curr_group = []
			curr_group.append(result)
			curr_latency = result['opt_latency']
	groups.append(curr_group)

	# # count number of elements in all groups
	# elements_in_groups = 0
	# for group in groups:
	# 	elements_in_groups += len(group)
	# print(f'Number of groups: {len(groups)}, number of elements in groups: {elements_in_groups}')

	# sort each group by ['opt_params']['SA_SIMD']
	for group in groups:
		group.sort(key=lambda x: x['DIM_SUM'])
	# split each group into subgroups with the same DIM_SUM
	subgroups = []
	for group in groups:
		curr_sa_simd = group[0]['DIM_SUM']
		curr_subgroup = []
		for result in group:
			if result['DIM_SUM'] == curr_sa_simd:
				curr_subgroup.append(result)
			else:
				subgroups.append(curr_subgroup)
				curr_subgroup = []
				curr_subgroup.append(result)
				curr_sa_simd = result['DIM_SUM']
		subgroups.append(curr_subgroup)

	# # count number of elements in all subgroups
	# elements_in_subgroups = 0
	# for subgroup in subgroups:
	# 	elements_in_subgroups += len(subgroup)
	# print(f'Number of subgroups: {len(subgroups)}, number of elements in subgroups: {elements_in_subgroups}')

	# for each subgroup, keep the one with the lowest number of opt_BRAM18K
	clean_results = []
	for subgroup in subgroups:
		min_BRAM18K = subgroup[0]['opt_BRAM18K']
		best_result = subgroup[0]
		for result in subgroup:
			if result['opt_BRAM18K'] < min_BRAM18K:
				best_result = result
				min_BRAM18K = result['opt_BRAM18K']
		clean_results.append(best_result)
	# sort clean_results by latency
	clean_results.sort(key=lambda x: x['opt_latency'])
	sorted_by_latency_results = clean_results

	# result_file = open('$STREAM_VSA_PATH/data/tests/enet_'+str(hw_params['DATA_W0'])+'.csv', 'w')
	# print('achieved_latency, ideal_latency, dsp_eff, fps, sa_rows, sa_cols, sa_simd, lane, dsp, bram', file=result_file)
	# print('id, achieved_latency, ideal_latency, dsp_eff, fps, sa_rows, sa_cols, sa_simd, lane, dsp, bram')
	table_list = []
	table_list.append(['achieved_latency', 'ideal_latency', 'dsp_eff', 'fps', 'GOPs', 'sa_rows', 'sa_cols', 'sa_simd', 'lane', 'dsp', 'bram'])
	# for idx in range(len(sorted_by_latency_results)):
	# 	result = sorted_by_latency_results[idx]
	# 	print(result['opt_params']['LAYER_OUT_NUM_T_LIST'][70:-1])
	# exit()
	for idx in range(50 if len(sorted_by_latency_results) > 50 else len(sorted_by_latency_results)):
	# for idx in range(len(sorted_by_latency_results)):
		result = sorted_by_latency_results[idx]
		# print(result['opt_params']['LAYER_OUT_NUM_T_LIST'][70:-1])
		opt_params = result['opt_params']
		opt_dsp = result['opt_DSP']
		opt_bram = result['opt_BRAM18K']
		sa_rows, sa_cols, sa_simd, lane = opt_params['SA_ROWS'], opt_params['SA_COLS'], opt_params['SA_SIMD'], opt_params['LANE']
		ideal_latency = total_macs/(opt_params['SA_ROWS']*opt_params['SA_COLS']*opt_params['SA_SIMD']*opt_params['FRE']*1e6)
		achieved_latency = result['opt_latency'] / (opt_params['FRE'] * 1e6)
		dsp_eff = (ideal_latency / achieved_latency)
		fps = 1/achieved_latency
		gops = total_macs*2/achieved_latency/1e9
		num_t_components = []
		# opt_time = print_results(sorted_by_latency_results, num_t_components, layer_configs, layers_order, hw_params, total_macs, board_info)
		# print(opt_time, achieved_latency)
		# print('%f, %f, %f, %f, %d, %d, %d, %d, %d, %d' % (achieved_latency, ideal_latency, dsp_eff, fps, sa_rows, sa_cols, sa_simd, lane, opt_dsp, opt_bram), file=result_file)
		table_list.append([achieved_latency, ideal_latency, dsp_eff, fps, gops, sa_rows, sa_cols, sa_simd, lane, opt_dsp, opt_bram])
		# print('Achieved latency: %0.5f (s)' % (achieved_latency), 'Ideal latency: %0.5f (s)' % (ideal_latency), 'DSP efficiency: %0.2f' % (dsp_eff), '%', end='')
		# print('\t', 'FPS: %0.2f' % fps, 'SA_ROWS: %d' % (sa_rows), 'SA_COLS: %d' % (sa_cols), 'SA_SIMD: %d' % (sa_simd), 'LANE: %d' % (lane))
	# result_file.close()
	table_df = pd.DataFrame(table_list[1:], columns=table_list[0])
	print('------------------------------------------candiadate design choices------------------------------------------')
	print(table_df)
	

	# design_params_file = open('../data/design_params.csv', 'w')
	# print(sorted_by_latency_results[0])
	# print_results(sorted_by_latency_results, connected_components, layer_configs, layers_order, hw_params, total_macs, board_info)
	# sorted_by_dsp_eff_results = sort_by_key(results, 'dsp_eff')
	# exit()

	# for layer in layers_order:
	# 	print(layer_configs[layer])
	# exit()
	# get user input
	design = input('Enter design id to continue: ')
	# design = 0#input('Enter design id to continue: ')

	top_result = sorted_by_latency_results[int(design)]
	# print_layers_details(top_result, layer_configs, layers_order, hw_params)
	print_result(top_result, num_t_components, layer_configs, layers_order, hw_params, total_macs, board_info, dse_report_file, detailed_dse_report_file)
	max_k = 0
	for layer in layers_order:
		layer_config = layer_configs[layer]
		ops = layer_config['ops']
		for op in ops:
			op_params = op['params']
			if 'kernel_h' in op_params or 'kernel_w' in op_params:
				if op_params['kernel_h'] > max_k or op_params['kernel_w'] > max_k:
					max_k = max(op_params['kernel_h'], op_params['kernel_w'])

	max_stride = 0
	for layer in layers_order:
		layer_config = layer_configs[layer]
		ops = layer_config['ops']
		for op in ops:
			op_params = op['params']
			if 'stride' in op_params:
				if op_params['stride'] > max_stride:
					max_stride = op_params['stride']
	
	design_params = {}
	design_params['ARCH_MODULES'] = arch_modules
	design_params['K'] = max_k
	design_params['STRIDE'] = max_stride
	design_params['TSTRIDE'] = max_upsample_factor
	design_params['DILATION_RATE'] = 1 #TODO: add dilation rate to graph translation
	design_params['IN_NUM'] = max_in_num
	design_params['OUT_NUM'] = max_out_num
	design_params['IN_H'] = max_in_h
	design_params['IN_W'] = max_in_w
	design_params['IN_NUM_T'] = top_result['opt_params']['LAYER_IN_NUM_T']
	design_params['OUT_NUM_T'] = top_result['opt_params']['LAYER_OUT_NUM_T']
	design_params['IN_H_T'] = top_result['opt_params']['LAYER_IN_H_T']
	design_params['IN_W_T'] = top_result['opt_params']['LAYER_IN_W_T']
	design_params['SA_ROWS'] = top_result['opt_params']['SA_ROWS']
	design_params['SA_COLS'] = top_result['opt_params']['SA_COLS']
	design_params['SA_SIMD'] = top_result['opt_params']['SA_SIMD']
	design_params['LANE'] = top_result['opt_params']['LANE']
	design_params['DATA_WIDTH'] = dw
	design_params['DATA_TYPE'] = hw_params['DATA_T0']

	# # exit()
	opt_params = top_result['opt_params']
	wt = opt_params["LAYER_IN_W_T_LIST"]
	ht = opt_params["LAYER_IN_H_T_LIST"]
	nt = opt_params["LAYER_IN_NUM_T_LIST"]
	mt = opt_params["LAYER_OUT_NUM_T_LIST"]
	design_params['IN_NUM_T_MAX'] = max(nt)
	design_params['OUT_NUM_T_MAX'] = max(mt)
	design_params['IN_H_T_MAX'] = max(ht)
	design_params['IN_W_T_MAX'] = max(wt)
	design_params['INSTS_NUM'] = len(layers_order)
	with open(f_arch_out, 'w') as f:
		json.dump(design_params, f, indent = 2)
	
	insts = []
	for idx, layer_name in enumerate(layer_configs):
		inst = {}
		layer_config = layer_configs[layer_name]
		inst['name'] = layer_config['LAYER_NAME']
		inst['main_input'] = layer_config['MAIN_INPUT']
		inst['main_output'] = layer_config['MAIN_OUTPUT']
		inst['output_name'] = layer_config['OUTPUT_NAME']
		inst['secondary_input'] = layer_config['SECONDARY_INPUT']
		inst['secondary_output'] = layer_config['SECONDARY_OUTPUT']
		inst['in_num'] = layer_config['LAYER_IN_NUM']
		inst['out_num'] = layer_config['LAYER_OUT_NUM']
		inst['in_h'] = layer_config['LAYER_IN_H']
		inst['in_w'] = layer_config['LAYER_IN_W']
		inst['out_h'] = layer_config['LAYER_OUT_H']
		inst['out_w'] = layer_config['LAYER_OUT_W']
		inst['in_num_t'] = nt[idx]
		inst['out_num_t'] = mt[idx]
		inst['in_h_t'] = ht[idx]
		inst['in_w_t'] = wt[idx]
		inst['ops'] = layer_config['ops']
		insts.append(inst)
	
	with open(f_model_out, 'w') as f:
		print(input_shape, file = f)
		for inst in insts:
			print(inst, file = f)

	# model_out = open("network_out.model", "w")
	# for i, line in enumerate(modelLines):
	# 	line = line.strip('\n')
	# 	if i == 0:
	# 		line += 'in_num_t,out_num_t,in_h_t,in_w_t'
	# 	else:
	# 		line += ';' + str(nt[i-1]) + ';' + str(mt[i-1]) + ';' + str(ht[i-1]) + ';' + str(wt[i-1])
	# 	model_out.write(line + '\n')
	
	# model_out.close()
	# model.close()
	print("*************************************************", file=dse_report_file)
	print("*************************************************")

	global_timer_end = time.time()
	print('Total elapsed time (s): %.3f' % (global_timer_end - global_timer_start), file=dse_report_file)
	print('Total elapsed time (s): %.3f' % (global_timer_end - global_timer_start))
	print("*************************************************", file=dse_report_file)
	print("*************************************************")
	print(str(design_params['SA_ROWS'])+'_'+str(design_params['SA_COLS'])+'_'+str(design_params['SA_SIMD']), file=dse_report_file)
	
def param_sweep(params_list, config, layer_configs, layers_order, num_t_components, w_t_components, systolic_en, solver_en, test):
	results = []
	count = 0
	# print(len(params_list))
	for i, params in enumerate(params_list):
		opt_latency = np.inf
		opt_DSP = np.inf
		opt_BRAM18K = np.inf
		opt_dsp_eff = 0
		opt_params = {}
		solver_fails = 0
		model_fails = 0
		# params_t = params_list[i]
		# params = dict(params_t)
		IN_H_T = params['LAYER_IN_H_T']
		IN_W_T = params['LAYER_IN_W_T'] 
		OUT_H_T = params['LAYER_OUT_H_T']
		OUT_W_T = params['LAYER_OUT_W_T'] 
		IN_NUM_T = params['LAYER_IN_NUM_T'] 
		OUT_NUM_T = params['LAYER_OUT_NUM_T'] 
		LANE = params['LANE'] 
		SA_ROWS = params['SA_ROWS'] 
		SA_COLS = params['SA_COLS'] 
		SA_SIMD = params['SA_SIMD'] 
		
		# resource estimation
		DSP, BRAM18K = res_est(params)
		# DSP = int(DSP/5)
		# hw pruning
		if IN_W_T % SA_COLS != 0 or IN_NUM_T % SA_SIMD != 0 or IN_NUM_T % SA_ROWS != 0:
			continue
		# resource pruning
		if DSP > config['BOARD']['DSP_THRES'] * config['BOARD']['DSP']:
			continue
		# if BRAM18K > config['BOARD']['BRAM18K_THRES'] * config['BOARD']['BRAM18K']:
		# 	continue

		# frequency adjustment
		# as the resource utilization will affect the frequency, we will adjust freqeuncy here using a simple step-wise function
		# if DSP / config['BOARD']['DSP'] > 0.8 or BRAM18K / config['BOARD']['BRAM18K'] > 0.8:
		# 	params['FRE'] = 180
		# else:
		params['FRE'] = 220

		count += 1

		latency, new_params = model_latency_est(params, layer_configs, layers_order, config['DYNAMIC_TILING_LEVEL'])
		# print(latency, params)
		# if params == {'DATA_W0': 32, 'DATA_W1': 32, 'DATA_W2': 32, 'BUS_W': 512, 'DATA_T0': 'float', 'DATA_T1': 'float', 'DATA_T2': 'float', 'K_T': 16, 'LAYER_IN_H_T': 32, 'LAYER_IN_W_T': 144, 'LAYER_OUT_H_T': 64, 'LAYER_OUT_W_T': 288, 'LAYER_IN_NUM_T': 64, 'LAYER_OUT_NUM_T': 64, 'LANE': 8, 'SA_ROWS': 8, 'SA_COLS': 9, 'SA_SIMD': 8, 'FRE': 220}:
		# 	print(latency)
		# 	for key in layer_configs:
		# 		layer_config = layer_configs[key]
		# 		for elem in layer_config:
		# 			print(f'\t{elem}: {layer_config[elem]}')
		# 	for layer_order in layers_order:
		# 		print(layer_order)
		# 	print(config['DYNAMIC_TILING_LEVEL'])
		if latency == np.inf:
			continue

		model_fails += new_params['MODEL_FAIL']


		cur_fps = new_params['FRE'] * 1e6 * (1 / latency)
		opt_fps = new_params['FRE'] * 1e6 * (1 / opt_latency)
		
		opt_latency = latency
		opt_DSP = DSP
		opt_BRAM18K = BRAM18K
		opt_params['LAYER_IN_H_T'] = new_params['LAYER_IN_H_T']
		opt_params['LAYER_IN_W_T'] = new_params['LAYER_IN_W_T']
		opt_params['LAYER_OUT_H_T'] = new_params['LAYER_OUT_H_T']
		opt_params['LAYER_OUT_W_T'] = new_params['LAYER_OUT_W_T']
		opt_params['LAYER_IN_NUM_T'] = new_params['LAYER_IN_NUM_T']
		opt_params['LAYER_OUT_NUM_T'] = new_params['LAYER_OUT_NUM_T']
		opt_params['SA_SIMD'] = new_params['SA_SIMD']
		opt_params['SA_ROWS'] = new_params['SA_ROWS']
		opt_params['SA_COLS'] = new_params['SA_COLS']
		opt_params['LANE'] = new_params['LANE']
		opt_params['LAYER_IN_NUM_T_LIST'] = list(new_params['LAYER_IN_NUM_T_LIST'])
		opt_params['LAYER_OUT_NUM_T_LIST'] = list(new_params['LAYER_OUT_NUM_T_LIST'])
		opt_params['LAYER_IN_H_T_LIST'] = list(new_params['LAYER_IN_H_T_LIST'])
		opt_params['LAYER_IN_W_T_LIST'] = list(new_params['LAYER_IN_W_T_LIST'])
		opt_params['LAYER_DSP_EFF_LIST'] = list(new_params['LAYER_DSP_EFF_LIST'])
		opt_params['FRE'] = new_params['FRE']

		res = {}
		res['opt_latency'] = opt_latency
		res['opt_DSP'] = opt_DSP
		res['opt_BRAM18K'] = opt_BRAM18K
		res['opt_params'] = opt_params
		res['solver_fails'] = solver_fails
		res['model_fails'] = model_fails
		results.append(res)
	return results

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
	

	parser.add_argument('-mi', '--model_in', metavar='MODEL_IN', default='./network.csv', help='model description', dest='model_in')
	parser.add_argument('-mo', '--model_out', metavar='MODEL_OUT', default='./network.csv', help='model description', dest='model_out')
	parser.add_argument('-o', '--output', metavar='OUTPUT', default='./network_out.csv', help='output description', dest='output')
	parser.add_argument('-ai', '--architecture-in', metavar='ARCHITECTURE', default='./architecture.json', help='architecture configuration', dest='arch_in')
	parser.add_argument('-ao', '--architecture-out', metavar='ARCHITECTURE_OUT', default='./architecture_out.json', help='architecture configuration', dest='arch_out')
	parser.add_argument('-b', '--board', metavar='BOARD', default='./vu9p.json', help='FPGA board information', dest='board')
	parser.add_argument('--parallel', help='multi-threading parallelization', default=True, action='store_false', dest='parallel')
	parser.add_argument('--systolic', help='systolic-array-search', default=True, action='store_false', dest='systolic')
	parser.add_argument('-dt', '--dynamic-tiling', metavar='DYNAMIC_TILING', help='dynamic tiling level (0:disabled, 1:channel 2:height/width)', required=False, type=int, default=2, dest='dynamic_tiling')
	parser.add_argument('--solver', help='use solver for model latency estimation', default=False, action='store_true', dest='solver')
	parser.add_argument('-dw', '--data_width', help='data width', default='32', type=int, dest='dw')
	parser.add_argument('--test', help='systolic-array-search', default=False, action='store_true', dest='test')
	args = parser.parse_args()
	run(args.model_in, args.model_out, args.board, args.arch_in, args.arch_out, args.parallel, args.systolic, args.dynamic_tiling, args.solver, args.output, args.dw, args.test)