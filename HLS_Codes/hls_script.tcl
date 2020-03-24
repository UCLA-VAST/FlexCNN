open_project pose_prj
set_top top_kernel
add_files pose.h
add_files kernel.cpp
add_files cnn_sw.cpp
add_files common_header_U1.h
add_files 2DDataFeed_U1.cpp
add_files 2DDataCollect_U1.cpp
add_files 2DDataFeedCollect_U1.cpp
add_files 2DPE_U1.cpp
add_files -tb tb_pose.cpp
open_solution "solution1"
set_part {xc7vx690tffg1761-2} -tool vivado
create_clock -period 3 -name default
config_interface -m_axi_addr64 -m_axi_offset off -register_io off
csim_design -compiler gcc
#csynth_design
#cosim_design -trace_level all
#export_design -format ip_catalog

exit
