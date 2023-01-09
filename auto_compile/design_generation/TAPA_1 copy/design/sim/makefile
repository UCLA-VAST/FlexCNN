PLATFORM := xilinx_u250_xdma_201830_2


# sources
KERNEL_SRC := ../src/hw_kernel.cpp

HOST_SRC := \
../src/hw_kernel.cpp \
../src/util.h \
../src/params.h \
../src/cnn_sw.cpp \
../src/cnn_sw.h \
../src/hw_kernel_tb.cpp

TOP_KERNEL := top_kernel

# targets
HOST_EXE := host.exe

CFLAGS := -ltapa -lfrt -lglog -lgflags -lOpenCL -I/opt/tools/xilinx/Vitis_HLS/$(VIV_VER)/include/

XOBJ := $(TOP_KERNEL).$(PLATFORM).hw.xo

XCLBIN_SCRIPT := $(TOP_KERNEL).$(PLATFORM).hw_generate_bitstream.sh

# default make host
host: $(HOST_EXE)

hw_all: host syn xclbin

csim_all: host run_csim

cosim_all: host syn cosim run_cosim

# host rules
$(HOST_EXE): $(HOST_SRC)
	g++ -o $@ -O3 $+ $(CFLAGS) 
	@echo 'Compiled Host Executable: $(HOST_EXE)'

xclbin: build/$(XCLBIN_SCRIPT)
	cd build;	bash $(XCLBIN_SCRIPT)


# syn: $(KERNEL_SRC)
# 	mkdir -p build
# 	tapa \
# 	--work-dir build \
# 	compile \
# 	--top $(TOP_KERNEL) \
# 	--platform $(PLATFORM) \
# 	--clock-period 3.33 \
# 	-o build/$(XOBJ) \
# 	--input ../src/hw_kernel.cpp \
# 	optimize-floorplan \
# 	--connectivity link_config.ini \
# 	--floorplan-strategy QUICK_FLOORPLANNING \
# 	link \
# 	--floorplan-output ./build/constraint.tcl

syn: $(KERNEL_SRC)
	mkdir -p build
	tapac \
	--work-dir build \
	--top $(TOP_KERNEL) \
	--platform $(PLATFORM) \
	--clock-period 3.33 \
	-o build/$(XOBJ) \
  --connectivity link_config.ini \
	--enable-synth-util \
	--max-parallel-synth-jobs 16 \
	--floorplan-output build/constraint.tcl \
	--floorplan-strategy QUICK_FLOORPLANNING \
	$+
	echo >> build/constraint.tcl
	echo "set_property USER_SLL_REG 0 [get_cells -hierarchical -regexp {.*_q\d+_reg.*}]" >> build/constraint.tcl
	
	
# sed -i '41d' build/top_kernel.xilinx_u250_xdma_201830_2.hw_generate_bitstream.sh
	
cosim: $(HOST_SRC)
	cd build; \
	v++ -o $(TOP_KERNEL).$(PLATFORM).hw_emu.xclbin \
	--link \
	--target hw_emu \
	--config ../connectivity.cfg \
	--kernel $(TOP_KERNEL) \
	--platform $(PLATFORM) \
	--advanced.param compiler.fsanitize=address,memory \
	--advanced.param compiler.deadlockDetection=TRUE \
  $(XOBJ)



run_csim: $(HOST_EXE)
	mkdir -p outputs
	./$(HOST_EXE) $(START_INST) $(END_INST) > ./outputs/csim_output.log

run_cosim: build/$(TOP_KERNEL).$(PLATFORM).hw_emu.xclbin $(HOST_EXE)
	mkdir -p outputs
	./$(HOST_EXE) $(START_INST) $(END_INST) --bitstream=./build/$(TOP_KERNEL).$(PLATFORM).hw_emu.xclbin > ./outputs/cosim_output.log

run_fpga: top_kernel_xilinx_u250_xdma_201830_2.xclbin $(HOST_EXE)
	mkdir -p outputs
	./$(HOST_EXE) $(START_INST) $(END_INST) --bitstream=./top_kernel_xilinx_u250_xdma_201830_2.xclbin > ./outputs/fpga_output.log

run_tapa_cosim: build/$(XOBJ) $(HOST_EXE)
	mkdir -p outputs
	./$(HOST_EXE) --bitstream=./build/$(XOBJ) > ./outputs/tapa_cosim_output.log
