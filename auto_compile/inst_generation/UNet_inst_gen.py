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
	filter_s_h = inst['conv_kernel_h']
	filter_s_w = inst['conv_kernel_w']
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

		out_pointer = 0
		cnn_inputs = []
		if 'VGG16' in f_model:
			inst = raw_insts[0]
			inst_addr_info = {
				'name': inst['main_input'][0],
			}
			inst_addr_info['main_output_addresses'] = {}
			inst_addr_info['main_output_addresses'][inst['name']] = out_pointer
			out_pointer += 817216#1048576
		elif 'UNet' in f_model:
			inst = raw_insts[0]
			inst_addr_info = {
				'name': inst['main_input'][0],
			}
			inst_addr_info['main_output_addresses'] = {}
			inst_addr_info['main_output_addresses'][inst['name']] = out_pointer
			out_pointer += 524288#1048576
			inst = raw_insts[1]
			inst_addr_info['main_output_addresses'][inst['name']] = out_pointer
			out_pointer += 540800#1081600
		elif 'ENet' in f_model:
			inst = raw_insts[0]
			inst_addr_info = {
				'name': inst['main_input'][0],
			}
			inst_addr_info['main_output_addresses'] = {}
			inst_addr_info['main_output_addresses'][inst['name']] = out_pointer
			out_pointer += 1336352#1048576
			inst = raw_insts[1]
			inst_addr_info['main_output_addresses'][inst['name']] = out_pointer
			out_pointer += 0#1081600

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
			main_out_inst = get_next_inst_main(raw_insts, inst)
			# (second_dilation_rate*second_k-second_dilation_rate+1)
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
			# main_out_inst['dilation_rate'] = dilation_rate
			# main_out_inst['dilated_kernel_h'] = dilated_kernel_h
			# main_out_inst['dilated_kernel_w'] = dilated_kernel_w
			# main_out_inst['conv_type'] = 2 if dilation_rate>1 else 0
			out_h = inst['out_h']
			out_w = inst['out_w']
			out_h_hw = out_h + out_h_np + out_h_sp
			out_w_hw = out_w + out_w_wp + out_w_ep

			if main_in_inst==None:
				if 'ENet' in f_model:
					inst['output_address'] = inst_addr_info['main_output_addresses'][inst['name']]
					inst['input_address'] = 0
					inst['prev_input_address'] = 672800
					inst['output_address'] = out_pointer 
					inst['main_output_addresses'] = {inst['main_output'][0]:out_pointer - out_num_t*(out_w_ep + out_w_hw*out_h_np)}
					out_pointer += out_h_hw*out_w_hw*out_num_hw - out_num_t*(out_w_ep + out_w_hw*out_h_np)
					inst['secondary_output_addresses'] = {}
					inst['weight_address'] = 0
					inst['bias_address'] = 0
				elif 'VGG16' in f_model:
					inst['output_address'] = inst_addr_info['main_output_addresses'][inst['name']]
					inst['input_address'] = 0
					inst['prev_input_address'] = 0
					inst['output_address'] = out_pointer 
					inst['main_output_addresses'] = {inst['main_output'][0]:out_pointer - out_num_t*(out_w_ep + out_w_hw*out_h_np)}
					out_pointer += out_h_hw*out_w_hw*out_num_hw - out_num_t*(out_w_ep + out_w_hw*out_h_np)
					inst['secondary_output_addresses'] = {}
					inst['weight_address'] = 0
					inst['bias_address'] = 0
				elif 'UNet' in f_model and idx==0:
					inst['input_address'] = inst_addr_info['main_output_addresses'][inst['name']]
					inst['prev_input_address'] = 0
					inst['output_address'] = out_pointer 
					inst['main_output_addresses'] = {}
					inst['secondary_output_addresses'] = {inst['secondary_output'][0]:out_pointer - out_num_t*(out_w_ep + out_w_hw*out_h_np)}
					# out_pointer += out_h_hw*out_w_hw*out_num_hw
					out_pointer = 3178944#4244032
					inst['weight_address'] = 0
					inst['bias_address'] = 0
				elif 'UNet' in f_model and idx==1:
					inst['input_address'] = inst_addr_info['main_output_addresses'][inst['name']]
					inst['prev_input_address'] = 0
					inst['output_address'] = out_pointer 
					inst['main_output_addresses'] = {inst['main_output'][0]:out_pointer - out_num_t*(out_w_ep + out_w_hw*out_h_np)}
					inst['secondary_output_addresses'] = {}
					out_pointer += out_h_hw*out_w_hw*out_num_hw
					inst['weight_address'] = 256#512			
					inst['bias_address'] = 0
				else:
					inst['output_address'] = 0
					inst['weight_address'] = 0
					inst['input_address'] = 0
					inst['bias_address'] = 0

				weight_pointer += in_num_hw*out_num_hw*conv_kernel_h*conv_kernel_w
				# print(weight_pointer)
				bias_pointer += out_num_hw
				
			else:
				# print(inst['name'])
				# calculate main input address
				inst['input_address'] = main_in_inst['main_output_addresses'][inst['name']]
				# calculate secondary input address
				inst['prev_input_address'] = 0
				if len(secondary_input) > 0:
					secondary_in_inst = get_prev_inst(raw_insts, secondary_input)
					if inst['name'] in secondary_in_inst['secondary_output_addresses'].keys():
						inst['prev_input_address'] = secondary_in_inst['secondary_output_addresses'][inst['name']]

				inst['output_address'] = out_pointer

				if isPool_only(inst):
					if idx != 15:
						# calculate main output addresses
						inst['main_output_addresses'] = {}
						for i, output in enumerate(inst['main_output']):
							if i == 0:
								inst['main_output_addresses'][output] = out_pointer 
								out_pointer += out_h*out_w*out_num_hw 
							else:
								inst['main_output_addresses'][output] = out_pointer# - out_num_t*(out_w_ep + out_w*out_h_np)
								out_pointer += out_h_hw*out_w_hw*out_num_hw 
						# calculate secondary output addresses
						inst['secondary_output_addresses'] = {}
						for output in inst['secondary_output']:
							inst['secondary_output_addresses'][output] = out_pointer 
					else:
						# calculate main output addresses
						inst['main_output_addresses'] = {}
						for i, output in enumerate(inst['main_output']):
							if i == 0:
								inst['main_output_addresses'][output] = out_pointer + out_h*out_w*out_num_hw
								out_pointer +=  out_h_hw*out_w_hw*out_num_hw 
							# else:
							# 	inst['main_output_addresses'][output] = out_pointer# - out_num_t*(out_w_ep + out_w*out_h_np)
							# 	out_pointer += out_h_hw*out_w_hw*out_num_hw 
						# calculate secondary output addresses
						inst['secondary_output_addresses'] = {}
						for output in inst['secondary_output']:
							inst['secondary_output_addresses'][output] = out_pointer
				else:
					inst['main_output_addresses'] = {}
					for i, output in enumerate(inst['main_output']):
						inst['main_output_addresses'][output] = out_pointer - out_num_t*(out_w_ep + out_w_hw*out_h_np)
					# calculate secondary output addresses
					inst['secondary_output_addresses'] = {}
					for output in inst['secondary_output']:
						inst['secondary_output_addresses'][output] = out_pointer - out_num_t*(out_w_ep + out_w_hw*out_h_np)
					out_pointer += out_h_hw*out_w_hw*out_num_hw 
				# print(inst['main_output_addresses'])
				
				# if isPool_only(inst):
				# 	print(idx, inst['main_output_addresses'])


				# print(len(inst['main_output_addresses']), len(inst['secondary_output_addresses']))
				# print(inst['name'], inst['main_output_addresses'], inst['secondary_output_addresses'])
			# 	if inst['in_h']==inst['in_h_hw'] and inst['in_w']==inst['in_w_hw']:
			# 		inst['input_address'] = main_in_inst['main_output_addresses'][inst['name']]
			# 	else:
			# 		if isPool_only(main_in_inst):
			# 			inst['input_address'] = main_in_inst['main_output_addresses'][inst['name']] + main_in_inst['out_num']*main_in_inst['out_h']*main_in_inst['out_w']
			# 		else:
			# 			inst['input_address'] = main_in_inst['main_output_addresses'][inst['name']] - main_in_inst['out_num_t']*(main_in_inst['out_w_ep'] + inst['in_w_hw']*main_in_inst['out_h_np'])

			# 	curr_input_address = inst['input_address']
			# 	same_input_address_offset = 0
			# 	for i in range(idx, 0, -1):
			# 		prev_inst = raw_insts[i-1]
			# 		if curr_input_address == prev_inst['input_address']:
			# 			same_input_address_offset = prev_inst['out_num_hw']*(prev_inst['out_h']+prev_inst['out_h_np']+prev_inst['out_h_sp'])*(prev_inst['out_w']+prev_inst['out_w_wp']+prev_inst['out_w_ep'])
			# 			break
					
			# 	if (out_w_wp>=1 or out_h_np>=1) and not isPool_only(inst):
			# 			inst['main_output_addresses'][main_out_inst['name']] = inst['input_address'] + in_num_hw*in_h_hw*in_w_hw + out_num_t*(out_w_ep + out_w_hw*out_h_np) + same_input_address_offset
			# 	else:
			# 		if isPool_only(main_in_inst):
			# 			prev_offset = main_in_inst['out_num_hw']*(main_in_inst['out_h']+main_in_inst['out_h_np']+main_in_inst['out_h_sp'])*(main_in_inst['out_w']+main_in_inst['out_w_wp']+main_in_inst['out_w_ep'])
			# 			inst['main_output_addresses'][main_out_inst['name']] = inst['input_address'] + in_num_hw*in_h_hw*in_w_hw + prev_offset
			# 		else:
			# 			inst['main_output_addresses'][main_out_inst['name']] = inst['input_address'] + in_num_hw*in_h_hw*in_w_hw + same_input_address_offset
				
			# 	if inst['main_output_addresses'][main_out_inst['name']] % 16 != 0:
			# 		inst['main_output_addresses'][main_out_inst['name']] += (inst['output_address'][main_out_inst['name']] % 16)

				if noConv(inst):
					inst['weight_address'] = 0
					inst['bias_address'] = 0
				else:
					inst['weight_address'] = weight_pointer
					inst['bias_address'] = bias_pointer
					weight_pointer += in_num_hw*out_num_hw*conv_kernel_h*conv_kernel_w
					bias_pointer += out_num_hw
				# print(conv_kernel_h, conv_kernel_w)

				# print(in_num_hw, out_num_hw)
			# if idx>0:
			# 	p_inst = raw_insts[idx-1]
			# 	if p_inst['input_address']==inst['input_address']:
			# 		offset = out_num_hw*out_h_hw*out_w_hw
			# 		inst['output_address'] += offset
			
			# if len(secondary_input) > 0:
			# 	secondary_in_inst = get_prev_inst(raw_insts, secondary_input)
			# 	inst['prev_input_address'] = secondary_in_inst['secondary_output_addresses'][inst['name']]
			# else:
			# 	inst['prev_input_address'] = 0

			# if idx==0:
			# 	if 'ENet' in f_model:
			# 		inst['prev_input_address'] = 672800
			# 	elif 'VGG16' in f_model:
			# 		inst['prev_input_address'] = 0#672800#  for enet


		for in_inst in raw_insts:
			inst = {}
			conv_kernel_h = 1
			conv_kernel_w = 1
			pool_kernel = 1
			conv_stride = 1
			pool_stride = 1
			conv_type = ''
			for op in in_inst['ops']:
				op_params = op['params']
				if op_params['en']:
					if op['name'] == 'Conv':
						conv_type = op_params['type']
						conv_kernel_h = op_params['kernel_h']
						conv_kernel_w = op_params['kernel_w']
						conv_stride = op_params['stride']
						tstride = op_params['tstride']
						dilation_rate = op_params['dilation_rate']
						dilated_kernel_h = conv_kernel_h*dilation_rate-dilation_rate+1
						dilated_kernel_w = conv_kernel_w*dilation_rate-dilation_rate+1
						pool_channels = op_params['in_channels']
						break
					elif op['name'] == 'ConvTranspose':
						conv_type = op_params['type']
						conv_kernel_h = op_params['kernel_h']
						conv_kernel_w = op_params['kernel_w']
						tstride = op_params['tstride']
						dilation_rate = 1#op_params['dilation_rate']
						dilated_kernel_h = conv_kernel_h*dilation_rate-dilation_rate+1
						dilated_kernel_w = conv_kernel_w*dilation_rate-dilation_rate+1
						pool_channels = op_params['in_channels']
						break
					elif op['name'] == 'MaxPool':
						pool_kernel = op_params['kernel_h']
						pool_stride = op_params['stride']
						pool_channels = op_params['in_channels']
						dilated_kernel_h = 1
						dilated_kernel_w = 1
						tstride = 1
						# pool_channel = op_params['channel']

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