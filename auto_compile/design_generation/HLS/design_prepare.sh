#!/bin/bash

cd systolic_array_kernel
# generate description file
python3 desp_gen.py -i cnn_features.json
# generate kernel files
python3 codegen.py -i ./output/design_desp.json -ar $1

cd ..
# copy kernel files to HLS project
cp systolic_array_kernel/output/2D* ./SA/
cp systolic_array_kernel/output/common* ./SA/
cp systolic_array_kernel/output/top.cpp ./SA/

# merge HLS kernel files into one SDx kernel file
cat ./SA/common_header_U1.h >> ./SA/hw_kernel0.cpp
cat ./SA/2DDataFeedCollect_U1.cpp >> ./SA/hw_kernel0.cpp
cat ./SA/2DDataFeed_U1.cpp >> ./SA/hw_kernel0.cpp
cat ./SA/2DDataCollect_U1.cpp >> ./SA/hw_kernel0.cpp
cat ./SA/2DPE_U1.cpp >> ./SA/hw_kernel0.cpp
# cat ./FlexCNN.cpp >> ./SA/hw_kernel0.cpp
cat ./SA/top.cpp >> ./SA/hw_kernel0.cpp

# modify hw_kernel.cpp
python hw_kernel_modify.py -i ./SA/hw_kernel0.cpp -o ./design/src/hw_kernel.cpp

# copy params.h to SDx project
# cp ../HLS_Codes/params.h ./

# # copy util.h to SDx project
# cp ../HLS_Codes/util.h ./src

# delete the extra kernel file
rm ./SA/hw_kernel0.cpp