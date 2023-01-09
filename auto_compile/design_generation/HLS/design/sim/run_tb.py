import sys
import os
from pathlib import Path

# from instGen import generateInsts
# from conv import generateData
from set_headers import setHeaders

start_layer = int(sys.argv[1])
end_layer = int(sys.argv[2])
layer_id_test = int(sys.argv[3])
sl_option = int(sys.argv[4]) # 0: both disabled, 1: load_only, 2: store_only, 3: both enabled 
mode = sys.argv[5]
if len(sys.argv) > 6:
  target_module = sys.argv[6]
else:
  target_module = 'NONE'
prj_path = Path(os.getenv('PRJ_PATH'))
# print(prj_path)

# step 2: get the inputs, outputs, and weights
# generateData(insts, inDims, prj_path, convType)

# step 3: update header files
setHeaders(prj_path, start_layer, end_layer, layer_id_test, target_module, sl_option)

#step 4: run the testbench
# os.system("./hls.sh sim " + str(prj_path))

if mode=='csim':
  os.system("vitis_hls hls.tcl csim " + str(prj_path.as_posix()))
elif mode=='cosim':
  os.system("make cosim_all")
# C:/Users/Suhail/Desktop/Projects/FlexCNN_VSA/designs/ENet_VSA/sim