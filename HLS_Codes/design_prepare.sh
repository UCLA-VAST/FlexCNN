#!/bin/bash

cd systolic_array_kernel
# generate description file
python desp_gen.py -i cnn_features.json
# generate kernel files
python codegen.py -i ./output/design_desp.json

cd ..
# copy kernel files to HLS project
cp systolic_array_kernel/output/2D* .
cp systolic_array_kernel/output/common* .

# copy params.h to HLS project
cp ../auto_compile/inst_gen/params.h .
