
import json
import sys
import os
import numpy as np
import argparse

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Design space exploration.')
	parser.add_argument('-m', '--model', metavar='MODEL', default='ENet', help='model name', dest='model')
	parser.add_argument('-dp', '--design-parameters', metavar='DESIGN_PARAMETERS', default='./architecture.json', help='architecture configuration', dest='design_params')
	parser.add_argument('-ar', '--architecture', metavar='ARCHITECTURE', default='./architecture.json', help='architecture configuration', dest='architecture')
	parser.add_argument('-o', '--output', metavar='OUTPUT', default='../../designs/', help='output directory', dest='output')
	parser.add_argument('-mt', '--mem-type', metavar='MEM_TYPE', default=0, help='mem type', dest='mem_type')
	parser.add_argument('-c', '--code', metavar='CODE', default=0, help='HLS, TAPA, ..', dest='code')
	args = parser.parse_args()


	design_name = args.model
	mem_type = int(args.mem_type)
	design_path = args.output
	design_path_model = design_path + '/' + design_name
	STREAM_VSA_PATH = os.environ['STREAM_VSA_PATH']
	
	# generate hw design
	if (args.code == 'HLS'):
		os.system('python $STREAM_VSA_PATH/auto_compile/design_generation/HLS/systolic_array_kernel/h_to_json.py \
		-dp %s \
		-mt %d\
		-i %s/auto_compile/data/insts/%s_instructions.dat \
		-o %s'\
		%(args.design_params, mem_type, STREAM_VSA_PATH, design_name, design_path_model))
		os.system('cd $STREAM_VSA_PATH/auto_compile/design_generation/HLS/;./design_prepare.sh %s; mkdir %s; cp -r design/* %s'%(args.architecture, design_path_model, design_path_model))

	elif(args.code == 'TAPA_1'):
		os.system('python $STREAM_VSA_PATH/auto_compile/design_generation/TAPA_1/systolic_array_kernel/h_to_json.py \
		-dp %s \
		-mt %d\
		-i %s/auto_compile/data/insts/%s_instructions.dat \
		-o %s'\
		%(args.design_params, mem_type, STREAM_VSA_PATH, design_name, design_path_model))
		os.system('cd $STREAM_VSA_PATH/auto_compile/design_generation/TAPA_1/;./design_prepare.sh %s; mkdir %s; cp -r design/* %s'%(args.architecture, design_path_model, design_path_model))


# import json
# import sys
# import os
# import numpy as np
# import argparse

# if __name__ == "__main__":
# 	parser = argparse.ArgumentParser(description='Design space exploration.')
# 	parser.add_argument('-m', '--model', metavar='MODEL', default='ENet', help='model name', dest='model')
# 	parser.add_argument('-dp', '--design-parameters', metavar='DESIGN_PARAMETERS', default='./architecture.json', help='architecture configuration', dest='design_params')
# 	parser.add_argument('-ar', '--architecture', metavar='ARCHITECTURE', default='./architecture.json', help='architecture configuration', dest='architecture')
# 	parser.add_argument('-o', '--output', metavar='OUTPUT', default='../../designs/', help='output directory', dest='output')
# 	parser.add_argument('-mt', '--mem-type', metavar='MEM_TYPE', default=0, help='mem type', dest='mem_type')
# 	parser.add_argument('-c', '--code', metavar='CODE', default=0, help='HLS, TAPA, ..', dest='code')
# 	args = parser.parse_args()

# 	design_name = args.model
# 	mem_type = int(args.mem_type)
# 	design_path = args.output
# 	design_path_model = design_path + '/' + design_name
# 	STREAM_VSA_PATH = os.environ['STREAM_VSA_PATH']
# 	architecture = args.architecture
	
# 	# generate hw design
# 	if (args.code == 'HLS'):
# 		os.system('python $STREAM_VSA_PATH/auto_compile/design_generation/HLS/systolic_array_kernel/h_to_json.py \
# 		-dp %s \
# 		-ar %s \
# 		-mt %d \
# 		-i %s/auto_compile/data/insts/%s_instructions.dat \
# 		-o %s'\
# 		%(args.design_params, architecture, mem_type, STREAM_VSA_PATH, design_name, design_path_model))
# 		os.system('cd $STREAM_VSA_PATH/auto_compile/design_generation/HLS/;./design_prepare.sh; mkdir %s; cp -r design/* %s'%(design_path_model, design_path_model))

# 	elif(args.code == 'TAPA_1'):
# 		os.system('python $STREAM_VSA_PATH/auto_compile/design_generation/TAPA_1/systolic_array_kernel/h_to_json.py \
# 		-dp %s \
# 		-ar %s \
# 		-mt %d\
# 		-i %s/auto_compile/data/insts/%s_instructions.dat \
# 		-o %s'\
# 		%(args.design_params, architecture, mem_type, STREAM_VSA_PATH, design_name, design_path_model))
# 		os.system('cd $STREAM_VSA_PATH/auto_compile/design_generation/TAPA_1/;./design_prepare.sh; mkdir %s; cp -r design/* %s'%(design_path_model, design_path_model))
