
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

# fix ranodm seed
np.random.seed(0)

def get_intermediate_tensors(onnx_file):

	# # add all intermediate outputs to onnx net
	ort_session = ort.InferenceSession(onnx_file)
	org_outputs = [x.name for x in ort_session.get_outputs()]

	model = onnx.load(onnx_file)
	for node in model.graph.node:
		for output in node.output:
			if output not in org_outputs:
				model.graph.output.extend([onnx.ValueInfoProto(name=output)])

	# #get input name
	input_name = model.graph.input[0].name
	input_shape = [model.graph.input[0].type.tensor_type.shape.dim[i].dim_value for i in range(1, len(model.graph.input[0].type.tensor_type.shape.dim))]

	# excute onnx
	ort_session = ort.InferenceSession(model.SerializeToString())
	outputs = [x.name for x in ort_session.get_outputs()]
	input_tensor = np.random.randn(1,*input_shape).astype(np.float32)
	ort_outs = ort_session.run(outputs, {input_name: input_tensor} )
	ort_outs = OrderedDict(zip(outputs, ort_outs))

	intermediate_outputs = ort_outs
	return intermediate_outputs, input_tensor

def reorganize_output_tensor(model_insts, arch_dict, output_buffer):
	
	inst = eval(model_insts[1]) #TODO: get the corrent input automatically
	# in_num_t = inst['in_num_t']
	out_num_t = inst['out_num_t']
	in_h_t = inst['in_h_t']
	in_w_t = inst['in_w_t']
	# in_num = inst['in_num']
	out_num = inst['out_num']
	out_h = inst['out_h']
	out_w = inst['out_w']
	# in_h = inst['in_h']
	# in_w = inst['in_w']
	stride = 0
	kernel_h = 0
	kernel_w = 0
	for op in inst['ops']:
		if op['name'] == 'Conv':
			op_params = op['params']
			if op_params['en']:
				stride = op_params['stride']
				kernel_h = op_params['kernel_h']
				kernel_w = op_params['kernel_w']
				break
	sa_simd = arch_dict['SA_SIMD']
	sa_rows = arch_dict['SA_ROWS']

	output_tensor = np.zeros((out_num, out_h, out_w))
	out_h_t = in_h_t//stride
	out_w_t = in_w_t//stride

	for o1 in range(math.ceil(out_num / out_num_t)):
		for h1 in range(math.ceil(out_h / out_h_t)):
			for w1 in range(math.ceil(out_w / out_w_t)):
				for h2 in range(out_h_t):
					for w2 in range(out_w_t):
						for o2 in range(out_num_t):
							o = o1 * out_num_t + o2
							h = h1 * out_h_t + h2
							w = w1 * out_w_t + w2
							
							I1 = o1 * out_h * out_w * out_num_t
							I2 = h1 * out_w * out_h_t * out_num_t
							I3 = w1 * out_h_t * out_w_t * out_num_t
							I4 = h2 * out_w_t * out_num_t
							I5 = w2 * out_num_t
							I6 = o2
							idx = I6 + I5 + I4 + I3 + I2 + I1
							if (o < out_num):
								output_tensor[o][h][w] = output_buffer[idx]
	# else:
	# 	# print(input_tensor.shape[1], input_tensor.shape[2])
	# 	in_h = input_tensor.shape[1]
	# 	in_w = input_tensor.shape[2]
	# 	for i1 in range(math.ceil(in_num / in_num_t)):
	# 		for h in range(in_h):
	# 			for w in range(in_w):
	# 				for i2 in range(in_num_t):
	# 					i = i1 * in_num_t + i2

	# 					I1 = i1 * in_h * in_w * in_num_t 
	# 					I2 = h * in_w * in_num_t 
	# 					I3 = w * in_num_t + i2
	# 					idx = I3 + I2 + I1
	# 					if (i < in_num):
	# 						# if idx == 2304:
	# 						# 	print(idx, i1, i2, h, w, input_tensor[0][h][w][i])
	# 						reorganized_input_tensor[idx] = input_tensor[0][h][w][i]
	# 	# print(len(reorganized_input_tensor))
	return output_tensor

def run(sw_file, hw_file, model_file, arch_file, output_path):
	# read csv file
	model_insts = open(model_file, 'r').readlines()
	# clear output file
	with open(arch_file) as json_file:
		arch_dict = json.load(json_file)
	
	sw_tensor = open(sw_file, 'r').readlines()
	# convert strings to float numbers
	sw_tensor = [float(x) for x in sw_tensor]
	sw_tensor = np.array(sw_tensor)
	sw_tensor = sw_tensor.reshape(13, 144, 144)

	hw_tensor = open(hw_file, 'r').readlines()
	# convert strings to float numbers
	hw_tensor = [float(x) for x in hw_tensor]
	hw_tensor = np.array(hw_tensor)
	hw_tensor = reorganize_output_tensor(model_insts, arch_dict, hw_tensor)
	tmp_test_file = open('../data/test/out_compare.txt', 'w')
	print('-'*20+'hw_tensor'+'-'*20, file=tmp_test_file)
	for c in range(hw_tensor.shape[0]):
		print('-'*20+'channel: '+str(c)+'-'*20, file=tmp_test_file)
		for h in range(hw_tensor.shape[1]):
			for w in range(hw_tensor.shape[2]):			
				print('{:>10.5f}'.format(hw_tensor[c][h][w]), end='\t', file=tmp_test_file)
			print(file=tmp_test_file)
			
	print('-'*20+'sw_tensor'+'-'*20, file=tmp_test_file)
	for c in range(sw_tensor.shape[0]):
		print('-'*20+'channel: '+str(c)+'-'*20, file=tmp_test_file)
		for h in range(sw_tensor.shape[1]):
			for w in range(sw_tensor.shape[2]):			
				print('{:>10.5f}'.format(sw_tensor[c][h][w]), end='\t', file=tmp_test_file)
			print(file=tmp_test_file)
	# print(sw_tensor[0][0][0], hw_tensor[0][0][0])
	# print(sw_tensor[0][0][1], hw_tensor[0][0][1])
	# print(sw_tensor[0][1][0], hw_tensor[0][1][0])
	# print(sw_tensor[0][1][1], hw_tensor[0][1][1])
	# print(sw_tensor[1][0][0], hw_tensor[1][0][0])
	# print(sw_tensor[-1][-1][-1], hw_tensor[-1][-1][-1])
	# # compare the two tensors
	# print(np.allclose(sw_tensor, hw_tensor, atol=1e-01))
	# print(sum(sum(sum(sw_tensor))))
	# print(sum(sum(sum(hw_tensor))))



if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Weight generation and reordering')

	parser.add_argument('-sw', '--sw_file', metavar='ONNX', default='./model.onnx', help='onnx model', dest='sw_file')
	parser.add_argument('-hw', '--hw_file', metavar='MODEL_IN', default='./network.csv', help='model description', dest='hw_file')
	parser.add_argument('-mi', '--model_in', metavar='MODEL_IN', default='./network.csv', help='model description', dest='model_file')
	parser.add_argument('-a', '--architecture', metavar='ARCHITECTURE', default='./architecture.json', help='architecture configuration', dest='arch_file')
	parser.add_argument('-o', '--output', metavar='OUTPUT', default='./network_out.csv', help='output description', dest='output_path')

	args = parser.parse_args()
	run(args.sw_file, args.hw_file, args.model_file, args.arch_file, args.output_path)