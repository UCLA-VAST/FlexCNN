import numpy as np

def BRAM_SDP_predict_HLS(dw, s):
	if dw > 18:
		alpha = np.ceil(dw / 36)
		BRAM = alpha * np.ceil(s / dw / 512)
	else:
		alpha = np.ceil(dw / 18)
		BRAM = alpha * np.ceil(s / dw / 1024)
	return BRAM

def res_est(params):
	SIMD_LANE = params['LANE']
	SA_ROWS = params['SA_ROWS']
	SA_COLS = params['SA_COLS']
	SA_SIMD_LANE = params['SA_SIMD']
	LAYER_IN_NUM_T = params['LAYER_IN_NUM_T']
	LAYER_OUT_NUM_T = params['LAYER_OUT_NUM_T']
	LAYER_IN_H_T = params['LAYER_IN_H_T']
	LAYER_IN_W_T = params['LAYER_IN_W_T']
	LAYER_OUT_H_T = params['LAYER_OUT_H_T']
	LAYER_OUT_W_T = params['LAYER_OUT_W_T']
	LAYER_K_T = params['K_T']

	# estimate DSPs
	if params['DATA_T0'] == "float":
		DSP_per_MAC = 5
	elif params['DATA_T0'] == "ap_fixed<16,8>":
		DSP_per_MAC = 1
	elif params['DATA_T0'] == "ap_fixed<8,4>":
		DSP_per_MAC = 0.25
	# depth_conv
	depth_conv_DSP = 0#(3 * 3 * SIMD_LANE + 1 * 1 * SIMD_LANE) * DSP_per_MAC
	# point_conv
	point_conv_DSP = SA_ROWS * SA_COLS * SA_SIMD_LANE * DSP_per_MAC
	DSP = depth_conv_DSP + point_conv_DSP

	# estimate BRAMs
	# cin_load
	cin_load_BRAM = BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W0'] * LAYER_IN_NUM_T * (LAYER_IN_H_T + LAYER_K_T - 1) * (LAYER_IN_W_T + LAYER_K_T - 1)) * 2
	# weight_load
	weight_load_BRAM = BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W1'] * 32 * 32 * LAYER_K_T * LAYER_K_T) #TODO: TEMP FIX
	# BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W1'] * LAYER_IN_NUM_T * LAYER_K_T * LAYER_K_T) + BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W1'] * LAYER_IN_NUM_T * LAYER_OUT_NUM_T * LAYER_K_T * LAYER_K_T) + BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W2'] * LAYER_OUT_NUM_T)
	# point_conv
	ROW_IL_FACTOR = LAYER_OUT_NUM_T / SA_ROWS
	COL_IL_FACTOR = LAYER_OUT_W_T / SA_COLS
	LOCAL_REG_NUM = LAYER_OUT_H_T * ROW_IL_FACTOR * COL_IL_FACTOR
	point_conv_BRAM = \
		BRAM_SDP_predict_HLS(params['DATA_W0'] * SIMD_LANE, LAYER_IN_NUM_T * (LAYER_IN_H_T + LAYER_K_T - 1) * (LAYER_IN_W_T + LAYER_K_T - 1) * params['DATA_W0']) + \
		BRAM_SDP_predict_HLS(params['DATA_W0'] * SIMD_LANE, LAYER_IN_NUM_T * (LAYER_IN_H_T + LAYER_K_T - 1) * (COL_IL_FACTOR + LAYER_K_T - 1) * params['DATA_W0']) * 2 * SA_COLS + \
		BRAM_SDP_predict_HLS(params['DATA_W1'] * SIMD_LANE, LAYER_IN_NUM_T * ROW_IL_FACTOR * LAYER_K_T * LAYER_K_T * params['DATA_W1']) * 2 * SA_ROWS + \
		BRAM_SDP_predict_HLS(params['DATA_W0'], LAYER_OUT_NUM_T * LAYER_OUT_H_T * COL_IL_FACTOR * params['DATA_W0'] / SIMD_LANE) * SIMD_LANE * 2 * SA_COLS + \
		BRAM_SDP_predict_HLS(params['DATA_W0'], LOCAL_REG_NUM * params['DATA_W0']) * 3 * SA_ROWS * SA_COLS
	# cout_write
	cout_write_BRAM = BRAM_SDP_predict_HLS(params['BUS_W'], params['DATA_W0'] * LAYER_OUT_H_T * LAYER_OUT_W_T * LAYER_OUT_NUM_T) * 2

	BRAM18K = cin_load_BRAM + weight_load_BRAM + point_conv_BRAM + cout_write_BRAM

	return DSP, BRAM18K
