source ./env.sh
cp tf_pose/estimator_batch16.py tf_pose/estimator.py
python run_stream_batch16.py --resize=384x384 --display --model=mobilenet_thin --video=$1 --device=FPGA

