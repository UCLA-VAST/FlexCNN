
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

def get_intermediate_tensors(onnx_file, input_shape):

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
	onnx_input_shape = [model.graph.input[0].type.tensor_type.shape.dim[i].dim_value for i in range(1, len(model.graph.input[0].type.tensor_type.shape.dim))]
	design_input_shape = [input_shape['IN_H'], input_shape['IN_W'], input_shape['IN_NUM']]
	
	# excute onnx
	ort_session = ort.InferenceSession(model.SerializeToString())
	outputs = [x.name for x in ort_session.get_outputs()]
	input_tensor = np.random.randn(1,*onnx_input_shape).astype(np.float32)
	# input_tensor = np.ones((1,*onnx_input_shape)).astype(np.float32)
	ort_outs = ort_session.run(outputs, {input_name: input_tensor} )
	ort_outs = OrderedDict(zip(outputs, ort_outs))
	intermediate_outputs = ort_outs
	if(onnx_input_shape != design_input_shape):
		# transpose to channel last
		input_tensor = np.transpose(input_tensor, (0, 3, 1, 2))

	return intermediate_outputs, input_tensor

def reorganize_input_tensor(inst, arch_dict, input_tensor, onnx_file, force_dl2=False, layer_num=0):
	
	# input shape channel last
	in_num_t = inst['in_num_t']
	in_h_t = inst['in_h_t']
	in_w_t = inst['in_w_t']
	in_num = inst['in_num']

	in_h = inst['in_h']
	in_w = inst['in_w']
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

	# no padding DL2
	if (((kernel_h-stride)==0 and (kernel_w-stride)==0) or force_dl2) and 'UNet' not in onnx_file:
		if in_num < max(sa_simd, in_num_t):
			input_tensor = np.pad(input_tensor, ((0,0),(0,0),(0,0),(0,max(sa_simd, in_num_t)-in_num)), 'constant')
		reorganized_input_tensor = np.zeros(input_tensor.shape)
		reorganized_input_tensor = reorganized_input_tensor.flatten()
		in_h = input_tensor.shape[1]
		in_w = input_tensor.shape[2]
		for i1 in range(math.ceil(in_num / in_num_t)):
			for w1 in range(math.ceil(in_w / in_w_t)):
				for h1 in range(math.ceil(in_h / in_h_t)):	
					for h2 in range(in_h_t):
						for w2 in range(in_w_t):
							for i2 in range(in_num_t):
								i = i1 * in_num_t + i2
								h = h1 * in_h_t + h2
								w = w1 * in_w_t + w2
								
								I1 = i1 * in_h * in_w * in_num_t
								# I2 = h1 * in_w * in_h_t * in_num_t
								# I3 = w1 * in_h_t * in_w_t * in_num_t
								I2 = w1 * in_h * in_w_t * in_num_t
								I3 = h1 * in_h_t * in_w_t * in_num_t
								I4 = h2 * in_w_t * in_num_t
								I5 = w2 * in_num_t
								I6 = i2
								idx = I6 + I5 + I4 + I3 + I2 + I1
								# print(idx, i, h, w)
								if (i < in_num):
									reorganized_input_tensor[idx] = input_tensor[0][h][w][i]
	# padding DL1
	else:
		# pad the end of height and width of input tensor with zeros
		if 'ENet' in onnx_file:
			input_tensor = np.pad(input_tensor, ((0,0),(0,2),(0,2),(0,0)), 'constant') #TODO: get the corrent padding automatically
		elif 'VGG16' in onnx_file:
			input_tensor = np.pad(input_tensor, ((0,0),(1,1),(1,1),(0,0)), 'constant') #TODO: get the corrent padding automatically
		elif 'UNet' in onnx_file:
			if layer_num == 2:
				input_tensor = np.pad(input_tensor, ((0,0),(2,2),(2,2),(0,0)), 'constant') #TODO: get the corrent padding automatically

		# print(input_tensor1.shape)
		# if input channel < SA_SIMD, pad the end of input tensor with zeros
		if in_num < max(sa_simd, in_num_t):
			input_tensor = np.pad(input_tensor, ((0,0),(0,0),(0,0),(0,max(sa_simd, in_num_t)-in_num)), 'constant')
		
		# create new flattened input tensor with size input tensor
		reorganized_input_tensor = np.zeros(input_tensor.shape)
		# flatten input tensor
		reorganized_input_tensor = reorganized_input_tensor.flatten()
		# first input tensor
		in_h = input_tensor.shape[1]
		in_w = input_tensor.shape[2]
		for i1 in range(math.ceil(in_num / in_num_t)):
			for h in range(in_h):
				for w in range(in_w):
					for i2 in range(in_num_t):
						i = i1 * in_num_t + i2

						I1 = i1 * in_h * in_w * in_num_t 
						I2 = h * in_w * in_num_t 
						I3 = w * in_num_t + i2
						idx = I3 + I2 + I1
						if (i < in_num):
							reorganized_input_tensor[idx] = input_tensor[0][h][w][i]

	return reorganized_input_tensor

def print_hr(tensor, outFile):
	tmp_test_file = open(outFile, 'a')
	print(tensor.shape, file=tmp_test_file)
	print(np.sum(tensor), file=tmp_test_file)
	# print(tensor.shape[1], tensor.shape[2], tensor.shape[3])
	for c in range(tensor.shape[1]):
		print('-'*20+'channel: '+str(c)+'-'*20, file=tmp_test_file)
		for h in range(tensor.shape[2]):
			for w in range(tensor.shape[3]):			
				print('{:>10.5f}'.format(tensor[0][c][h][w]), end='\t', file=tmp_test_file)
			print(file=tmp_test_file)

def run(model_file, onnx_file, arch_file, output_path):
	onnx_model = onnx.load(onnx_file)
	# read csv file
	model_insts = open(model_file, 'r').readlines()
	input_shape = eval(model_insts[0])

	model_insts = model_insts[1:]
	# clear output file
	with open(arch_file) as json_file:
		arch_dict = json.load(json_file)

	intermediate_tensors, input_tensor = get_intermediate_tensors(onnx_file, input_shape)

	if 'ENet' in onnx_file:
		reorganized_input_tensor1 = reorganize_input_tensor(eval(model_insts[0]), arch_dict, input_tensor, onnx_file)
		reorganized_input_tensor2 = reorganize_input_tensor(eval(model_insts[0]), arch_dict, input_tensor, onnx_file, True)
		reorganized_input_tensor1.tofile(output_path+'inputs_1.dat', sep="\n", format='%s')
		reorganized_input_tensor2.tofile(output_path+'inputs_2.dat', sep="\n", format='%s')
	elif 'VGG16' in onnx_file:
		reorganized_input_tensor1 = reorganize_input_tensor(eval(model_insts[0]), arch_dict, input_tensor, onnx_file)
		reorganized_input_tensor2 = np.zeros((0))
		reorganized_input_tensor1.tofile(output_path+'inputs_1.dat', sep="\n", format='%s')
		reorganized_input_tensor2.tofile(output_path+'inputs_2.dat', sep="\n", format='%s')
	elif 'UNet' in onnx_file:
		reorganized_input_tensor1 = reorganize_input_tensor(eval(model_insts[0]), arch_dict, input_tensor, onnx_file, False, 1)
		reorganized_input_tensor2 = reorganize_input_tensor(eval(model_insts[1]), arch_dict, input_tensor, onnx_file, False, 2)
		reorganized_input_tensor1.tofile(output_path+'inputs_1.dat', sep="\n", format='%s')
		reorganized_input_tensor2.tofile(output_path+'inputs_2.dat', sep="\n", format='%s')

	output_tensor = 0
	for idx, inst in enumerate(model_insts):
		inst = eval(inst)
		output_name = inst['output_name'][0]
		for output in intermediate_tensors:
			# print(output, output_name)
			if output == output_name:
				# print(output_path+'tmp/L')
				intermediate_tensors[output].tofile(output_path+'/outputs/L'+str(idx+1)+'_outputs.dat', sep="\n", format='%s')
				break
	# debug
	# idx = 0
	# for output in intermediate_tensors:
	# 	if len(intermediate_tensors[output].shape) == 4:
	# 		print('printing output: ', output)
	# 		print(output, file=open(output_path+'/outputs/D_L'+str(idx+1)+'_outputs.dat', 'w'))
	# 		print_hr(intermediate_tensors[output], output_path+'/outputs/D_L'+str(idx+1)+'_outputs.dat')
	# 		idx += 1
		
if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Weight generation and reordering')

	parser.add_argument('-ox', '--onnx', metavar='ONNX', default='./model.onnx', help='onnx model', dest='onnx_file')
	parser.add_argument('-mi', '--model_in', metavar='MODEL_IN', default='./network.csv', help='model description', dest='model_file')
	parser.add_argument('-a', '--architecture', metavar='ARCHITECTURE', default='./architecture.json', help='architecture configuration', dest='arch_file')
	parser.add_argument('-o', '--output', metavar='OUTPUT', default='./network_out.csv', help='output description', dest='output_path')

	args = parser.parse_args()
	run(args.model_file, args.onnx_file, args.arch_file, args.output_path)