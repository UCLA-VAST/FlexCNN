#
# this file was created by a computer. trust it.
#

# compiler tools
XILINX_VIVADO_HLS ?= $(XILINX_SDX)/Vivado_HLS

SDX_CXX ?= $(XILINX_SDX)/bin/xcpp
XOCC ?= $(XILINX_SDX)/bin/xocc
RM = rm -f
RMDIR = rm -rf

#SDX_PLATFORM = xilinx_aws-vu9p-f1-04261818_dynamic_5_0
SDX_PLATFORM = xilinx_vcu1525_xdma_201830_1
XILINX_XRT = /opt/xilinx/xrt

# host compiler global settings
CXXFLAGS += -DSDX_PLATFORM=$(SDX_PLATFORM) -D__USE_XOPEN2K8 -I/opt/tools/xilinx/SDx/2018.3/runtime/include/1_2/ -I/opt/tools/xilinx/SDx//2018.3/include/ -O2 -Wall -c -fmessage-length=0 -std=c++14 -I/opt/xilinx/xrt/include/
LDFLAGS += -L/opt/xilinx/xrt/lib/ -lxilinxopencl -lpthread -lrt -lstdc++ -L/opt/tools/xilinx/SDx//2018.3/runtime/lib/x86_64

# kernel compiler global settings
XOCC_OPTS = -t hw --save-temps --report system --max_memory_ports top_kernel --platform $(SDX_PLATFORM) -O3 --sp top_kernel_1.m_axi_gmem1:bank0 --sp top_kernel_1.m_axi_gmem2:bank1 --sp top_kernel_1.m_axi_gcontrol:bank0

#
# OpenCL kernel files
#

BINARY_CONTAINERS += binary_container_1.xclbin

BUILD_SUBDIRS += binary_container_1
BINARY_CONTAINER_1_OBJS += binary_container_1/top_kernel.xo
ALL_KERNEL_OBJS += binary_container_1/top_kernel.xo

ALL_MESSAGE_FILES = $(subst .xo,.mdb,$(ALL_KERNEL_OBJS)) $(subst .xclbin,.mdb,$(BINARY_CONTAINERS))

#
# host files
#

HOST_OBJECTS += src/cnn_sw.o
HOST_OBJECTS += src/host.o
HOST_OBJECTS += src/xcl2.o

HOST_EXE = pose_prj.exe

BUILD_SUBDIRS += src/

#
# primary build targets
#

.PHONY: all clean
all: $(BINARY_CONTAINERS) $(HOST_EXE)

clean:
	-$(RM) $(BINARY_CONTAINERS) $(ALL_KERNEL_OBJS) $(ALL_MESSAGE_FILES)  $(HOST_EXE) $(HOST_OBJECTS)
	-$(RM) *.xclbin.sh
	-$(RMDIR) $(BUILD_SUBDIRS)
	-$(RMDIR) _xocc*
	-$(RMDIR) .Xil

.PHONY: incremental
incremental: all


nothing:

#
# binary container: binary_container_1.xclbin
#

binary_container_1/top_kernel.xo: ../src/hw_kernel.cpp ../src/pose.h /opt/tools/xilinx/Vivado/2018.3/include/hls_stream.h /opt/tools/xilinx/Vivado/2018.3/include/ap_int.h /opt/tools/xilinx/Vivado/2018.3/include/ap_fixed.h
	@mkdir -p $(@D)
	-@$(RM) $@
	$(XOCC) $(XOCC_OPTS) -c -k top_kernel --max_memory_ports top_kernel --messageDb $(subst .xo,.mdb,$@) -I"$(<D)" --xp misc:solution_name=_xocc_compile_binary_container_1_top_kernel -o"$@" "$<" --kernel_frequency 310

binary_container_1.xclbin: $(BINARY_CONTAINER_1_OBJS)
	-@echo $(XOCC) $(XOCC_OPTS) -l --nk top_kernel:1 --messageDb $(subst .xclbin,.mdb,$@) --xp misc:solution_name=_xocc_link_binary_container_1 --remote_ip_cache /home/centos/src/project_data/200G_workspace/openpose_sdx/ip_cache -o"$@" $(+) > binary_container_1.xclbin.sh
	$(XOCC) $(XOCC_OPTS) -l --nk top_kernel:1 --messageDb $(subst .xclbin,.mdb,$@) --xp misc:solution_name=_xocc_link_binary_container_1 --remote_ip_cache /home/centos/src/project_data/200G_workspace/openpose_sdx/ip_cache -o"$@" $(+) --kernel_frequency 310

#
# host rules
#

src/cnn_sw.o: ../src/cnn_sw.cpp ../src/pose.h
	@mkdir -p $(@D)
	$(SDX_CXX) $(CXXFLAGS) -DSDX_PLATFORM=$(SDX_PLATFORM) -D__USE_XOPEN2K8 -I/opt/tools/xilinx/SDx/2018.3/runtime/include/1_2/ -I/opt/tools/xilinx/Vivado/2018.3/include/ -O2 -Wall -c -fmessage-length=0 -o "$@" "$<"

src/host.o: ../src/host.cpp ../src/xcl2.hpp ../src/pose.h
	@mkdir -p $(@D)
	$(SDX_CXX) $(CXXFLAGS) -DSDX_PLATFORM=$(SDX_PLATFORM) -D__USE_XOPEN2K8 -I/opt/tools/xilinx/SDx/2018.3/runtime/include/1_2/ -I/opt/tools/xilinx/Vivado/2018.3/include/ -O2 -Wall -c -fmessage-length=0 -o "$@" "$<"

src/xcl2.o: ../src/xcl2.cpp ../src/xcl2.hpp
	@mkdir -p $(@D)
	$(SDX_CXX) $(CXXFLAGS) -DSDX_PLATFORM=$(SDX_PLATFORM) -D__USE_XOPEN2K8 -I/opt/tools/xilinx/SDx/2018.3/runtime/include/1_2/ -I/opt/tools/xilinx/Vivado/2018.3/include/ -O2 -Wall -c -fmessage-length=0 -o "$@" "$<"

$(HOST_EXE): $(HOST_OBJECTS)
	$(SDX_CXX) -o "$@" $(+) $(LDFLAGS) -lxilinxopencl -lpthread -lrt -lstdc++ -L/opt/tools/xilinx/SDx/2018.3/runtime/lib/x86_64
