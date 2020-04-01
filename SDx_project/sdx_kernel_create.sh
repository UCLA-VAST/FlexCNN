#!/bin/bash

rm ./src/hw_kernel*

# merge HLS kernel files into one SDx kernel file
cat ../HLS_Codes/common_header_U1.h >> ./src/hw_kernel0.cpp
cat ../HLS_Codes/2DDataFeedCollect_U1.cpp >> ./src/hw_kernel0.cpp
cat ../HLS_Codes/2DDataFeed_U1.cpp >> ./src/hw_kernel0.cpp
cat ../HLS_Codes/2DDataCollect_U1.cpp >> ./src/hw_kernel0.cpp
cat ../HLS_Codes/2DPE_U1.cpp >> ./src/hw_kernel0.cpp
cat ../HLS_Codes/kernel.cpp >> ./src/hw_kernel0.cpp

# modify hw_kernel.cpp
python hw_kernel_modify.py -i src/hw_kernel0.cpp -o src/hw_kernel.cpp

# copy params.h to SDx project
cp ../HLS_Codes/params.h ./src/

# copy pose.h to SDx project
cp ../HLS_Codes/pose.h ./src

# delete the extra kernel file
rm ./src/hw_kernel0.cpp
