dw=($3)

echo "Translating Graph..."
python3 $STREAM_VSA_PATH/auto_compile/graph_translation/extract_info.py \
  -g=$STREAM_VSA_PATH/auto_compile/data/onnx/$1.onnx \
  -a=$STREAM_VSA_PATH/auto_compile/data/pre_dse_architectures/$1_arch.json \
  -m=$STREAM_VSA_PATH/auto_compile/data/pre_dse_models/$1

echo "Design Space Exploration..."
python3 $STREAM_VSA_PATH/auto_compile/dse/dse.py \
  -ai=$STREAM_VSA_PATH/auto_compile/data/pre_dse_architectures/$1_arch.json \
  -ao=$STREAM_VSA_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
  -mi=$STREAM_VSA_PATH/auto_compile/data/pre_dse_models/$1.csv \
  -mo=$STREAM_VSA_PATH/auto_compile/data/post_dse_models/$1.csv \
  -b=$STREAM_VSA_PATH/auto_compile/data/boards/$4.json \
  -o=$STREAM_VSA_PATH/auto_compile/data/dse_reports/out_$1$dw $7 $8 \
  -dw=$dw 

SA_DIM=$(tail -n 1 $STREAM_VSA_PATH/auto_compile/data/dse_reports/out_$1$dw.txt)

design_name=$6_$2_$SA_DIM"_MEM_"$5_$dw

tested_cnns=(VGG16 UNet ENet)
# if $1 is not in tested_cnns, then it is a custom CNN
if [[ ! " ${tested_cnns[@]} " =~ " ${1} " ]]; then
  echo "Instruction Generation..."
  python3 $STREAM_VSA_PATH/auto_compile/inst_generation/inst_gen.py \
  -mi=$STREAM_VSA_PATH/auto_compile/data/post_dse_models/$1.csv \
  -a=$STREAM_VSA_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
  -io=$STREAM_VSA_PATH/auto_compile/data/insts/$design_name"_instructions.dat"
else
  echo "Instruction Generation..."
  python3 $STREAM_VSA_PATH/auto_compile/inst_generation/$1_inst_gen.py \
  -mi=$STREAM_VSA_PATH/auto_compile/data/post_dse_models/$1.csv \
  -a=$STREAM_VSA_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
  -io=$STREAM_VSA_PATH/auto_compile/data/insts/$design_name"_instructions.dat"
fi

echo "Generating HLS HW design..."
python3 $STREAM_VSA_PATH/auto_compile/design_generation/generate.py \
-m=$design_name \
-dp=$STREAM_VSA_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
-ar=$STREAM_VSA_PATH/auto_compile/data/arch_connectivity/$1.json \
-o=$PRJ_PATH \
-mt=$5 \
-c=$6

cp $STREAM_VSA_PATH/auto_compile/data/dse_reports/out_$1$dw.txt $PRJ_PATH/$design_name/data/dse_report.txt
cp $STREAM_VSA_PATH/auto_compile/data/dse_reports/out_$1$dw"_detailed.csv" $PRJ_PATH/$design_name/data/detailed_dse_report.csv
cp $STREAM_VSA_PATH/auto_compile/data/insts/$design_name"_instructions.dat" $PRJ_PATH/$design_name/data/instructions.dat

echo "Generating weights..."
python3 $STREAM_VSA_PATH/auto_compile/data_generation/generate_weights.py \
  -ox=$STREAM_VSA_PATH/auto_compile/data/onnx/$1.onnx \
  -mi=$STREAM_VSA_PATH/auto_compile/data/post_dse_models/$1.csv \
  -a=$STREAM_VSA_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
  -o=$PRJ_PATH/$design_name/data/weights.dat

echo "Generating biases..."
python3 $STREAM_VSA_PATH/auto_compile/data_generation/generate_biases.py \
  -ox=$STREAM_VSA_PATH/auto_compile/data/onnx/$1.onnx \
  -mi=$STREAM_VSA_PATH/auto_compile/data/post_dse_models/$1.csv \
  -a=$STREAM_VSA_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
  -o=$PRJ_PATH/$design_name/data/biases.dat

if [ $dw -gt 16 ]
then
  echo "Generating data..."
  python3 $STREAM_VSA_PATH/auto_compile/data_generation/generate_data.py \
    -ox=$STREAM_VSA_PATH/auto_compile/data/onnx/$1.onnx \
    -mi=$STREAM_VSA_PATH/auto_compile/data/post_dse_models/$1.csv \
    -a=$STREAM_VSA_PATH/auto_compile/data/post_dse_architectures/$1_arch.json \
    -o=$PRJ_PATH/$design_name/data/
fi

echo "Done!"