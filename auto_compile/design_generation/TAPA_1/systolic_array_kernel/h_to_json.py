#read json file
import json
import sys
import os
import numpy as np
import argparse
import math

def get_mem_params(inst_file, insts_num):
	instFile = open(inst_file, 'r')
	lines = instFile.readlines()
	insts = []
	for i in range(insts_num):
		inst = []
		for line in range(0,6):
			instLine = lines[i*7+line].split()
			for num in instLine:
				inst.append(int(num))
		insts.append(inst)

	instDicts = []

	for inst in insts:
		instDict = {}
		instDict['IN_NUM_HW'	      ] = inst[0 ]
		instDict['OUT_NUM_HW'	      ] = inst[1 ]
		instDict['IN_H_HW'		      ] = inst[2 ]
		instDict['IN_W_HW'		      ] = inst[3 ]
		instDict['OUT_H_NP'         ] = inst[4 ]
		instDict['OUT_H_SP'         ] = inst[5 ]
		instDict['OUT_W_EP'         ] = inst[6 ]
		instDict['OUT_W_WP'         ] = inst[7 ]
		instDict['IN_NUM'			      ] = inst[8 ]
		instDict['OUT_NUM'		      ] = inst[9 ]
		instDict['IN_H'				      ] = inst[10]
		instDict['IN_W'				      ] = inst[11]
		instDict['OUT_H'			      ] = inst[12]
		instDict['OUT_W'			      ] = inst[13]
		instDict['CIN_OFFSET'	      ] = inst[14]
		instDict['WEIGHT_OFFSET'  	] = inst[15]
		instDict['BIAS_OFFSET'		  ] = inst[16]
		instDict['COUT_OFFSET'		  ] = inst[17]
		instDict['FILTER_S1'	    	] = inst[18]
		instDict['FILTER_S2_H'		  ] = inst[19]
		instDict['FILTER_S2_W'		  ] = inst[20]
		instDict['STRIDE'			      ] = inst[21]
		instDict['EN'				        ] = inst[22]
		instDict['PREV_CIN_OFFSET'	] = inst[23]
		instDict['IN_NUM_T'			    ] = inst[24]
		instDict['OUT_NUM_T'		    ] = inst[25]
		instDict['IN_H_T'			      ] = inst[26]
		instDict['IN_W_T'			      ] = inst[27]
		instDict['BATCH_NUM'		    ] = inst[28]
		instDict['TASK_NUM1'		    ] = inst[29]
		instDict['TASK_NUM2'		    ] = inst[30]
		instDict['LOCAL_ACCUM_NUM'	] = inst[31]
		instDict['LOCAL_REG_NUM'	  ] = inst[32]
		instDict['ROW_IL_FACTOR'	  ] = inst[33]
		instDict['COL_IL_FACTOR'	  ] = inst[34]
		instDict['CONV_TYPE'		    ] = inst[35]
		instDict['FILTER_D0_H'	    ] = inst[36]
		instDict['FILTER_D0_W'	    ] = inst[37]
		instDict['FILTER_D1_H'	    ] = inst[38]
		instDict['FILTER_D1_W'	    ] = inst[39]
		instDict['DILATION_RATE'	  ] = inst[40]
		instDict['TCONV_STRIDE'		  ] = inst[41]
		instDict['K_NUM'			      ] = inst[42]
		instDict['KH_0'			        ] = inst[43]
		instDict['KH_1'			        ] = inst[44]
		instDict['KH_2'			        ] = inst[45]
		instDict['KH_3'			        ] = inst[46]
		instDict['KW_0'			        ] = inst[47]
		instDict['KW_1'			        ] = inst[48]
		instDict['KW_2'			        ] = inst[49]
		instDict['KW_3'			        ] = inst[50]
		instDict['POOL_CH'  	      ] = inst[51]
		instDict['POOL_CH_HW'	      ] = inst[52]
		instDict['OUT_H_HW'] = instDict['OUT_H'] + instDict['OUT_H_NP'] + instDict['OUT_H_SP']
		instDict['OUT_W_HW'] = instDict['OUT_W'] + instDict['OUT_W_EP'] + instDict['OUT_W_WP']
		instDicts.append(instDict)

	# get max ROW_IL_FACTOR from instDicts
	max_cin_buff = 0
	max_cin_prev_buff = 0
	max_weight_buff = 0
	max_cout_buff = 0
	max_row_il_factor = 0
	max_col_il_factor = 0
	max_data0_buf_size = 0
	max_data1_buf_size = 0
	max_data2_buf_size = 0
	max_local_reg_num = 0
	max_transfer_reg_num = 0
	for inst in instDicts:
		upsample_factor = math.ceil(inst['OUT_W']/inst['IN_W'])
		downsample_factor = math.ceil(inst['IN_W']/inst['OUT_W'])
		in_num_t = inst['IN_NUM_T']
		out_num_t = inst['OUT_NUM_T']
		in_h_t = inst['IN_H_T']
		in_w_t = inst['IN_W_T']
		filter_d0_h = inst['FILTER_D0_H']
		filter_d0_w = inst['FILTER_D0_W']
		filter_d1_h = inst['FILTER_D1_H']
		filter_d1_w = inst['FILTER_D1_W']
		stride = inst['STRIDE']
		row_il_factor = inst['ROW_IL_FACTOR']
		col_il_factor = inst['COL_IL_FACTOR']
		local_reg_num = inst['LOCAL_REG_NUM']
		transfer_reg_num = local_reg_num*inst['K_NUM']
		local_reg_num = local_reg_num*upsample_factor*upsample_factor


		out_h_t = (in_h_t//downsample_factor)*upsample_factor
		out_w_t = (in_w_t//downsample_factor)*upsample_factor

		cin_buff = in_num_t*(in_h_t+filter_d0_h-stride)*(in_w_t+filter_d0_w-stride)
		if 'UNet' in inst_file:
			cin_prev_buff = out_num_t*out_h_t*out_w_t
		else:
			cin_prev_buff = inst['POOL_CH']*(in_h_t*downsample_factor)*(in_w_t*downsample_factor)
		weight_buff = in_num_t*out_num_t*filter_d1_h*filter_d1_w
		cout_buff = out_num_t*out_h_t*out_w_t
		data0_buf_size = in_num_t*(in_h_t+filter_d0_h-stride)*(col_il_factor+filter_d0_w-stride)
		data1_buf_size = in_num_t*row_il_factor*filter_d1_h*filter_d1_w
		data2_buf_size = out_num_t*max(in_h_t, out_h_t)*col_il_factor*upsample_factor

		if cin_buff > max_cin_buff:
			max_cin_buff = cin_buff
		if cin_prev_buff > max_cin_prev_buff:
			max_cin_prev_buff = cin_prev_buff
		if weight_buff > max_weight_buff:
			max_weight_buff = weight_buff
		if cout_buff > max_cout_buff:
			max_cout_buff = cout_buff
		
		if data0_buf_size > max_data0_buf_size:
			max_data0_buf_size = data0_buf_size
		if data1_buf_size > max_data1_buf_size:
			max_data1_buf_size = data1_buf_size
		if data2_buf_size > max_data2_buf_size:
			max_data2_buf_size = data2_buf_size

		if row_il_factor > max_row_il_factor:
			max_row_il_factor = row_il_factor
		if col_il_factor > max_col_il_factor:
			max_col_il_factor = col_il_factor
		if local_reg_num > max_local_reg_num:
			max_local_reg_num = local_reg_num
		if transfer_reg_num > max_transfer_reg_num:
			max_transfer_reg_num = transfer_reg_num
		# if inst['IN_NUM_T']*(inst['IN_H_T']+inst['FILTER_S2_H']-1)*(inst['IN_W_T']*upsample_factor+inst['FILTER_S2_W']-1) > max_data0_buf_size:
		#   max_data0_buf_size = inst['IN_NUM_T']*(inst['IN_H_T']+inst['FILTER_S2_H']-1)*(inst['IN_W_T']*upsample_factor+inst['FILTER_S2_W']-1)
		# if inst['IN_NUM_T']*inst['ROW_IL_FACTOR']*inst['FILTER_S2_H']*inst['FILTER_S2_W'] > max_data1_buf_size:
		#   max_data1_buf_size = inst['IN_NUM_T']*inst['ROW_IL_FACTOR']*inst['FILTER_D1_H']*inst['FILTER_D1_W']
		# if inst['OUT_NUM_T']*inst['IN_H_T']*upsample_factor*inst['COL_IL_FACTOR'] > max_data2_buf_size:
		#   max_data2_buf_size = inst['OUT_NUM_T']*inst['IN_H_T']*upsample_factor*inst['COL_IL_FACTOR']
	max_buff_dict = {}
	max_buff_dict['CIN_BUFF'] = max_cin_buff
	max_buff_dict['CIN_PREV_BUFF'] = max_cin_prev_buff
	max_buff_dict['WEIGHT_BUFF'] = max_weight_buff
	max_buff_dict['COUT_BUFF'] = max_cout_buff
	max_buff_dict['DATA0_BUF_SIZE'] = max_data0_buf_size
	max_buff_dict['DATA1_BUF_SIZE'] = max_data1_buf_size
	max_buff_dict['DATA2_BUF_SIZE'] = max_data2_buf_size
	max_buff_dict['ROW_IL_FACTOR'] = max_row_il_factor
	max_buff_dict['COL_IL_FACTOR'] = max_col_il_factor
	max_buff_dict['LOCAL_REG_NUM'] = max_local_reg_num
	max_buff_dict['TRANSFER_REG_NUM'] = max_transfer_reg_num
	return max_buff_dict
	# print('max_cin_buff:', max_cin_buff)
	# print('max_cin_prev_buff:', max_cin_prev_buff)
	# print('max_weight_buff:', max_weight_buff)
	# print('max_cout_buff:', max_cout_buff)

	# print('max_row_il_factor:', max_row_il_factor)
	# print('max_col_il_factor:', max_col_il_factor)
	# print('max_local_reg_num:', max_local_reg_num)
	# print('max_transfer_reg_num:', max_transfer_reg_num)
	# print('max_data0_buf_size:', max_data0_buf_size)
	# print('max_data1_buf_size:', max_data1_buf_size)
	# print('max_data2_buf_size:', max_data2_buf_size)

if __name__ == "__main__":

	parser = argparse.ArgumentParser(description='Design space exploration.')
	parser.add_argument('-dp', '--design-parameters', metavar='DESIGN_PARAMETERS', default='./architecture.json', help='architecture configuration', dest='design_params')
	parser.add_argument('-mt', '--mem-type', metavar='MEM_TYPE', default=0, help='mem type', dest='mem_type')
	parser.add_argument('-i', '--instructions', metavar='INSTRUCTIONS', default='../../data/instructions.dat', help='input file', dest='insts')
	parser.add_argument('-o', '--output', metavar='OUTPUT', default='../../designs/', help='output directory', dest='output')
	args = parser.parse_args()

	design_path = args.output
	# load json file design_params
	design_params = json.load(open(args.design_params))
	INSTS_NUM = design_params['INSTS_NUM']
	buff_dict = get_mem_params(args.insts, INSTS_NUM)
	K = design_params['K']
	TS = design_params['TSTRIDE']
	DR = design_params['DILATION_RATE']
	IN_NUM = design_params['IN_NUM']
	OUT_NUM = design_params['OUT_NUM']
	IN_IMG_H = design_params['IN_H'] + K -1
	IN_IMG_W = design_params['IN_W'] + K -1

	IN_NUM_T = design_params['IN_NUM_T']
	OUT_NUM_T = design_params['OUT_NUM_T']
	IN_H_T = design_params['IN_H_T']
	IN_W_T = design_params['IN_W_T']
	OUT_H_T = IN_H_T*TS
	OUT_W_T = IN_W_T*TS

	IN_NUM_T_MAX = design_params['IN_NUM_T_MAX']
	OUT_NUM_T_MAX = design_params['OUT_NUM_T_MAX']
	IN_H_T_MAX = design_params['IN_H_T_MAX']
	IN_W_T_MAX = design_params['IN_W_T_MAX']
	OUT_H_T_MAX = IN_H_T_MAX*TS
	OUT_W_T_MAX = IN_W_T_MAX*TS

	OUT_IMG_H = IN_IMG_H*TS
	OUT_IMG_W = IN_IMG_W*TS

	IN_IMG_H_T = IN_H_T_MAX + K - 1
	IN_IMG_W_T = IN_W_T_MAX + K - 1
	OUT_IMG_H_T = OUT_H_T_MAX
	OUT_IMG_W_T = OUT_W_T_MAX
	
	LAYER_BATCH = 1
	STRIDE = design_params['STRIDE']
	SA_ROWS = design_params['SA_ROWS']
	SA_COLS = design_params['SA_COLS']
	SIMD_FACTOR = design_params['SA_SIMD']

	DATA_WIDTH = design_params['DATA_WIDTH']
	DATA_TYPE = design_params['DATA_TYPE']


	ROW_IL_FACTOR = int(OUT_NUM_T_MAX/SA_ROWS)
	COL_IL_FACTOR = int(OUT_IMG_W_T/SA_COLS)

	prj_path = os.environ['STREAM_VSA_PATH']

	jsonfile = []
	jsonfile.append('{\n')
	jsonfile.append('  "APP_NAME": "cnn",\n')
	jsonfile.append('  "PARAMETERS": {\n')
	jsonfile.append('    "IN_NUM": %d,\n '% IN_NUM)
	jsonfile.append('    "OUT_NUM": %d,\n '% OUT_NUM)
	jsonfile.append('    "IN_IMG_H": %d,\n '% IN_IMG_H)
	jsonfile.append('    "IN_IMG_W": %d,\n '% IN_IMG_W)
	jsonfile.append('    "OUT_IMG_H": %d,\n '% OUT_IMG_H)
	jsonfile.append('    "OUT_IMG_W": %d,\n '% OUT_IMG_W)
	jsonfile.append('    "K": %d,\n '% K)
	jsonfile.append('    "IN_NUM_T": %d,\n '% IN_NUM_T_MAX)
	jsonfile.append('    "OUT_NUM_T": %d,\n '% OUT_NUM_T_MAX)
	jsonfile.append('    "IN_IMG_H_T": %d,\n '% IN_IMG_H_T)
	jsonfile.append('    "IN_IMG_W_T": %d,\n '% IN_IMG_W_T)
	jsonfile.append('    "OUT_IMG_H_T": %d,\n '% OUT_IMG_H_T)
	jsonfile.append('    "OUT_IMG_W_T": %d,\n '% OUT_IMG_W_T)
	jsonfile.append('    "LAYER_BATCH": %d,\n '% LAYER_BATCH)
	jsonfile.append('    "STRIDE": %d\n '% STRIDE)
	jsonfile.append('  },   \n')
	jsonfile.append('  "ITERATORS": [\n')
	jsonfile.append('    {\n')
	jsonfile.append('      "VARIABLE": "out_num",\n')
	jsonfile.append('      "BOUND": [\n')
	jsonfile.append('        0,\n')
	jsonfile.append('        %d\n'% OUT_NUM)
	jsonfile.append('      ],\n')
	jsonfile.append('      "TILE": {\n')
	jsonfile.append('        "ENABLE": 1,\n')
	jsonfile.append('        "TILE_FACTOR": %d\n'% OUT_NUM_T_MAX)
	jsonfile.append('      }\n')
	jsonfile.append('    },\n')
	jsonfile.append('    {\n')
	jsonfile.append('      "VARIABLE": "out_img_h",\n')
	jsonfile.append('      "BOUND": [\n')
	jsonfile.append('        0,\n')
	jsonfile.append('        %d\n'% OUT_IMG_H)
	jsonfile.append('      ],\n')
	jsonfile.append('      "TILE": {\n')
	jsonfile.append('        "ENABLE": 1,\n')
	jsonfile.append('        "TILE_FACTOR": %d\n'% OUT_IMG_H_T)
	jsonfile.append('      }\n')
	jsonfile.append('    },\n')
	jsonfile.append('    {\n')
	jsonfile.append('      "VARIABLE": "out_img_w",\n')
	jsonfile.append('      "BOUND": [\n')
	jsonfile.append('        0,\n')
	jsonfile.append('        %d\n'% OUT_IMG_W)
	jsonfile.append('      ],\n')
	jsonfile.append('      "TILE": {\n')
	jsonfile.append('        "ENABLE": 1,\n')
	jsonfile.append('        "TILE_FACTOR": %d\n'% OUT_IMG_W_T)
	jsonfile.append('      }\n')
	jsonfile.append('    },\n')
	jsonfile.append('    {\n')
	jsonfile.append('      "VARIABLE": "in_num",\n')
	jsonfile.append('      "BOUND": [\n')
	jsonfile.append('        0,\n')
	jsonfile.append('        %d\n'% IN_NUM)
	jsonfile.append('      ],\n')
	jsonfile.append('      "TILE": {\n')
	jsonfile.append('        "ENABLE": 1,\n')
	jsonfile.append('        "TILE_FACTOR": %d\n'% IN_NUM_T_MAX)
	jsonfile.append('      }\n')
	jsonfile.append('    },\n')
	jsonfile.append('    {\n')
	jsonfile.append('      "VARIABLE": "p",\n')
	jsonfile.append('      "BOUND": [\n')
	jsonfile.append('        0,\n')
	jsonfile.append('        %d\n'% K)
	jsonfile.append('      ],\n')
	jsonfile.append('      "TILE": {\n')
	jsonfile.append('        "ENABLE": 0,\n')
	jsonfile.append('        "TILE_FACTOR": %d\n'% K)
	jsonfile.append('      }\n')
	jsonfile.append('    },\n')
	jsonfile.append('    {\n')
	jsonfile.append('      "VARIABLE": "q",\n')
	jsonfile.append('      "BOUND": [\n')
	jsonfile.append('        0,\n')
	jsonfile.append('        %d\n'% K)
	jsonfile.append('      ],\n')
	jsonfile.append('      "TILE": {\n')
	jsonfile.append('        "ENABLE": 0,\n')
	jsonfile.append('        "TILE_FACTOR": %d\n'% K)
	jsonfile.append('      }\n')
	jsonfile.append('    }\n')
	jsonfile.append('  ],  \n')
	jsonfile.append('  "TYPE": "local",\n')
	jsonfile.append('  "SA_ROWS": %d,\n'% SA_ROWS)
	jsonfile.append('  "SA_COLS": %d,\n'% SA_COLS)
	jsonfile.append('  "OP_CHANNEL_DIR": [\n')
	jsonfile.append('    "D", \n')
	jsonfile.append('    "R"\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "RES_CHANNEL_DIR": [\n')
	jsonfile.append('    "D"\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "DATA_TYPE": [\n')
	jsonfile.append('    "%s",\n'% DATA_TYPE)
	jsonfile.append('    "%s",\n'% DATA_TYPE)
	jsonfile.append('    "%s"\n'% DATA_TYPE)
	jsonfile.append('  ],\n')
	jsonfile.append('  "BUS_WIDTH": [\n')
	jsonfile.append('    512, \n')
	jsonfile.append('    512, \n')
	jsonfile.append('    512\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "DATA_WIDTH": [\n')
	jsonfile.append('    %d, \n'% DATA_WIDTH)
	jsonfile.append('    %d, \n'% DATA_WIDTH)
	jsonfile.append('    %d \n'% DATA_WIDTH)
	jsonfile.append('  ],\n')
	jsonfile.append('  "SIMD_FACTOR": %d,\n'% SIMD_FACTOR)
	jsonfile.append('  "FC_SIMD_FACTOR": [\n')
	jsonfile.append('    %d, \n'% SIMD_FACTOR)
	jsonfile.append('    %d, \n'% SIMD_FACTOR)
	jsonfile.append('    %d \n'% SIMD_FACTOR)
	jsonfile.append('  ],\n')
	jsonfile.append('  "FC_GROUP_FACTOR": [\n')
	jsonfile.append('    1, \n')
	jsonfile.append('    1, \n')
	jsonfile.append('    1\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "FC_SPLIT_FACTOR": [\n')
	jsonfile.append('    1, \n')
	jsonfile.append('    1, \n')
	jsonfile.append('    1\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "IL_ENABLE": 1,\n')
	jsonfile.append('  "ROW_IL_FACTOR": %d,\n'% ROW_IL_FACTOR)
	jsonfile.append('  "COL_IL_FACTOR": %d,\n'% COL_IL_FACTOR)
	jsonfile.append('  "FIXED_EN": 0,\n')
	jsonfile.append('  "KERNEL_ID": 1,\n')
	jsonfile.append('  "OP_REF": [\n')
	jsonfile.append('    "cin[IN_IMG_H][IN_IMG_W][IN_NUM]",\n')
	jsonfile.append('    "weight[OUT_NUM][K][K][IN_NUM]"\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "RES_REF": [\n')
	jsonfile.append('    "cout[OUT_IMG_H][OUT_IMG_W][OUT_NUM]"\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "OP_NAME": [\n')
	jsonfile.append('    "cin",\n')
	jsonfile.append('    "weight"\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "RES_NAME": [\n')
	jsonfile.append('    "cout"\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "OP_DIM": [\n')
	jsonfile.append('    3,\n')
	jsonfile.append('    4\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "RES_DIM": [\n')
	jsonfile.append('    3\n')
	jsonfile.append('  ],\n')
	jsonfile.append('  "INIT_VALUE": 0,\n')
	jsonfile.append('  "MAC_STAT": "sum += op0_u[i] * op1_u[i];\\n"\n')
	jsonfile.append('}\n')

	#print jsonfile to out.json
	with open(prj_path + '/auto_compile/design_generation/TAPA_1/systolic_array_kernel/cnn_features.json', 'w') as outfile:
		for line in jsonfile:
			outfile.write(line)

	OUT_NUM_T_MAX = math.ceil(OUT_NUM_T_MAX/(512/DATA_WIDTH))*(512/DATA_WIDTH)
	paramfile = []
	paramfile.append('#include "ap_fixed.h"\n')
	paramfile.append('#include <tapa.h>\n')
	paramfile.append('#define MAX_IN_NUM_T %d\n'% IN_NUM_T_MAX)
	paramfile.append('#define MAX_IN_H_T %d\n'% IN_H_T_MAX)
	paramfile.append('#define MAX_IN_W_T %d\n'% IN_W_T_MAX)
	paramfile.append('#define MAX_OUT_NUM_T %d\n'% OUT_NUM_T_MAX)
	paramfile.append('#define MAX_OUT_H_T %d\n'% OUT_H_T_MAX)
	paramfile.append('#define MAX_OUT_W_T %d\n'% OUT_W_T_MAX)
	paramfile.append('\n')
	# paramfile.append('#define IN_NUM_T_MAX %d\n'% IN_NUM_T_MAX)
	# paramfile.append('#define OUT_NUM_T_MAX %d\n'% OUT_NUM_T_MAX)
	# paramfile.append('#define K_T %d\n'% K)
	# paramfile.append('#define K_T_S %d\n'% (3*DR-DR+1))
	# max_buff_dict['CIN_BUFF'] = max_cin_buff
	# max_buff_dict['CIN_PREV_BUFF'] = max_cin_prev_buff
	# max_buff_dict['WEIGHT_BUFF'] = max_weight_buff
	# max_buff_dict['COUT_BUFF'] = max_cout_buff
	# max_buff_dict['DATA0_BUF_SIZE'] = max_data0_buf_size
	# max_buff_dict['DATA1_BUF_SIZE'] = max_data1_buf_size
	# max_buff_dict['DATA2_BUF_SIZE'] = max_data2_buf_size
	# max_buff_dict['ROW_IL_FACTOR'] = max_row_il_factor
	# max_buff_dict['COL_IL_FACTOR'] = max_col_il_factor
	# max_buff_dict['LOCAL_REG_NUM'] = max_local_reg_num
	# max_buff_dict['TRANSFER_REG_NUM'] = max_transfer_reg_num

	paramfile.append('#define CIN_BUFF %d\n'% buff_dict['CIN_BUFF'])
	paramfile.append('#define CIN_PREV_BUFF %d\n'% buff_dict['CIN_PREV_BUFF'])
	paramfile.append('#define WEIGHT_BUFF %d\n'% buff_dict['WEIGHT_BUFF'])
	paramfile.append('#define COUT_BUFF %d\n'% buff_dict['COUT_BUFF'])
	paramfile.append('#define U1_DATA0_BUF_SIZE %d\n'% buff_dict['DATA0_BUF_SIZE'])
	paramfile.append('#define U1_DATA1_BUF_SIZE %d\n'% buff_dict['DATA1_BUF_SIZE'])
	paramfile.append('#define U1_DATA2_BUF_SIZE %d\n'% buff_dict['DATA2_BUF_SIZE'])
	# paramfile.append('#define ROW_IL_FACTOR %d\n'% buff_dict['ROW_IL_FACTOR'])
	# paramfile.append('#define COL_IL_FACTOR %d\n'% buff_dict['COL_IL_FACTOR'])
	paramfile.append('#define U1_LOCAL_REG_NUM %d\n'% buff_dict['LOCAL_REG_NUM'])
	paramfile.append('#define U1_TRANSFER_REG_NUM %d\n'% buff_dict['TRANSFER_REG_NUM'])
	paramfile.append('#define SIMD_LANE %d\n'% SIMD_FACTOR)
	# paramfile.append('#define OUT_W_T2 24\n')
	paramfile.append('#define LAYER_NUM %d\n'% INSTS_NUM)
	paramfile.append('// Data Types\n')
	paramfile.append('// primtive data types\n')
	paramfile.append('typedef %s data_t0; // cin, cout\n'% DATA_TYPE)
	paramfile.append('typedef %s data_t1; // weight\n'% DATA_TYPE)
	paramfile.append('typedef %s data_t2; // bias\n'% DATA_TYPE)
	paramfile.append('typedef unsigned int data_t3; // inst\n')
	paramfile.append('using bus_t = tapa::vec_t<data_t0, %d>;\n'%int(512/DATA_WIDTH))
	paramfile.append('#define DATA_W0 %d\n'% DATA_WIDTH)
	paramfile.append('#define DATA_W1 %d\n'% DATA_WIDTH)
	paramfile.append('#define DATA_W2 %d\n'% DATA_WIDTH)
	paramfile.append('#define DATA_W3 %d\n'% DATA_WIDTH)
	# print('Data Type: %s'% DATA_TYPE)
	if DATA_TYPE=='float':
		paramfile.append('#define FLOAT_DESIGN\n')
	else:
		paramfile.append('#define FIXED_DESIGN\n')

	
	# paramfile.append('#define VGG_LAYERS 88\n')
	# paramfile.append('#define MobileNetV2_LAYERS 13\n')
	# paramfile.append('#define STAGE1_LAYERS 5\n')
	# paramfile.append('#define STAGE1_ITER 1\n')
	# paramfile.append('#define STAGE2_LAYERS 5\n')
	# paramfile.append('#define STAGE2_ITER 5\n')
	# paramfile.append('#define MAX_IN_W_T 96\n')
	# paramfile.append('#define MAX_IN_H_T 24\n')
	# paramfile.append('#define IN_OUT_OFFSET 1191968\n')
	# paramfile.append('#define LAYER1_IN_NUM 3\n')
	# paramfile.append('#define LAYER1_OUT_NUM 16\n')
	# paramfile.append('#define LAYER1_IN_NUM_T 8\n')
	# paramfile.append('#define LAYER1_OUT_NUM_T 16\n')
	# paramfile.append('#define LAYER1_IN_H 384\n')
	# paramfile.append('#define LAYER1_IN_W 384\n')
	# paramfile.append('#define LAYER1_OUT_H 192\n')
	# paramfile.append('#define LAYER1_OUT_W 192\n')
	# paramfile.append('#define LAYER1_IN_NUM_HW 8\n')
	# paramfile.append('#define LAYER1_OUT_NUM_HW 16\n')
	# paramfile.append('#define LAYER1_IN_H_HW 386\n')
	# paramfile.append('#define LAYER1_IN_W_HW 386\n')
	# paramfile.append('#define LAYER1_OUT_H_HW 194\n')
	# paramfile.append('#define LAYER1_OUT_W_HW 194\n')
	# paramfile.append('#define LAYER1_K 3\n')
	# paramfile.append('#define LAYER1_POOL 0\n')
	# paramfile.append('#define STAGE2L_OUT_NUM 38\n')
	# paramfile.append('#define STAGE2L_OUT_NUM_T 48\n')
	# paramfile.append('#define STAGE2L_OUT_H 48\n')
	# paramfile.append('#define STAGE2L_OUT_W 48\n')
	# paramfile.append('#define STAGE2L_OUT_NUM_HW 48\n')
	# paramfile.append('#define STAGE2L_OUT_H_HW 50\n')
	# paramfile.append('#define STAGE2L_OUT_W_HW 50\n')
	# paramfile.append('#define STAGE2L_K 3\n')
	# paramfile.append('#define STAGE2R_OUT_NUM 19\n')
	# paramfile.append('#define STAGE2R_OUT_NUM_T 48\n')
	# paramfile.append('#define STAGE2R_OUT_H 48\n')
	# paramfile.append('#define STAGE2R_OUT_W 48\n')
	# paramfile.append('#define STAGE2R_OUT_NUM_HW 48\n')
	# paramfile.append('#define STAGE2R_OUT_H_HW 50\n')
	# paramfile.append('#define STAGE2R_OUT_W_HW 50\n')
	# paramfile.append('#define STAGE2R_K 3\n')
	paramfile.append('#define CIN_SIZE 25000000\n')
	paramfile.append('#define WEIGHT_SIZE 20000000\n')
	paramfile.append('#define BIAS_SIZE 50000\n')
	#define CIN_SIZE 14193456
#define WEIGHT_SIZE 357760
#define BIAS_SIZE 16544
	# paramfile.append('#define STAGE2L_OFFSET 12160160\n')
	# paramfile.append('#define STAGE2R_OFFSET 12280160\n')
	paramfile.append('#define MAX_LAYER_BATCH 1\n')

	mem_type = int(args.mem_type)
	#print jsonfile to out.json
	with open(prj_path + '/auto_compile/design_generation/TAPA_1/design/src/params.h', 'w') as outfile:
		for line in paramfile:
			outfile.write(line)
		print('//on-chip memory assignments',file=outfile)
		# mem assignment
		if(mem_type==0):
			print("#define U1_DataFeed0Head_MEM %d"         % (0), file=outfile)
			print("#define U1_DataFeed1Head_MEM %d"         % (0), file=outfile)
			print("#define U1_DataCollect2Head_MEM %d"      % (0), file=outfile)
			print("#define U1_DataFeed0Engine0_MEM %d"      % (0), file=outfile)
			print("#define U1_DataFeed0EngineLast_MEM %d"   % (0), file=outfile)
			print("#define U1_DataFeed1Engine0_MEM %d"      % (0), file=outfile)
			print("#define U1_DataFeed1EngineLast_MEM %d"   % (0), file=outfile)
			print("#define U1_DataCollect2Engine0_MEM %d"   % (0), file=outfile)
			print("#define U1_DataCollect2EngineLast_MEM %d"% (0), file=outfile)
			print("#define cin_load_MEM %d"                 % (0), file=outfile)
			print("#define cin_load_prev_MEM %d"            % (0), file=outfile)
			print("#define weight_load_MEM %d"              % (0), file=outfile)
			print("#define bias_load_MEM %d"                % (0), file=outfile)
			print("#define cout_write_MEM %d"               % (0), file=outfile)
		elif(mem_type==1):
			print("#define U1_DataFeed0Head_MEM %d"         % (1), file=outfile)
			print("#define U1_DataFeed1Head_MEM %d"         % (1), file=outfile)
			print("#define U1_DataCollect2Head_MEM %d"      % (1), file=outfile)
			print("#define U1_DataFeed0Engine0_MEM %d"      % (1), file=outfile)
			print("#define U1_DataFeed0EngineLast_MEM %d"   % (1), file=outfile)
			print("#define U1_DataFeed1Engine0_MEM %d"      % (1), file=outfile)
			print("#define U1_DataFeed1EngineLast_MEM %d"   % (1), file=outfile)
			print("#define U1_DataCollect2Engine0_MEM %d"   % (1), file=outfile)
			print("#define U1_DataCollect2EngineLast_MEM %d"% (1), file=outfile)
			print("#define cin_load_MEM %d"                 % (1), file=outfile)
			print("#define cin_load_prev_MEM %d"            % (1), file=outfile)
			print("#define weight_load_MEM %d"              % (1), file=outfile)
			print("#define bias_load_MEM %d"                % (1), file=outfile)
			print("#define cout_write_MEM %d"               % (1), file=outfile)
		elif(mem_type==2):
			print("#define U1_DataFeed0Head_MEM %d"         % (1), file=outfile)
			print("#define U1_DataFeed1Head_MEM %d"         % (1), file=outfile)
			print("#define U1_DataCollect2Head_MEM %d"      % (1), file=outfile)
			print("#define U1_DataFeed0Engine0_MEM %d"      % (1), file=outfile)
			print("#define U1_DataFeed0EngineLast_MEM %d"   % (1), file=outfile)
			print("#define U1_DataFeed1Engine0_MEM %d"      % (1), file=outfile)
			print("#define U1_DataFeed1EngineLast_MEM %d"   % (1), file=outfile)
			print("#define U1_DataCollect2Engine0_MEM %d"   % (1), file=outfile)
			print("#define U1_DataCollect2EngineLast_MEM %d"% (1), file=outfile)
			print("#define cin_load_MEM %d"                 % (0), file=outfile)
			print("#define cin_load_prev_MEM %d"            % (0), file=outfile)
			print("#define weight_load_MEM %d"              % (0), file=outfile)
			print("#define bias_load_MEM %d"                % (0), file=outfile)
			print("#define cout_write_MEM %d"               % (0), file=outfile)
		elif(mem_type==3):
			print("#define U1_DataFeed0Head_MEM %d"         % (0), file=outfile)
			print("#define U1_DataFeed1Head_MEM %d"         % (0), file=outfile)
			print("#define U1_DataCollect2Head_MEM %d"      % (0), file=outfile)
			print("#define U1_DataFeed0Engine0_MEM %d"      % (0), file=outfile)
			print("#define U1_DataFeed0EngineLast_MEM %d"   % (0), file=outfile)
			print("#define U1_DataFeed1Engine0_MEM %d"      % (0), file=outfile)
			print("#define U1_DataFeed1EngineLast_MEM %d"   % (0), file=outfile)
			print("#define U1_DataCollect2Engine0_MEM %d"   % (0), file=outfile)
			print("#define U1_DataCollect2EngineLast_MEM %d"% (0), file=outfile)
			print("#define cin_load_MEM %d"                 % (1), file=outfile)
			print("#define cin_load_prev_MEM %d"            % (1), file=outfile)
			print("#define weight_load_MEM %d"              % (1), file=outfile)
			print("#define bias_load_MEM %d"                % (1), file=outfile)
			print("#define cout_write_MEM %d"               % (1), file=outfile)
		elif(mem_type==4):
			print("#define U1_DataFeed0Head_MEM %d"         % (1), file=outfile)
			print("#define U1_DataFeed1Head_MEM %d"         % (0), file=outfile)
			print("#define U1_DataCollect2Head_MEM %d"      % (0), file=outfile)
			print("#define U1_DataFeed0Engine0_MEM %d"      % (1), file=outfile)
			print("#define U1_DataFeed0EngineLast_MEM %d"   % (0), file=outfile)
			print("#define U1_DataFeed1Engine0_MEM %d"      % (0), file=outfile)
			print("#define U1_DataFeed1EngineLast_MEM %d"   % (0), file=outfile)
			print("#define U1_DataCollect2Engine0_MEM %d"   % (0), file=outfile)
			print("#define U1_DataCollect2EngineLast_MEM %d"% (0), file=outfile)
			print("#define cin_load_MEM %d"                 % (1), file=outfile)
			print("#define cin_load_prev_MEM %d"            % (1), file=outfile)
			print("#define weight_load_MEM %d"              % (1), file=outfile)
			print("#define bias_load_MEM %d"                % (1), file=outfile)
			print("#define cout_write_MEM %d"               % (1), file=outfile)
