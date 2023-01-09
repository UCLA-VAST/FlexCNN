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

def get_bias_tensors(onnx_model, model_insts):
	tiling_factors = []
	bias_tensors = []
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
									if len(w_np.shape) == 1:
										tiling_factors.append(
											{'in_num':in_num, 'out_num':out_num, 'in_num_t': in_num_t, 'out_num_t': out_num_t, 'kernel_h': kernel_h, 'kernel_w' : kernel_w, 'tstride': tstride, 'conv_type': op['name']})
										bias_tensors.append(w_np)
									elif len(w_np.shape) == 1:
										bias_tensors.append(w_np) 
			elif op_params['en'] and op['name'] == 'BatchNormalization':
				op_name = op_params['name']
				for node in onnx_model.graph.node:
					if op_name == node.name:
						op_inputs = node.input
						for input in op_inputs:
							for w in onnx_model.graph.initializer:
								if input == w.name:
									w_np = numpy_helper.to_array(w)
									if len(w_np.shape) == 1:
										tiling_factors.append(
											{'in_num':in_num, 'out_num':out_num, 'in_num_t': in_num_t, 'out_num_t': out_num_t, 'kernel_h': kernel_h, 'kernel_w' : kernel_w, 'tstride': tstride, 'conv_type': op['name']})
										bias_tensors.append(w_np)
									elif len(w_np.shape) == 1:
										bias_tensors.append(w_np) 
	return bias_tensors, tiling_factors

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
	bias_tensors, tiling_factors = get_bias_tensors(onnx_model, model_insts)
	for b, t_dict in zip(bias_tensors, tiling_factors):
		# reorganized_biases = reorganize_biases(b, t_dict, arch_dict)
		b.tofile(output_file, sep="\n", format='%s')
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