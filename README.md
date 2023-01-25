# FlexCNN

## Publication
+ Suhail Basalama, Atefeh Sohrabizadeh, Jie Wang, Licheng Guo, Jason Cong. [FlexCNN: An End-to-End Framework for Composing CNN Accelerators on FPGA](https://dl.acm.org/doi/abs/10.1145/3570928). In TRETS, 2022.

+ Atefeh Sohrabizadeh, Jie Wang, Jason Cong. [End-to-End Optimization of Deep Learning Applications](https://dl.acm.org/doi/abs/10.1145/3373087.3375321). In FPGA, 2020.

## About
This repo contains the codes for building FlexCNN, an accelerator for running CNNs on FPGA, described in the papers above. As mentioned in the papers, you can further integrate FlexCNN to TensorFlow and offload CNN computation of your application to FPGA.


The  latest version of FlexCNN is tested on U-Net, E-Net, and VGG16.

## Content
1. [Hardware and Operating System](#hardware-and-operating-system)
2. [Requirements and Dependencies](#requirements-and-dependencies)
3. [Testing and Deployment](#testing-and-deployment)
6. [Citation](#citation)


## Hardware and Operating System
### Development
For development, the OS should be **Ubuntu 18.04.6 LTS**

## Requirements and Dependencies

### Requirements
FlexCNN generate Vitis HLS code or TAPA HLS code (TAPA code is optimized for better P&R). 
The netwrorks are tested using Vitis 2021.2.
To install TAPA, follow this [tutorial](https://tapa.readthedocs.io/en/latest/installation.html).


### Dependencies
You should have Python 3.8 installed with the libraries in requirements.txt

## Testing and Deployment 
For testing and depolyment, despite the os requirement above, the server/PC should also be equipped with Alveo [U250](https://www.xilinx.com/products/boards-and-kits/alveo/u250.html) or [U280](https://www.xilinx.com/products/boards-and-kits/alveo/u280.html) Data Center Accelerator Cards.

### Run the Tested CNNs
1. 
	````bash
    source env.sh
		mkdir $STREAM_VSA_PATH/auto_compile/data/onnx
    ````

2. Download the ONNX file for [U-Net](https://drive.google.com/file/d/12LAthDE-FELXpnM8Rz4TPr6dVL6QfTNL/view?usp=share_link), [E-Net](https://drive.google.com/file/d/1H0MiMeHNRmirkCGn5dMC3sof_PFR8Q53/view?usp=share_link), or [VGG16](https://drive.google.com/file/d/1Gckmst34D8OEJKBSb_IgZrV3-vc2NyA5/view?usp=share_link), and put it in 			

	````bash 
	$STREAM_VSA_PATH/auto_compile/data/onnx/
	````

2. Generate the the design for CNN specified in generate_design.sh: 
	````bash
	./generate_design.sh
    ````
	````bash
	echo 13 | $STREAM_VSA_PATH/auto_compile/run.sh ENet ENet 32 u250 4 TAPA_1
	````
	This command runs the framework for ENet targeting 32-bit float implementation on u250 generating TAPA code. '4' is used for memory allocation between BRAMs and URAMs. 

	This will generate a design under the ''designs'' directory
3. To run the C-simultation, run:
	````bash
	cd designs/TAPA_1_ENet_8_9_8_MEM_4_32
	source env.sh # to set up TAPA and Xilinx libraries
	cd sim
	make csim_all
	#./host.exe start_instruction_id end_instruction_id
	./host.exe 1 89 # for ENet
    ````
	C-simulation should be outputed to sim/outputs/csim_output.log
4. To generate the bitstream, run:
	````bash
	cd designs/TAPA_1_ENet_8_9_8_MEM_4_32
	source env.sh # to set up TAPA and Xilinx libraries
	cd sim
	make hw_all
	#./host.exe start_instruction_id end_instruction_id
	./host.exe 1 89 --bitstream=path_to/your_bitstream.xclbin # for ENet
    ````

## Citation
If you find any of the ideas/codes useful for your research, please cite our papers:

	@article{basalama2022flexcnn,
		title={FlexCNN: An End-to-End Framework for Composing CNN Accelerators on FPGA},
		author={Basalama, Suhail and Sohrabizadeh, Atefeh and Wang, Jie and Guo, Licheng and Cong, Jason},
		journal={ACM Transactions on Reconfigurable Technology and Systems},
		year={2022},
		publisher={ACM New York, NY}
	}

	@inproceedings{sohrabizadeh2020end,
	  title={End-to-End Optimization of Deep Learning Applications},
	  author={Sohrabizadeh, Atefeh and Wang, Jie and Cong, Jason},
	  booktitle={The 2020 ACM/SIGDA International Symposium on Field-Programmable Gate Arrays},
	  pages={133--139},
	  year={2020}
	}


