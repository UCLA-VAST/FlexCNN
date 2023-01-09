
/**
You can add any new module that you want here.
You just need to follow the coding structure of the other modules.
You can uncomment the commented modules if your application needs them. All the modules listed in this file (commneted or uncommented) has a working implementation.
For convolution, you can either choose to use a naive implementation (which is slow), or add the systolic array kernel.
**/
void top_kernel(
  bus_mem_0 dram_b0,
  bus_mem_1 dram_b1,
  bus_mem_3 layer_config,
  uint start_inst,
  uint end_inst
){
	tapa::stream<CinLoadData0Type, 128> fifo_cin_load("fifo_cin_load");
	tapa::stream<CinLoadData0Type, 128> fifo_cin_prev_0("fifo_cin_prev_0");
	tapa::stream<CinLoadData0Type, 128> fifo_beta_conv("fifo_beta_conv");
	tapa::stream<CinLoadData0Type, 128> fifo_gamma_conv("fifo_gamma_conv");
  tapa::stream<CinLoadData0Type, 128> fifo_weight_load("fifo_weight_load");
  tapa::stream<CinLoadData0Type, 128> fifo_pool_0("fifo_pool_0");
	tapa::stream<CinLoadData0Type, 128> fifo_upsample_0("fifo_upsample_0");
	tapa::stream<CinLoadData0Type, 128> fifo_concat_0("fifo_concat_0");
	tapa::stream<CinLoadData0Type, 128> fifo_concat_1("fifo_concat_1");
	tapa::stream<CinLoadData0Type, 128> fifo_add_0("fifo_add_0");
	tapa::stream<CinLoadData0Type, 128> fifo_relu_0("fifo_relu_0");
  tapa::stream<CinLoadData0Type, 128> fifo_SA("fifo_SA");

  tapa::stream<ConfigInst,16> config_prev_load("config_prev_load");
  tapa::stream<ConfigInst,16> config_bias_load("config_bias_load");
  tapa::stream<ConfigInst,16> config_weight_load("config_weight_load");
  tapa::stream<ConfigInst,16> config_SA("config_SA");
  tapa::stream<ConfigInst,16> config_upsample("config_upsample");
  tapa::stream<ConfigInst,16> config_pool("config_pool");
  tapa::stream<ConfigInst,16> config_concat("config_concat");
  tapa::stream<ConfigInst,16> config_add("config_add");
  tapa::stream<ConfigInst,16> config_relu("config_relu");
	tapa::stream<ConfigInst,16> config_data_write("config_data_write");

  tapa::stream<int> cin_to_cout_sync("cin_to_cout_sync");
  tapa::stream<int> cout_to_cin_sync("cout_to_cin_sync");

  tapa::task()
  .invoke(cin_load, 
      start_inst, end_inst,
			dram_b0, 
			layer_config,
			fifo_cin_load, 
			config_prev_load,
      cout_to_cin_sync, cin_to_cout_sync
  )
  .invoke(cin_load_prev,
      start_inst, end_inst,
      dram_b0,
			config_prev_load,
			fifo_cin_prev_0,
			config_bias_load
  )
  .invoke(bias_load,
      start_inst, end_inst,
      dram_b1,
			config_bias_load,
      fifo_beta_conv, fifo_gamma_conv,
			config_weight_load
  )
  .invoke(weight_load,
      start_inst, end_inst,
      dram_b1,
			config_weight_load,
			fifo_weight_load,
			config_SA
  )
  .invoke(SA,
      start_inst, end_inst,
      fifo_cin_load,
      fifo_weight_load,
      fifo_SA,
      config_SA,
      config_upsample
  )
  .invoke(upsample,
      start_inst, end_inst,
      fifo_SA,
      config_upsample,
      fifo_upsample_0,
      config_pool
  )
  .invoke(pool,
      start_inst, end_inst,
      fifo_cin_prev_0,
      config_pool,
      fifo_pool_0,
      config_concat
	)
  .invoke(concat,
    start_inst, end_inst,
    fifo_pool_0,
    fifo_upsample_0,
    config_concat,
    fifo_concat_0, //pool
    fifo_concat_1, //conv and concat
    config_add
  )
	.invoke(add,
    start_inst, end_inst,
    fifo_concat_0,
    fifo_concat_1,
    config_add,
    fifo_add_0,
    config_relu
	)
  .invoke(relu,
    start_inst, end_inst,
    fifo_add_0, 
    config_relu,
    fifo_relu_0,
    config_data_write,
    fifo_beta_conv, fifo_gamma_conv
	)
  .invoke(cout_write, 
      start_inst, end_inst,
			fifo_relu_0,
			config_data_write,
			dram_b0,
      cin_to_cout_sync, cout_to_cin_sync
	);
}
