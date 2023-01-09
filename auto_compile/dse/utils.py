import numpy as np
from latency_est import layer_latency_est
def print_layers_details(result, layer_configs, layers_order, hw_params, detailed_dse_report_file):
	total_latency = 0
	print('latency', file=detailed_dse_report_file)
	for i, layer_name in enumerate(layers_order):
		# print(i)
		
		layer = layer_configs[layer_name]
		params = result['opt_params']
		next_in_num_t = 0
		next_in_h_t = 0
		next_in_w_t = 0
		next_exists = False
		if len(layer['MAIN_OUTPUT'])>0:
			next_name = layer['MAIN_OUTPUT'][-1]
			next_layer_idx = layers_order.index(next_name)
			next_in_num_t = params['LAYER_IN_NUM_T_LIST'][next_layer_idx]
			next_in_h_t = params['LAYER_IN_H_T_LIST'][next_layer_idx]
			next_in_w_t = params['LAYER_IN_W_T_LIST'][next_layer_idx]
			next_exists = True
	
		instDict = {}
		in_num = layer['LAYER_IN_NUM']
		out_num = layer['LAYER_OUT_NUM']
		in_h = layer['LAYER_IN_H']
		in_w = layer['LAYER_IN_W']
		out_h = layer['LAYER_OUT_H']
		out_w = layer['LAYER_OUT_W']
		in_num_t = params['LAYER_IN_NUM_T_LIST'][i]
		out_num_t = params['LAYER_OUT_NUM_T_LIST'][i]
		in_h_t = params['LAYER_IN_H_T_LIST'][i]
		in_w_t = params['LAYER_IN_W_T_LIST'][i]
		out_h_t = params['LAYER_IN_H_T_LIST'][i]
		out_w_t = params['LAYER_IN_W_T_LIST'][i]
		kernel_size = 0
		for op in layer['ops']:
			op_params = op['params']
			if op_params['en']:
				if op['name'] == 'Conv' or op['name']=='ConvTranspose':
					kernel_size = (op_params['kernel_h'], op_params['kernel_w'])
					conv_type = op_params['type']
		next_layer_is_1x1_conv = True
		for next_layer_name in layer['MAIN_OUTPUT']:
			next_layer_config = layer_configs[next_layer_name]
			for op in next_layer_config['ops']:
				op_params = op['params']
				if op_params['en'] and op['name'] == 'Conv' and (op_params['kernel_h'] > 1 or op_params['kernel_w'] > 1):
					next_layer_is_1x1_conv = False
					break
		instDict.update(layer)
		instDict.update(params)
		instDict.update(hw_params)
		instDict['LAYER_IN_NUM_T'] = in_num_t
		instDict['LAYER_OUT_NUM_T'] = out_num_t
		instDict['LAYER_IN_H_T'] = in_h_t
		instDict['LAYER_IN_W_T'] = in_w_t
		# print(next_layer_is_1x1_conv)
		latency, eff = layer_latency_est(instDict)
		# print(latency/234e6)
		dsp_eff = params['LAYER_DSP_EFF_LIST'][i][0]
		bottleneck = params['LAYER_DSP_EFF_LIST'][i][1]
		total_latency += latency
		# print(eff[0], eff[1])
		# print()
		# print(eff[0])
		# print(dsp_eff)
		# print()
		# print('{} {} {} {} {} {}'.format(in_num, out_num, in_h, in_w, out_h, out_w))
		
		# print('{} {} {} {}'.format(in_num_t, out_num_t, in_h_t, in_w_t), latency/234e6)
		# print(layer['LAYER_ID'])
		 
		print('{:<5}, '.format(layer['LAYER_ID']+1), conv_type+' ,', '{:<20}, '.format(bottleneck), '{:.2%}, '.format(dsp_eff/100), '{:}'.format(latency/(instDict['FRE']*1e6)), file=detailed_dse_report_file)
		# print('{:}'.format(latency/(instDict['FRE']*1e6)), file=detailed_dse_report_file)
		# '{} {} {} {} {} {}'.format(in_num, out_num, in_h, in_w, out_h, out_w), kernel_size)
		# print('{} {} {} {}'.format(in_num_t, out_num_t, in_h_t, in_w_t))
		# print()
	total_latency = total_latency / (instDict['FRE']*1e6)
	print('latency in s: ', total_latency, file=detailed_dse_report_file)
	print('FPS: ', 1/total_latency)


#function to create a list of first element from a list of tuples
def get_first_element(list_of_tuples):
	return [x[0] for x in list_of_tuples]

#function to sort list of tuples by second element of the tuples in decreasing order
def sort_by_second_element(list_of_tuples):
	return sorted(list_of_tuples, key=lambda x: x[1])

#sort a litst of dicts by a key
def sort_by_key(list_of_dicts, key, reverse=False):
	return sorted(list_of_dicts, key=lambda x: x[key], reverse=reverse)

def list_split(ori_list, split_num):
	chunk_size = int(np.ceil(float(len(ori_list)) / split_num))
	chunks = [ori_list[i: i + min(chunk_size, len(ori_list) - i)] for i in range(0, len(ori_list), chunk_size)]
	return chunks

#sort a dict of dicts by a key and return a list
def sort_by_key_and_return_list(dict_of_dicts, key, reverse=False):
	return [x for x in sort_by_key(dict_of_dicts.values(), key, reverse)]


def print_results(results, connected_components, layer_configs, layers_order, hw_params, total_macs, board_info):
	top_result = results[0]
	layer_in_num_t_list = top_result['opt_params']['LAYER_IN_NUM_T_LIST']
	layer_out_num_t_list = top_result['opt_params']['LAYER_OUT_NUM_T_LIST']
	layer_in_h_t_list = top_result['opt_params']['LAYER_IN_H_T_LIST']
	layer_in_w_t_list = top_result['opt_params']['LAYER_IN_W_T_LIST']
	# print(layer_in_num_t_list)
	# print(layer_out_num_t_list)
	# print(layer_in_h_t_list)
	# print(layer_in_w_t_list)

	layer_dsp_eff_list = top_result['opt_params']['LAYER_DSP_EFF_LIST']

	opt_latency = results[0]['opt_latency']
	opt_DSP = results[0]['opt_DSP']
	opt_BRAM18K = results[0]['opt_BRAM18K']
	opt_params = results[0]['opt_params']
	solver_fails = results[0]['solver_fails']
	model_fails = results[0]['model_fails']

	print("*************************************************")
	print("model fails: ", model_fails)
	print("*************************************************")

	dependency_broken = False
	for component in connected_components:
		compList = list(component)
		layer_name = compList[0][1]
		layer_config = layer_configs[layer_name]
		layer_id = layer_config['LAYER_ID']
		comp_t = layer_in_num_t_list[layer_id] if compList[0][0] == 'in' else layer_out_num_t_list[layer_id]
		for tpl in compList:
			layer_name = tpl[1]
			layer_config = layer_configs[layer_name]
			layer_id = layer_config['LAYER_ID']
			layer_t = layer_in_num_t_list[layer_id] if tpl[0] == 'in' else layer_out_num_t_list[layer_id]
			if(layer_t != comp_t):
				dependency_broken = True
				# print('Error: component %s is not consistent with the optimized parameters' % (component))
				# break
	print("*************************************************")
	if(dependency_broken):
		print('Error: component %s is not consistent with the optimized parameters' % (connected_components))
	else:
		print('dependency maintainted')

	# print_layer_details()
	print("*************************************************")

	print_layers_details(top_result, layer_configs, layers_order, hw_params)
	opt_time = opt_latency / (opt_params['FRE'] * 1e6)
	opt_fps = 1 / opt_time
	# print("opt cycles: ", opt_latency)
	# print("opt latency solver (s) @%dMHz: " % (opt_params['FRE']), latencySolver)
	ideal_latency = total_macs/(opt_params['SA_ROWS']*opt_params['SA_COLS']*opt_params['SA_SIMD']*opt_params['FRE']*1e6)
	print("ideal latency      (s) @%dMHz: " % (opt_params['FRE']), ideal_latency)
	print("opt latency        (s) @%dMHz: " % (opt_params['FRE']), opt_time)
	print("DSP efficiency: %0.2f" % ((ideal_latency / opt_time)*100), "%")
	print("opt FPS: ", opt_fps)
	opt_BRAM18K_util = opt_BRAM18K / board_info['BRAM18K'] * 100
	opt_DSP_util = opt_DSP / board_info['DSP'] * 100
	print("opt BRAM18K: %d (%d%%)" % (opt_BRAM18K, opt_BRAM18K_util))
	print("opt DSP: %d (%d%%)" % (opt_DSP, opt_DSP_util))
	print("SA (COLS, ROWS, SIMD): (%d, %d, %d)" % (opt_params['SA_ROWS'],opt_params['SA_COLS'],opt_params['SA_SIMD']))
	print("LANES: ", opt_params['LANE'])
	print("Max tiling factors: (%d, %d, %d, %d)" % (opt_params['LAYER_IN_NUM_T'], opt_params['LAYER_OUT_NUM_T'], opt_params['LAYER_IN_H_T'], opt_params['LAYER_IN_W_T']))
	return opt_time


def print_result(result, connected_components, layer_configs, layers_order, hw_params, total_macs, board_info, dse_report_file, detailed_dse_report_file):
	top_result = result
	layer_in_num_t_list = top_result['opt_params']['LAYER_IN_NUM_T_LIST']
	layer_out_num_t_list = top_result['opt_params']['LAYER_OUT_NUM_T_LIST']
	layer_in_h_t_list = top_result['opt_params']['LAYER_IN_H_T_LIST']
	layer_in_w_t_list = top_result['opt_params']['LAYER_IN_W_T_LIST']



	layer_dsp_eff_list = top_result['opt_params']['LAYER_DSP_EFF_LIST']

	opt_latency = top_result['opt_latency']
	opt_DSP = top_result['opt_DSP']
	opt_BRAM18K = top_result['opt_BRAM18K']
	opt_params = top_result['opt_params']
	solver_fails = top_result['solver_fails']
	model_fails = top_result['model_fails']

	print("*************************************************", file=dse_report_file)
	print("model fails: ", model_fails, file=dse_report_file)
	print("*************************************************", file=dse_report_file)

	dependency_broken = False
	for component in connected_components:
		compList = list(component)
		layer_name = compList[0][1]
		layer_config = layer_configs[layer_name]
		layer_id = layer_config['LAYER_ID']
		comp_t = layer_in_num_t_list[layer_id] if compList[0][0] == 'in' else layer_out_num_t_list[layer_id]
		for tpl in compList:
			layer_name = tpl[1]
			layer_config = layer_configs[layer_name]
			layer_id = layer_config['LAYER_ID']
			layer_t = layer_in_num_t_list[layer_id] if tpl[0] == 'in' else layer_out_num_t_list[layer_id]
			if(layer_t != comp_t):
				dependency_broken = True
				# print('Error: component %s is not consistent with the optimized parameters' % (component))
				# break
	print("*************************************************", file=dse_report_file)
	if(dependency_broken):
		print('Error: component %s is not consistent with the optimized parameters' % (connected_components), file=dse_report_file)
	else:
		print('dependency maintainted', file=dse_report_file)

	# print_layer_details()
	print("*************************************************", file=dse_report_file)

	print_layers_details(top_result, layer_configs, layers_order, hw_params, detailed_dse_report_file)
	opt_time = opt_latency / (opt_params['FRE'] * 1e6)
	opt_fps = 1 / opt_time
	# print("opt cycles: ", opt_latency)
	# print("opt latency solver (s) @%dMHz: " % (opt_params['FRE']), latencySolver)
	ideal_latency = total_macs/(opt_params['SA_ROWS']*opt_params['SA_COLS']*opt_params['SA_SIMD']*opt_params['FRE']*1e6)
	print("ideal latency      (s) @%dMHz: " % (opt_params['FRE']), ideal_latency, file=dse_report_file)
	print("opt latency        (s) @%dMHz: " % (opt_params['FRE']), opt_time, file=dse_report_file)
	print("DSP efficiency: %0.2f" % ((ideal_latency / opt_time)*100), "%", file=dse_report_file)
	print("opt FPS: ", opt_fps, file=dse_report_file)
	opt_BRAM18K_util = opt_BRAM18K / board_info['BRAM18K'] * 100
	opt_DSP_util = opt_DSP / board_info['DSP'] * 100
	print("opt BRAM18K: %d (%d%%)" % (opt_BRAM18K, opt_BRAM18K_util), file=dse_report_file)
	print("opt DSP: %d (%d%%)" % (opt_DSP, opt_DSP_util), file=dse_report_file)
	print("SA (COLS, ROWS, SIMD): (%d, %d, %d)" % (opt_params['SA_ROWS'],opt_params['SA_COLS'],opt_params['SA_SIMD']), file=dse_report_file)
	print("LANES: ", opt_params['LANE'], file=dse_report_file)
	print("Max tiling factors: (%d, %d, %d, %d)" % (opt_params['LAYER_IN_NUM_T'], opt_params['LAYER_OUT_NUM_T'], opt_params['LAYER_IN_H_T'], opt_params['LAYER_IN_W_T']), file=dse_report_file)
	return opt_time
