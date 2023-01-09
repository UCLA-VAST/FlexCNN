@echo off

SET VAR0=sim
SET VAR1=cosim
SET VAR2=syn

SET hls=vitis_hls


$hls hls.tcl csim_design %~2 -l hls_sim.log
