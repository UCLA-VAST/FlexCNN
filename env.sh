# sudo apt install opencl-clhpp-headers for CL headers
export PRJ_PATH=$(pwd)/designs
export STREAM_VSA_PATH=$(pwd)
conda activate your_env
# if designs dir is not present, create it
if [ ! -d "$PRJ_PATH" ]; then
    mkdir $PRJ_PATH
fi
echo 'done'