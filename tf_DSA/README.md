# TensorFlow to FPGA

This folder serves as an application for testing the system and is built on https://github.com/ildoonet/tf-pose-estimation. Please refer to the original repo for more information.<br> The codes for integrating FlexCNN to TensorFlow are added here.

Original Repo : https://github.com/ildoonet/tf-pose-estimation

# Integrate Your Own Application to FPGA

1. First, you should install the libsacc library. Make sure you have followed the instructions [here](https://github.com/UCLA-VAST/FlexCNN/blob/master/libsacc/README.md#use-it-with-your-own-application).

2. Copy `./sacc_utils.py` to your project and modify the path to the libsacc library.

3. Import libsacc library in your code.
````Python
import sacc_utils 
````

4. Add the following to offload the CNN computation to FPGA.
````Python
self.constants = sacc_utils.Constants()
self.sacc_module = tf.load_op_library(self.constants.custom_lib_path)
result = self.sacc_module.sacc([self.tensor_image])
````

The last line takes a list of N images as input, sends them to FPGA, and returns a list of size N as the output. Refer to [this file](https://github.com/UCLA-VAST/FlexCNN/blob/master/tf_DSA/tf_pose/estimator_batch16.py) for an example on the usage.

5. To enable pipelining at the TensoFlow level, you can divide your tasks into several functions. 

Use Python’s Process to mimic the stages of the pipeline and use Python’s Queue to connect the stages to each other.

For example, you can define one stage of pipeline with an input and output FIFO as the following:
````Python
input_q = Queue()
output_q = Queue()
t = Process(target=task1, args=(input_q, output_q))
t.start()
````
Refer to [this file](https://github.com/UCLA-VAST/FlexCNN/blob/master/tf_DSA/run_stream_batch16.py) for a complete example.

