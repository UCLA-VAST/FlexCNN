open_project hls_[lindex $argv 0]
set_top top_kernel
add_files [lindex $argv 1]/src/util.h
add_files [lindex $argv 1]/src/hw_kernel.cpp
#add cnn_sw only if $argv 0 is not csynth_design
if { [lindex $argv 0] != "csynth_design" } {
  add_files [lindex $argv 1]/src/cnn_sw.cpp
  add_files [lindex $argv 1]/src/cnn_sw.h
  add_files -tb [lindex $argv 1]/src/hw_kernel_tb.cpp
}
open_solution "solution1"
set_part {xcvu9p-fsgd2104-2L-e}
create_clock -period 3 -name default
config_interface -m_axi_addr64 -m_axi_offset off -register_io off
[lindex $argv 0]

exit
