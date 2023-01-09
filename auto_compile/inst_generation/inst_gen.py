import numpy as np
import json
import argparse
import copy
import multiprocessing
import subprocess
import time
import pandas as pd
import math
import sys
import networkx as nx

def get_SA_insts(inst, arch):

	sa_rows = arch['SA_ROWS']
	sa_cols = arch['SA_COLS']
	sa_simd = arch['SA_SIMD']
	in_num = inst['in_num']
	out_num = inst['out_num']
	in_h = inst['in_h']
	in_w = inst['in_w']
	out_h = inst['out_h']
	out_w = inst['out_w']
	in_num_t = inst['in_num_t']
	out_num_t = inst['out_num_t']
	in_h_t = inst['in_h_t']
	in_w_t = inst['in_w_t']
	filter_s_h = inst['filter_d1_h']
	filter_s_w = inst['filter_d1_w']
	stride = inst['conv_stride']

	task_num1 = int(np.ceil(float(in_num) / in_num_t) * np.ceil(float(out_num) / out_num_t) * np.ceil(float(in_h) / in_h_t) * np.ceil(float(in_w) / in_w_t))
	task_num2 = int(np.ceil(float(out_num) / out_num_t) * np.ceil(float(in_h) / in_h_t) * np.ceil(float(in_w) / in_w_t))
	local_accum_num = int(in_num_t / sa_simd * filter_s_h * filter_s_w)
	local_reg_num = int((in_h_t / stride) * (in_w_t / sa_cols / stride) * (out_num_t / sa_rows))
	row_il_factor = int(out_num_t / sa_rows)
	col_il_factor = int(in_w_t / sa_cols / stride)

	return task_num1, task_num2, local_accum_num, local_reg_num, row_il_factor, col_il_factor

def get_prev_inst(insts, main_input):
	for inst in insts:
		if main_input[0] == inst['name']:
			return inst
	return None

def isPool_only(inst):
	ops = inst['ops']
	maxpool_en = False
	conv_en = False
	for op in ops:
		if op['name'] == 'MaxPool' and op['params']['en']:
			maxpool_en = True
		elif op['name'] == 'Conv' and op['params']['en']:
			conv_en = True
	if maxpool_en and not conv_en:
		return True
	else:
		return False

def noConv(inst):
	ops = inst['ops']
	for op in ops:
		if op['name'] == 'Conv' and op['params']['en']:
			return False
		elif op['name'] == 'ConvTranspose' and op['params']['en']:
			return False
	return True

def get_next_inst(insts, curr_inst):
	curr_name = curr_inst['name']
	for inst in insts:
		main_input = inst['main_input'][0]
		if curr_name == main_input:
			return inst
	return None

def get_next_inst_main(insts, curr_inst):
	curr_name = curr_inst['name']
	next_insts = []
	for inst in insts:
		main_inputs = inst['main_input']
		for input in main_inputs:
			if curr_name == input:
				next_insts.append(inst)
	main_output = None
	max_kernel_h = 1
	max_kernel_w = 1
	for inst in next_insts:
		for op in inst['ops']:
			op_params = op['params']
			if op_params['en']:
				if op['name'] == 'Conv' or op['name'] == 'ConvTranspose':
					kernel_h = op_params['kernel_h']
					kernel_w = op_params['kernel_w']
					if kernel_h > max_kernel_h and kernel_w > max_kernel_w:
						max_kernel_h = kernel_h
						max_kernel_w = kernel_w
						main_output = inst
	return main_output

def get_next_inst_secondary(insts, curr_inst):
	curr_name = curr_inst['name']
	for inst in insts:
		main_input = inst['secondary_input'][0]
		if curr_name == main_input:
			return inst
	return None

def get_en_value(inst):
	# CONV_1ST_EN    				= LAYER_EN[0];
	# DEPTH_CONV_EN  				= LAYER_EN[1];
	# CONV_EN        				= LAYER_EN[2];
	# RELU_EN        				= LAYER_EN[3];
	# RELU6_EN       				= LAYER_EN[4];
	# POOL_EN        				= LAYER_EN[5];
	# UP_SAMPLE_EN   				= LAYER_EN[6];
	# BIAS_EN        				= LAYER_EN[7];
	# INTER_LOAD_EN  				= LAYER_EN[8];
	# INTER_WRITE_EN 				= LAYER_EN[9];	
	# BATCH_NORM_EN  				= LAYER_EN[10];
	# LOAD_PREV_CIN  				= LAYER_EN[11];
	# BATCH_NORM_EN_DEPTH  	= LAYER_EN[12];
	# RELU_1_EN             = LAYER_EN[13];
	# BIAS_1_EN             = LAYER_EN[14];
	# BATCH_NORM_1_EN       = LAYER_EN[15];
	# CONCAT_EN      				= LAYER_EN[16];
	# ADD_EN         				= LAYER_EN[17];
	conv_en = 0
	relu_en = 0
	pool_en = 0
	upsample_en = 0
	bias_en = 0
	bn_en   = 0
	prev_en = 0
	cnct_en = 0
	add_en  = 0
	for op in inst['ops']:
		op_params = op['params']
		op_en = op_params['en']
		# print(op['name'], op_en)
		if op_en:
			if op['name'] == 'Conv' or op['name'] == 'ConvTranspose':
				conv_en = op_en
				bias_en = op['params']['bias_en']
			elif op['name'] == 'PReLU':
				relu_en = op_en
				bias_en = op_en
			elif op['name'] == 'LeakyRelu' or op['name'] == 'Relu':
				relu_en = op_en
			elif op['name'] == 'BatchNormalization':
				bn_en = op_en
			elif op['name'] == 'MaxPool':
				pool_en = op_en
			elif op['name'] == 'Upsample':
				upsample_en = op_en
			elif op['name'] == 'Concat':
				cnct_en = op_en
				prev_en = op_en
			elif op['name'] == 'Add':
				add_en  = op_en
				prev_en = op_en
	en = conv_en*2**2 + relu_en*2**3 + pool_en*2**5 + upsample_en*2**6 + bias_en*2**7 + bn_en*2**10 + prev_en*2**11 + cnct_en*2**16 + add_en*2**17
	return en
		

def calculate_addresses(f_model, insts, sa_rows, sa_cols, sa_simd):
	graph = nx.DiGraph()
	if 'ENet' in f_model:
		data = {}
		data['main_output_addr'] = 2328
		# data['secondary_output_addr'] = 0
		graph.add_node('StatefulPartitionedCall/model/initial_max_pooling_layer/MaxPool__6_padded', data = data)
		data = {}
		data['main_output_addr'] = 672800
		# data['secondary_output_addr'] = 672800
		graph.add_node('StatefulPartitionedCall/model/initial_max_pooling_layer/MaxPool__6', data = data)
	elif 'UNet' in f_model:
		data = {}
		data['main_output_addr'] = 0
		# data['secondary_output_addr'] = 0
		graph.add_node('StatefulPartitionedCall/model/conv2d_6/Conv2D__6', data = data)
		data = {}
		data['main_output_addr'] = 524288 + 4176
		# data['secondary_output_addr'] = 672800
		graph.add_node('StatefulPartitionedCall/model/conv2d_6/Conv2D__6_padded', data = data)
	elif 'VGG16' in f_model:
		data = {}
		data['main_output_addr'] = 3632
		# data['secondary_output_addr'] = 0
		graph.add_node('StatefulPartitionedCall/vgg16/block1_conv1/BiasAdd__6', data = data)
	
	for inst in insts:
		out_num = inst['out_num']
		out_h = inst['out_h']
		out_w = inst['out_w']
		for main_input in inst['main_input']:
			data = {'tensor_size': (out_num, out_h, out_w), 'type': 'main'} # main with respect to the receiving node
			graph.add_edge(main_input, inst['name'], data = data)
		for secondary_input in inst['secondary_input']:
			data = {'tensor_size': (out_num, out_h, out_w), 'type': 'secondary'} # secondary with respect to the receiving node
			graph.add_edge(secondary_input, inst['name'], data = data)

	for idx, inst in enumerate(insts):
		node = inst['name']

		in_num = inst['in_num']
		in_h = inst['in_h']
		in_w = inst['in_w']
		in_num_hw = inst['in_num_hw']
		in_h_hw = inst['in_h_hw']
		in_w_hw = inst['in_w_hw']
		out_num = inst['out_num']
		out_h = inst['out_h']
		out_w = inst['out_w']

		in_num_t = inst['in_num_t']
		out_num_t = inst['out_num_t']
		out_h_np = inst['out_h_np'] 
		out_h_sp = inst['out_h_sp'] 
		out_w_wp = inst['out_w_wp'] 
		out_w_ep = inst['out_w_ep'] 
		
		out_h_hw = out_h + out_h_np + out_h_sp
		out_w_hw = out_w + out_w_wp + out_w_ep
		in_w_wp = (in_w_hw - in_w)//2
		in_w_ep = in_w_wp
		in_h_np = (in_h_hw - in_h)//2
		in_h_sp = in_h_np
		if out_w_wp == 0 and out_w_ep == 0 and out_h_np == 0 and out_h_sp == 0:
			out_padding_offset = 0
		else:
			out_padding_offset = out_num_t * (out_w_hw*out_h_np + out_w_wp)
		if in_w_wp == 0 and in_w_ep == 0 and in_h_np == 0 and in_h_sp == 0:
			in_padding_offset = 0
		else:
			in_padding_offset = in_num_t * (in_w_hw*in_h_np + in_w_wp)

		data = {}
		# get input nodes
		in_nodes = graph.predecessors(node)
		for n in in_nodes:
			edge_type = graph[n][node]['data']['type']
			if edge_type == 'main':
				data['main_input_addr'] = graph.nodes[n]['data']['main_output_addr'] - in_padding_offset
			elif edge_type == 'secondary':
				data['secondary_input_addr'] = graph.nodes[n]['data']['main_output_addr']

		if idx == 0 and 'ENet' in f_model:
			data['main_output_addr'] = 1336352
		elif idx == 0 and 'UNet' in f_model:
			data['main_output_addr'] = 1065088
		elif idx == 1 and 'UNet' in f_model:
			data['main_output_addr'] = 3178944
		else:
			data['main_output_addr'] = data['main_input_addr'] + in_num_hw*in_h_hw*in_w_hw + out_padding_offset
		
		# special case for ENet
		# successors = [node for node in graph.successors(node)]
		# edges = [graph[node][successor]['data'] for successor in successors]
		# edge_types = [edge['type'] for edge in edges]
		
		# flag = False
		# if len(edge_types) > 1 and 'secondary' not in edge_types:
		# 	flag = True
		# 	prev_nodes = successors

		# if node in prev_nodes:
		# 	node1 = prev_nodes[0]
		# 	node2 = prev_nodes[1]
		# 	print(node1, node2)
		# data['main_output_addr'] = graph.nodes[node2]['data']['main_output_addr'] 
		# prev_nodes.remove(node)
		if idx == 72 and 'ENet' in f_model:
			data['main_output_addr'] = 9332304
		if idx == 82 and 'ENet' in f_model:
			data['main_output_addr'] = 11107152

		graph.nodes[node]['data'] = data

		inst['input_address'] = graph.nodes[inst['name']]['data']['main_input_addr']
		if 'secondary_input_addr' in graph.nodes[inst['name']]['data']:
			inst['prev_input_address'] = graph.nodes[inst['name']]['data']['secondary_input_addr']
		else:
			inst['prev_input_address'] = 0
		inst['output_address'] = graph.nodes[inst['name']]['data']['main_output_addr']

	return insts

def run(f_model, f_arch, f_insts):

	with open(f_arch, 'r') as f:
		arch = json.load(f)
	
	modules = arch['ARCH_MODULES']
	sa_cols = arch['SA_COLS']
	sa_rows = arch['SA_ROWS']
	sa_simd = arch['SA_SIMD']

	insts = []
	with open(f_model, 'r') as f:
		lines = f.readlines()
		input_shape = eval(lines[0])
		lines = lines[1:]
		
		in_pointer = 0
		out_pointer = input_shape['IN_NUM']*input_shape['IN_H']*input_shape['IN_W']
		weight_pointer = 0
		bias_pointer = 0

		raw_insts = []
		for line in lines:
			inst = eval(line)
			raw_insts.append(inst)

		for idx, inst in enumerate(raw_insts):
			
			inst_name = inst['name']
			main_input = inst['main_input']
			secondary_input = inst['secondary_input']
			conv_kernel_h = 1
			conv_kernel_w = 1
			pool_kernel = 1
			conv_stride = 1
			pool_stride = 1
			for op in inst['ops']:
				op_params = op['params']
				if op_params['en']:
					if op['name'] == 'Conv' or op['name'] == 'ConvTranspose':
						conv_kernel_h = op_params['kernel_h']
						conv_kernel_w = op_params['kernel_w']
						conv_stride = op_params['stride']
						dilation_rate = op_params['dilation_rate']
						dilated_kernel_h = conv_kernel_h*dilation_rate-dilation_rate+1
						dilated_kernel_w = conv_kernel_w*dilation_rate-dilation_rate+1
						in_h_hw = inst['in_h'] + conv_kernel_h - 1 if idx==0 else inst['in_h'] + dilated_kernel_h - conv_stride
						in_w_hw = inst['in_w'] + conv_kernel_w - 1 if idx==0 else inst['in_w'] + dilated_kernel_w - conv_stride
						break
					elif op['name'] == 'MaxPool':
						pool_kernel = op_params['kernel_h']
						pool_stride = op_params['stride']
						in_h_hw = inst['in_h']
						in_w_hw = inst['in_w']

			in_num_hw = max(int((np.ceil(inst['in_num']/sa_simd))*sa_simd), inst['in_num_t'])
			out_num_hw = max(int((np.ceil(inst['out_num']/sa_rows))*sa_rows), inst['out_num_t'])	
			if idx==88:
				in_h_hw = inst['in_h']
				in_w_hw = inst['in_w']
			inst['in_num_hw'] = in_num_hw
			inst['out_num_hw'] = out_num_hw
			inst['in_h_hw'] = in_h_hw
			inst['in_w_hw'] = in_w_hw
			in_num_t = inst['in_num_t']
			out_num_t = inst['out_num_t']

			#get the inst whose name is in the main_input of current inst
			main_in_inst = get_prev_inst(raw_insts, main_input)
			
			# get inst whose main input is the current inst
			out_h_np = 0
			out_h_sp = 0
			out_w_wp = 0
			out_w_ep = 0
			main_out_inst = get_next_inst(raw_insts, inst)# doesn't work for UNet
			if main_out_inst is not None:
				for op in main_out_inst['ops']:
					op_params = op['params']
					if op_params['en']:
						if op['name'] == 'Conv' or op['name'] == 'ConvTranspose':
							dilation_rate = op_params['dilation_rate']
							dilated_kernel_h = op_params['kernel_h']*dilation_rate-dilation_rate+1
							dilated_kernel_w = op_params['kernel_w']*dilation_rate-dilation_rate+1
							conv_stride = op_params['stride']
							break
						elif op['name'] == 'MaxPool':
							dilated_kernel_h = op_params['kernel_h']
							dilated_kernel_w = op_params['kernel_w']
							conv_stride = op_params['stride']
							# print('dilated_kernel_h', dilated_kernel_h)
							# print('dilated_kernel_w', dilated_kernel_w)
							# print('conv_stride', conv_stride)
							break
						# elif op['name'] == 'ConvTranspose':
						# 	dilation_rate = 1
						# 	dilated_kernel_h = op_params['kernel_h']*dilation_rate-dilation_rate+1
						# 	dilated_kernel_w = op_params['kernel_w']*dilation_rate-dilation_rate+1
						# 	conv_stride = op_params['stride']
						# elif op['name'] == 'MaxPool':
						# 	pool_kernel = op_params['kernel_h']
						# 	pool_stride = op_params['stride']

				out_h_np = (dilated_kernel_h - conv_stride)//2
				out_h_sp = (dilated_kernel_h - conv_stride)//2
				out_w_wp = (dilated_kernel_w - conv_stride)//2
				out_w_ep = (dilated_kernel_w - conv_stride)//2
			# print(out_h_np, out_h_sp, out_w_wp, out_w_ep)

			inst['out_h_np'] = out_h_np
			inst['out_h_sp'] = out_h_sp
			inst['out_w_wp'] = out_w_wp
			inst['out_w_ep'] = out_w_ep

			out_h = inst['out_h']
			out_w = inst['out_w']
			out_h_hw = out_h + out_h_np + out_h_sp
			out_w_hw = out_w + out_w_wp + out_w_ep

				
			if noConv(inst):
				inst['weight_address'] = 0
				inst['bias_address'] = 0
			else:
				inst['weight_address'] = weight_pointer
				inst['bias_address'] = bias_pointer
				weight_pointer += in_num_hw*out_num_hw*conv_kernel_h*conv_kernel_w
				bias_pointer += out_num_hw
		
		raw_insts = calculate_addresses(f_model, raw_insts, sa_rows, sa_cols, sa_simd)

		for in_inst in raw_insts:
			inst = {}

			conv_op = None
			tconv_op = None
			maxpool_op = None

			for op in in_inst['ops']:
				op_params = op['params']
				if op_params['en'] and op['name'] == 'Conv':
					conv_op = op
				elif op_params['en'] and op['name'] == 'ConvTranspose':
					tconv_op = op
				elif op_params['en'] and op['name'] == 'MaxPool':
					maxpool_op = op

			if conv_op is not None:
				op_params = conv_op['params']
				conv_type = op_params['type']
				conv_kernel_h = op_params['kernel_h']
				conv_kernel_w = op_params['kernel_w']
				conv_stride = op_params['stride']
				tstride = op_params['tstride']
				dilation_rate = op_params['dilation_rate']
				dilated_kernel_h = conv_kernel_h*dilation_rate-dilation_rate+1
				dilated_kernel_w = conv_kernel_w*dilation_rate-dilation_rate+1
				pool_kernel = 1
				pool_stride = 1
				pool_channels = 0
			if tconv_op is not None:
				op_params = tconv_op['params']
				conv_type = op_params['type']
				conv_kernel_h = op_params['kernel_h']
				conv_kernel_w = op_params['kernel_w']
				tstride = op_params['tstride']
				dilation_rate = 1
				dilated_kernel_h = conv_kernel_h*dilation_rate-dilation_rate+1
				dilated_kernel_w = conv_kernel_w*dilation_rate-dilation_rate+1
				pool_kernel = 1
				pool_stride = 1
				pool_channels = 0
			if maxpool_op is not None:
				op_params = maxpool_op['params']
				pool_kernel = op_params['kernel_h']
				pool_stride = op_params['stride']
				pool_channels = op_params['in_channels']

			inst['in_num_hw'] = max(in_inst['in_num_hw'], in_inst['in_num_t'])
			inst['out_num_hw'] = max(in_inst['out_num_hw'], in_inst['out_num_t']) #TODO: dram bus width may be needed here
			inst['in_h_hw'] = in_inst['in_h_hw']
			inst['in_w_hw'] = in_inst['in_w_hw']
			inst['out_h_np'] = in_inst['out_h_np']
			inst['out_h_sp'] = in_inst['out_h_sp']
			inst['out_w_wp'] = in_inst['out_w_wp']
			inst['out_w_ep'] = in_inst['out_w_ep']
			inst['in_num'] = in_inst['in_num']
			inst['out_num'] = in_inst['out_num']
			inst['in_h'] = in_inst['in_h']
			inst['in_w'] = in_inst['in_w']
			inst['out_h'] = in_inst['out_h']
			inst['out_w'] = in_inst['out_w']
			inst['input_address'] = in_inst['input_address']
			inst['weight_address'] = in_inst['weight_address']
			inst['bia_address'] = in_inst['bias_address']
			inst['output_address'] = in_inst['output_address']
			inst['dw_kernel'] = 1
			inst['conv_kernel_h'] = dilated_kernel_h
			inst['conv_kernel_w'] = dilated_kernel_w
			inst['pool_kernel'] = pool_kernel
			inst['conv_stride'] = conv_stride
			inst['pool_stride'] = pool_stride
			inst['en'] = get_en_value(in_inst)
			inst['prev_input_address'] = in_inst['prev_input_address']
			inst['in_num_t'] = in_inst['in_num_t']
			inst['out_num_t'] = in_inst['out_num_t']
			inst['in_h_t'] = in_inst['in_h_t']
			inst['in_w_t'] = in_inst['in_w_t']
			inst['batch_size'] = 1


			if conv_type=='NConv' or conv_type=='DConv' or conv_type=='':
				inst['conv_type'] = 0 if conv_type=='NConv' or conv_type=='' else 2
				inst['filter_d0_h'] = dilated_kernel_h
				inst['filter_d0_w'] = dilated_kernel_w
				inst['filter_d1_h'] = conv_kernel_h
				inst['filter_d1_w'] = conv_kernel_w
				inst['dconv_dr'] = dilation_rate
				inst['tconv_stride'] = tstride
				inst['k_num'] = 1
				inst['k_h'] = str(conv_kernel_h)+' 0 0 0'
				inst['k_w'] = str(conv_kernel_w)+' 0 0 0'
			elif conv_type=='TConv':
				if conv_kernel_h==3 and conv_kernel_w==3:
					inst['conv_type'] = 1
					inst['filter_d0_h'] = 2
					inst['filter_d0_w'] = 2
					inst['filter_d1_h'] = conv_kernel_h
					inst['filter_d1_w'] = conv_kernel_w
					inst['dconv_dr'] = dilation_rate
					inst['tconv_stride'] = tstride
					inst['k_num'] = tstride*tstride
					inst['k_h'] = '2 2 1 1'
					inst['k_w'] = '2 1 2 1'
				elif conv_kernel_h==2 and conv_kernel_w==2:
					inst['conv_type'] = 1
					inst['filter_d0_h'] = 1
					inst['filter_d0_w'] = 1
					inst['filter_d1_h'] = conv_kernel_h
					inst['filter_d1_w'] = conv_kernel_w
					inst['dconv_dr'] = dilation_rate
					inst['tconv_stride'] = tstride
					inst['k_num'] = tstride*tstride
					inst['k_h'] = '1 1 1 1'
					inst['k_w'] = '1 1 1 1'

			if isPool_only(in_inst):
				inst['task_num1'] = 0
				inst['task_num2'] = 0
				inst['local_accum_num'] = 0
				inst['local_reg_num'] = 0
				inst['row_il_factor'] = 0
				inst['col_il_factor'] = 0
			else:
				inst['task_num1'], \
				inst['task_num2'], \
				inst['local_accum_num'], \
				inst['local_reg_num'], \
				inst['row_il_factor'], \
				inst['col_il_factor'] = get_SA_insts(inst, arch)

			if inst['en']==67620:
				inst['prev_channel'] = inst['in_num_hw']
				inst['prev_channel_hw'] = inst['in_num_hw']
			elif inst['en']==133292:
				inst['prev_channel'] = pool_channels
				inst['prev_channel_hw'] = inst['out_num_hw']
			elif inst['en']==133260:
				inst['prev_channel'] = inst['out_num_hw']
				inst['prev_channel_hw'] = inst['out_num_hw']
			elif inst['en'] == 133132:
				inst['prev_channel'] = inst['in_num_hw']
				inst['prev_channel_hw'] = inst['in_num_hw']
			elif inst['en'] == 134156:
				inst['prev_channel'] = inst['out_num_hw']
				inst['prev_channel_hw'] = inst['out_num_hw']
			else:
				inst['prev_channel'] = 0
				inst['prev_channel_hw'] = 0
			# print(inst['in_num_hw'], inst['out_num_hw'], inst['in_h_hw'], inst['in_w_hw'])
			insts.append(inst)
			# kernel_size = 1
			# stride = 1
			# for op in inst['ops']:
			# 	if 'kernel_size' in op['params']:
			# 		kernel_size = op['params']['kernel_size']
			# 	if 'stride' in op['params']:
			# 		stride = op['params']['stride']
	with open(f_insts, 'w') as f:
		for inst in insts:
			print(inst['in_num_hw'], inst['out_num_hw'], inst['in_h_hw'], inst['in_w_hw'], inst['out_h_np'], inst['out_h_sp'], inst['out_w_wp'], inst['out_w_ep'], file=f)#inst['out_h_hw'], inst['out_w_hw'], file=f)
			print(inst['in_num'], inst['out_num'], inst['in_h'], inst['in_w'], inst['out_h'], inst['out_w'], file=f)
			print(inst['input_address'], inst['weight_address'], inst['bia_address'], inst['output_address'], end=' ', file=f)
			# print(inst['dw_kernel'], inst['conv_kernel'], inst['pool_kernel'], inst['conv_stride'], inst['pool_stride'], file=f)
			print(inst['dw_kernel'], inst['conv_kernel_h'], inst['conv_kernel_w'], inst['conv_stride'], file=f)
			print(inst['en'], inst['prev_input_address'], inst['in_num_t'], inst['out_num_t'], inst['in_h_t'], inst['in_w_t'], inst['batch_size'], file=f)
			print(inst['task_num1'], inst['task_num2'], inst['local_accum_num'], inst['local_reg_num'], inst['row_il_factor'], inst['col_il_factor'], file=f)
			print(inst['conv_type'], inst['filter_d0_h'], inst['filter_d0_w'], inst['filter_d1_h'], inst['filter_d1_w'], inst['dconv_dr'], inst['tconv_stride'], inst['k_num'], inst['k_h'], inst['k_w'], inst['prev_channel'], inst['prev_channel_hw'], file=f)
			# print(inst['ops'], file=f)
			print(file=f)

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Design space exploration.')

	parser.add_argument('-mi', '--model_in', metavar='MODEL_IN', default='./network.csv', help='model description', dest='model_in')
	parser.add_argument('-a', '--architecture', metavar='ARCHITECTURE', default='./network.json', help='model description', dest='architecture')
	parser.add_argument('-io', '--inst_out', metavar='INST_OUT', default='./network.csv', help='model description', dest='inst_out')
	args = parser.parse_args()
	run(args.model_in, args.architecture, args.inst_out)