Library for Integrating FPGA to TensorFlow

This instruction describes how to build, install and integrate FlexCNN, developed by [UCLA VAST LAB](https://vast.cs.ucla.edu/), to TensorFlow. This implementation is adapted from the [**tensorflow custom operation interface**](https://www.tensorflow.org/guide/extend/op).

# Content
1. [Hardware and Operating System](#Hardware-and-Operating-System)
2. [Requirements and Dependencies](#Requirements-and-Dependencies)
3. [Build The Library From The Source](#Build-The-Library-From-The-Source)
4. [API and Usage](#API-And-Usage)
5. [Project File Tree](#Project-File-Tree)
6. [Acknowledgement](#Acknowledgement)

# HardWare and Operating System
## Development
For development, the OS should be **Ubuntu 18.04 LTS**
## Testing and Deployment 
For testing and depolyment, despite the os requirement above, the server/PC should also equips with [Xilinx Virtex UltraScale+ FPGA VCU1525 Development Kit](https://www.xilinx.com/products/boards-and-kits/vcu1525-a.html)



# Requirements and Dependencies

## Requirements
The [Xilinx Runtime v2018.3](https://www.xilinx.com/products/boards-and-kits/vcu1525-a.html#gettingStarted) should be installed.

If also want to compile the library from source, the **Xilinx Deployment Shell v2018.3**, **Xilinx Development Shell** and **SDAccel Design Environment v2018.3** should also be installed. You can find them through [this link](https://www.xilinx.com/products/boards-and-kits/vcu1525-a.html#gettingStarted).

## Dependencies
This library uses the [**cmake**](https://cmake.org/) (version >= 3.0) as the building system and the [**googletest**](https://github.com/google/googletest) as the test framework. It also depends on the [**tensorflow**](https://www.tensorflow.org/).


# Build the Library From The Source

1. Pull the source code from this repo
    ````bash
    git clone [URL of this repo]
    ````
2. In ./src/libsacc.cpp change the following line (give your project path)
    ````python
	std::string Sacc::lib_base_path = "/path/to/libsacc/";
	````
3. Setting environments
    ````bash
    cd libsacc
    source env.sh
    ````
4. Installing packages
	````bash
    python3.6 -m venv venv
	source venv/bin/activate
	sudo apt-get install build-essential libcap-dev
	sudo apt install opencl-headers
	pip3 install cython
	pip3 install numpy
	pip3 install -r requirements.txt
    ````
5. Build the library via (execute in the root of the library: "libsacc" directory)
    ````bash
    mkdir build
	cd build
    cmake ..
    make all
    ````
Now, the FlexCNN library is successfully installed in your system.

# Use it with Your Own Application

1. Make sure you have installed the library successfully (refer to previous section).

2. You first should build the bitstream. To do that, follow the instructions [here](https://github.com/UCLA-VAST/FlexCNN#build-your-own-hardware).

3. Now, that you have the required files, copy the bitstream (.xclbin file), host executable (.exe file) and instructions (.insts file) to `./config`. 

4. Replace your TensorFlow project with `../tf_DSA`. For that, follow the instructions [here](https://github.com/UCLA-VAST/FlexCNN/blob/master/tf_DSA/README.md#integrate-your-own-application-to-fpga)


# API and Usage
## API

This library follows the tensorflow custom operation interface. You can find details [here](https://www.tensorflow.org/guide/extend/op).

## Usage
Setting up the environment variable `XILINX_XRT` correspondingly (just source the `env.sh` in the root of this project) and then you can call this operation in python as below:

````python
import tensorflow as tf

class SaccTest(tf.test.TestCase):
  def SaccTest(self):
    sacc_module = tf.load_op_library('/path/to/this/lib/libsacc.so')
    with tf.Session(''):
      result = sacc_module.sacc([[1, 2], [3, 4]])
      result.eval()

if __name__ == "__main__":
  tf.test.main()
````


# Project File Tree
The project file structure is shown below,
````
.
+-- CMakeList.txt
+-- src
|   +-- libsacc.cpp
|   +-- xcl2.cpp # xilinx opencl utils
+-- inc
|   +-- CMakeLists.txt
|   +-- sa_params.h
|   +-- sa_types.h
|   +-- sacc.hpp
|   +-- xcl2.hpp # xilinx runtime support
+-- test
|   +-- CMakeLists.txt
|   +-- test.cpp
+-- utils
|   +-- gen_params.sh
+-- docker
|   +-- Dockerfile 
|   +-- run.sh
+-- data
+-- config
````
The `src` and `inc` contain source files and header files.

The `data` folder contains bias and weight information used by the accelerator and the `config` contains the instructions and the FPGA binary. 

# Acknowledgement
The first version of this library was developed by Tong He. 
