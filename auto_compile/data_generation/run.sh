echo generating weights...
python3 generate_weights.py \
  -ox=$PRJ_PATH/auto_compile/data/onnx/$1.onnx \
  -mi=$PRJ_PATH/auto_compile/data/post_dse_models/$1.csv \
  -a=$PRJ_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
  -o=$PRJ_PATH/designs/ENet_VSA/data/weights.dat

echo generating data...
python3 generate_data.py \
  -ox=$PRJ_PATH/auto_compile/data/onnx/$1.onnx \
  -mi=$PRJ_PATH/auto_compile/data/post_dse_models/$1.csv \
  -a=$PRJ_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
  -o=$PRJ_PATH/designs/ENet_VSA/data/

# echo comparing results...
# python3 compare_results.py \
#   -sw=$PRJ_PATH/designs/ENet_VSA/data/L1_output.dat \
#   -hw=$PRJ_PATH/designs/ENet_VSA/data/test/L1_output_HW.dat \
#   -mi=$PRJ_PATH/auto_compile/data/post_dse_models/$1.csv \
#   -a=$PRJ_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
#   -o=$PRJ_PATH/designs/ENet_VSA/data/