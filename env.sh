# sudo apt install opencl-clhpp-headers for CL headers
export PRJ_PATH=$(pwd)/designs
export STREAM_VSA_PATH=$(pwd)
# if designs dir is not present, create it
if [ ! -d "$PRJ_PATH" ]; then
    mkdir $PRJ_PATH
fi
if [ ! -d "$STREAM_VSA_PATH/auto_compile/data/onnx" ]; then
    mkdir $STREAM_VSA_PATH/auto_compile/data/onnx
fi
if [ ! -d "$STREAM_VSA_PATH/auto_compile/data/post_dse_architectures" ]; then
    mkdir $STREAM_VSA_PATH/auto_compile/data/post_dse_architectures
fi
if [ ! -d "$STREAM_VSA_PATH/auto_compile/data/post_dse_models" ]; then
    mkdir $STREAM_VSA_PATH/auto_compile/data/post_dse_models
fi
if [ ! -d "$STREAM_VSA_PATH/auto_compile/data/pre_dse_models" ]; then
    mkdir $STREAM_VSA_PATH/auto_compile/data/pre_dse_models
fi
if [ ! -d "$STREAM_VSA_PATH/auto_compile/data/dse_reports" ]; then
    mkdir $STREAM_VSA_PATH/auto_compile/data/dse_reports
fi
if [ ! -d "$STREAM_VSA_PATH/auto_compile/data/insts" ]; then
    mkdir $STREAM_VSA_PATH/auto_compile/data/insts
fi
echo 'done'