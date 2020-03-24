Library for Integrating FPGA to TensorFlow
===
This instruction describes how to build, install and integrate FlexCNN, developed by [UCLA VAST LAB](https://vast.cs.ucla.edu/), to TensorFlow. This implementation is adapted for the [**tensorflow custom operation interface**](https://www.tensorflow.org/guide/extend/op).

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
    python3 -m venv venv
	source venv/bin/activate
	sudo apt-get install build-essential libcap-dev
	pip3 install -r requirements.txt
    ````
5. Build the library via (execute in the root of the library: "libsacc" directory)
    ````bash
    mkdir build
    cmake ..
    make all
    ````
Now, the FlexCNN library has been successfully installed in your system.

# API and Usage
## API

This library follows the tensorflow custom operation interface. You can find details [here](https://www.tensorflow.org/guide/extend/op).

## Usage
Setting up the environment variable `XILINX_XRT` correspondingly (just source the `env.sh` in the root of this project) and then you can call this operation in python as below:

````python
import tensorflow as tf
sacc_module = tf.load_op_library('/path/to/this/lib/libsacc.so')
with tf.Session(''):
  sacc_module.sacc([[1, 2], [3, 4]]).eval()

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
This directory is first developed by Tong He. 
