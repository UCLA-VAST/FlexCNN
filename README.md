# FlexCNN

## Publication

+ Atefeh Sohrabizadeh, Jie Wang, Jason Cong. [End-to-End Optimization of Deep Learning Applications](https://dl.acm.org/doi/abs/10.1145/3373087.3375321). In FPGA, 2020.

## About
This repo contains the codes for building FlexCNN, an accelerator for running CNNs on FPGA, described in [here](https://dl.acm.org/doi/abs/10.1145/3373087.3375321). As mentioned in the paper, you can further integrate FlexCNN to TensorFlow and offload CNN computation of your application to FPGA.

In this repo, we use [OpenPose](https://arxiv.org/abs/1611.08050) to demonstrate our flow.

## Content
1. [Hardware and Operating System](#Hardware-and-Operating-System)
2. [Requirements and Dependencies](#Requirements-and-Dependencies)
3. [Project File Tree](#Project-File-Tree)
4. [Run the Project](#Run-the-Project)
5. [API and Usage](#API-and-Usage)
6. [Build Your Own Hardware](#Build_Your_Own_Hardware)


## HardWare and Operating System
### Development
For development, the OS should be **Ubuntu 18.04 LTS**
## Testing and Deployment 
For testing and depolyment, despite the os requirement above, the server/PC should also equips with [Xilinx Virtex UltraScale+ FPGA VCU1525 Development Kit](https://www.xilinx.com/products/boards-and-kits/vcu1525-a.html)

## Requirements and Dependencies

### Requirements
The [Xilinx Runtime v2018.3](https://www.xilinx.com/products/boards-and-kits/vcu1525-a.html#gettingStarted) should be installed.

If also want to compile the library from source, the **Xilinx Deployment Shell v2018.3**, **Xilinx Development Shell** and **SDAccel Design Environment v2018.3** should also be installed. You can find them through [this link](https://www.xilinx.com/products/boards-and-kits/vcu1525-a.html#gettingStarted).

### Dependencies
This library uses the [**cmake**](https://cmake.org/) (version >= 3.0) as the building system and the [**googletest**](https://github.com/google/googletest) as the test framework. It also depends on the [**tensorflow**](https://www.tensorflow.org/).


## Project File Tree
The project file structure is shown below,
````
.
+-- auto_compile # Generating hardware configurations and its hardware
+-- HLS_Codes # HLS codes of FlexCNN
+-- libsacc # library for integrating FlexCNN to TensorFlow
+-- SDx_Project # SDAccel Project for creating FPGA binary
+-- tf_DSA # TensorFlow codes for OpenPose application and our integrator
````


## Run the Project

1. In ./tf_DSA/tf_pose/sacc_utils.py change the following lines (give your project path)
	````python
    self.sa_dsa_path = '/path/to/libsacc/';
	self.custom_lib_path = '/path/to/libsacc/build/lib/libsacc.so';
	with open("/path/to/libsacc/inc/sacc_params.h", 'r') as fobj:
    ````
2. Follow the instructions in ./libsacc/README.md to install the library.
3. Setting environments
    ````bash
    cd tf_DSA
    source env.sh
    ````
4. Installing packages
	````bash
    python3 -m venv venv
	source venv/bin/activate
	sudo apt-get install build-essential libcap-dev
	pip3 install -r requirements.txt
	pip3 install sacc
	pip3 install yappi
	sudo apt install swig
	cd tf_pose/pafprocess/                                      
	swig -python -c++ pafprocess.i && python3 setup.py build_ext --inplace
    ````
5. Run the project
    ````bash
    ./test.sh
    ````
You should see a window opens with a man moving and his poses showing.

## API and Usage
### API
To Do

### Usage
To Do


## Build Your Own Hardware
To Do
 


