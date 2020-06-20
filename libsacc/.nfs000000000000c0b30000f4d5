Integrate systolic array accelerator to OpenPose progeam
===
This document presents how to integrate the systolic array accelerator to the openpose program (tensorflow version). The tensorflow version openpose can be found [here](https://github.com/ildoonet/tf-pose-estimation).

# Content
1. [Requirements and Dependencies](#Requirements-and-Dependencies)
2. [Integrated Version of OpenPose](#Integrated-Version-of-OpenPose)
3. [Patch for OpenPose](#Patch-for-OpenPose)

# Requirements and Dependencies

## Requirements
The OS should be **Ubuntu 16.04 LTS (kernel version >= 4.4)** and the server/PC should equip the [Xilinx Virtex UltraScale+ FPGA VCU1525 Development Kit](https://www.xilinx.com/products/boards-and-kits/vcu1525-a.html).

## Dependencies
The systolic array accelerator need the xilinx runtime support [Xilinx Runtime v2018.3](https://www.xilinx.com/products/boards-and-kits/vcu1525-a.html#gettingStarted). 

`Tensorflow` is also a necessity for the accelerator.

Currently, this integration happens on the [**tensorflow version of openpose program**](https://github.com/ildoonet/tf-pose-estimation) with commit `unknown yet`, and it uses the `mobilenet_thin` dataset.

# Integrated Version of OpenPose
Just retrieve the source of [this]() repo and set up the environment variable `XILINX_XRT` correspondingly, it is ready to go.

````bash
git clone https://github.com/hetong07/openposeDSA_mobilenet.git
git checkout --track origin/tf
````

# Patch for OpenPose
You can also apply the patch in the root of this project to original tensorflow version openpose program. Please note that the patch is only tested under the commit `unknown yet`.

Commands to apply the patch are shown below,
````bash
git clone https://github.com/ildoonet/tf-pose-estimation.git
git checkout [unknown yet]
cp openposeDSA.tf.patch openpose/
cd openpose
patch -p1 < openposeDSA.tf.patch
````
Before running the openpose program, please first switch to the root of the openpose project and run `source config.sh` command.
