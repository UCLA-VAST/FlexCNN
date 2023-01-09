#onnxruntime
import onnxruntime as ort
import onnx as onnx
from onnx import helper
from onnx import TensorProto
import numpy as np
import json
# OrderedDict
from collections import OrderedDict
from onnx import numpy_helper
import math
import argparse

def get_weight_tensors(onnx_model, model_insts):
	tiling_factors = []
	weight_tensors = []
	bias_tensors = []
	model_insts = model_insts[1:]
	for inst in model_insts:
		inst = eval(inst)
		in_num_t = inst['in_num_t']
		out_num_t = inst['out_num_t']
		in_num = inst['in_num']
		out_num = inst['out_num']
		ops = inst['ops']
		for op in ops:
			op_params = op['params']
			if op_params['en'] and (op['name'] == 'Conv' or op['name'] == 'ConvTranspose'):
				conv_name = op_params['name']
				kernel_h = op_params['kernel_h']
				kernel_w = op_params['kernel_w']
				tstride = op_params['tstride']
				for node in onnx_model.graph.node:
					if conv_name == node.name:
						conv_inputs = node.input
						for input in conv_inputs:
							for w in onnx_model.graph.initializer:
								if input == w.name:
									w_np = numpy_helper.to_array(w)
									if len(w_np.shape) == 4:
										tiling_factors.append(
											{'in_num':in_num, 'out_num':out_num, 'in_num_t': in_num_t, 'out_num_t': out_num_t, 'kernel_h': kernel_h, 'kernel_w' : kernel_w, 'tstride': tstride, 'conv_type': op['name']})
										weight_tensors.append(w_np)
									elif len(w_np.shape) == 1:
										bias_tensors.append(w_np) 

	return weight_tensors, tiling_factors

def reorder(arr, in_num, out_num, K, TS):
	if(TS==2):
		if(K==2):
			order = [0,1,2,3]
		elif(K==3):
			order = [8,6,7,2,0,1,5,3,4]
		elif(K==4):
			order = [10,8,11,9,2,0,3,1,14,12,15,13,6,4,7,5]
		elif(K==5):
			order = [24,22,20,23,21,14,12,10,13,11,4,2,0,3,1,19,17,15,18,16,9,7,5,8,6]
	elif(TS==3):
		if(K==3):
			order = [0,1,2,3,4,5,6,7,8]
		elif(K==4):
			order = [15,12,13,14,3,0,1,2,7,4,5,6,11,8,9,10]
		elif(K==5):
			order = [18,15,17,19,16,3,0,2,4,1,13,10,12,14,11,23,20,22,24,21,8,5,7,9,6]
	elif(TS==4):
		if(K==4):
			order = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]
	arr = np.moveaxis(arr, [1, 2, 3], [2, 3, 1])
	arr = arr.reshape((in_num*out_num, K*K))
	arr = np.array([[arr[j][i] for i in order] for j in range(len(arr))])
	arr = arr.reshape((out_num, in_num, K, K))
	return np.moveaxis(arr, [1, 2, 3], [3, 1, 2])  

def reorganize_weights(w, t_dict, arch_dict):
	# read json file
	sa_rows = arch_dict['SA_ROWS']
	sa_simd = arch_dict['SA_SIMD']
	conv_type = t_dict['conv_type']
	in_num_t = t_dict['in_num_t']
	out_num_t = t_dict['out_num_t']
	in_num = t_dict['in_num']
	out_num = t_dict['out_num']

	# Conv: original w layout [out_num][in_num][in_h][in_w] -> new w layout [out_num][in_w][in_h][in_num]
	if conv_type == 'Conv':
		w = np.transpose(w, (0, 2, 3, 1))

	# ConvTranspose: original w layout [in_num][out_num][in_h][in_w] -> new w layout [out_num][in_w][in_h][in_num]
	elif conv_type == 'ConvTranspose':
		w = np.transpose(w, (1, 2, 3, 0))
		w = reorder(w, in_num, out_num, t_dict['kernel_h'], t_dict['tstride'])
		

	# if w.shape[0] < sa_simd, pad with zeros
	if w.shape[3] < sa_simd:
		sa_simd = max(sa_simd, in_num_t)
		w = np.pad(w, ((0, 0), (0, 0), (0, 0), (0, sa_simd - w.shape[3])), 'constant')
	# if w.shape[3] < sa_rows, pad with zeros
	if w.shape[0] < sa_rows:
		sa_rows = max(sa_rows, out_num_t)
		w = np.pad(w, ((0, sa_rows - w.shape[0]), (0, 0), (0, 0), (0, 0)), 'constant')

	kernel_h = w.shape[1]
	kernel_w = w.shape[2]

	# TODO: reorder weights for transposed convolution
	
	weights_reorg = np.zeros((int(math.ceil(float(out_num) / out_num_t)), int(math.ceil(float(in_num) / in_num_t)), out_num_t, kernel_h, kernel_w, in_num_t))
	for o1 in range(int(math.ceil(float(out_num) / out_num_t))):
		for i1 in range(int(math.ceil(float(in_num) / in_num_t))):
			for o2 in range(out_num_t):
				for p in range(kernel_h):
					for q in range(kernel_w):
						for i2 in range(in_num_t):
							L2 = o1*out_num_t+o2
							L1 = i1*in_num_t+i2
							if (o1 * out_num_t + o2 < out_num) and (i1 * in_num_t + i2 < in_num):
								weights_reorg[o1][i1][o2][p][q][i2] = w[L2][p][q][L1]
							else:
								weights_reorg[o1][i1][o2][p][q][i2] = float(0.0)	
	return weights_reorg

def run(model_file, onnx_file, arch_file, output_file):
	onnx_model = onnx.load(onnx_file)
	# read csv file
	model_insts = open(model_file, 'r').readlines()
	# clear output file
	open(output_file, 'w').close()
	output_file = open(output_file, 'a')
	with open(arch_file) as json_file:
		arch_dict = json.load(json_file)
	weight_tensors, tiling_factors = get_weight_tensors(onnx_model, model_insts)
	for w, t_dict in zip(weight_tensors, tiling_factors):
		reorganized_weights = reorganize_weights(w, t_dict, arch_dict)
		reorganized_weights.tofile(output_file, sep="\n", format='%s')
		output_file.write("\n")
	output_file.close()

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Weight generation and reordering')

	parser.add_argument('-ox', '--onnx', metavar='ONNX', default='./model.onnx', help='onnx model', dest='onnx_file')
	parser.add_argument('-mi', '--model_in', metavar='MODEL_IN', default='./network.csv', help='model description', dest='model_file')
	parser.add_argument('-a', '--architecture', metavar='ARCHITECTURE', default='./architecture.json', help='architecture configuration', dest='arch_file')
	parser.add_argument('-o', '--output', metavar='OUTPUT', default='./network_out.csv', help='output description', dest='output_file')

	args = parser.parse_args()
	run(args.model_file, args.onnx_file, args.arch_file, args.output_file)