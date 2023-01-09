/**
 *  This file is automatically generated by PolySA CodeGen.
 *  Version: 1.0
 *  Authos: Jie Wang
 */

#include "common_header_U1.h"

void U1_Data2WriteData0(
  U1_data_t2 buffer[U1_DATA2_FC_GROUP_FACTOR][U1_DATA2_BUF_SIZE / U1_DATA2_FC_SIMD_FACTOR][U1_DATA2_FC_SIMD_FACTOR],
  stream<U1_Data2TransferChannelType> &fifo_transfer_in,
  stream<U1_Data2TransferChannelType> &fifo_transfer_out,
  unsigned int engine_id,
  uint LAYER_OUT_NUM_T,
  uint LAYER_IN_IMG_H_T,
  uint LAYER_IN_IMG_W_T,
  uint LAYER_COL_IL_FACTOR,
  uint LAYER_STRIDE,
  uint LAYER_TCONV_STRIDE
){
#pragma HLS INLINE off

  bool LAST_ENGINE = (engine_id == 14 / U1_DATA2_FC_SPLIT_FACTOR - 1);

  bool more_to_read_from_buffer = true;
  bool more_to_collect_from_sys_arr = true;
  bool data_is_from_local_buffer;
  bool data_is_from_external_buffer;
  ap_uint<9> oo = 0;
  ap_uint<7> h = 0;
  ap_uint<7> h_bound = LAYER_IN_IMG_H_T*LAYER_TCONV_STRIDE / LAYER_STRIDE;
  ap_uint<10> w = 0;
  ap_uint<10> w_bound = LAYER_IN_IMG_W_T*LAYER_TCONV_STRIDE / LAYER_STRIDE;
  LAYER_COL_IL_FACTOR *= LAYER_TCONV_STRIDE;
  bool done = 0;

  while(!done){
#pragma HLS PIPELINE II=1
    ap_uint<19> local_buf_idx = h * LAYER_COL_IL_FACTOR * LAYER_OUT_NUM_T + (w % LAYER_COL_IL_FACTOR) * LAYER_OUT_NUM_T + oo * U1_DATA2_FC_SIMD_FACTOR;
    if (w >= engine_id * LAYER_COL_IL_FACTOR){
      ap_uint<10> collector_id = w / LAYER_COL_IL_FACTOR;
      data_is_from_local_buffer = (collector_id == engine_id);
      data_is_from_external_buffer = !data_is_from_local_buffer;

      U1_Data2TransferChannelType data_write_to_fifo;

      if (data_is_from_external_buffer){
        data_write_to_fifo = fifo_transfer_in.read();
      } else {
        U1_data_t2 data0 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][0];
        ap_uint<U1_DATA2_WIDTH> data0_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data0);
        U1_data_t2 data1 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][1];
        ap_uint<U1_DATA2_WIDTH> data1_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data1);
        U1_data_t2 data2 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][2];
        ap_uint<U1_DATA2_WIDTH> data2_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data2);
        U1_data_t2 data3 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][3];
        ap_uint<U1_DATA2_WIDTH> data3_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data3);
        U1_data_t2 data4 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][4];
        ap_uint<U1_DATA2_WIDTH> data4_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data4);
        U1_data_t2 data5 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][5];
        ap_uint<U1_DATA2_WIDTH> data5_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data5);
        U1_data_t2 data6 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][6];
        ap_uint<U1_DATA2_WIDTH> data6_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data6);
        U1_data_t2 data7 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][7];
        ap_uint<U1_DATA2_WIDTH> data7_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data7);
        ap_uint<U1_DATA2_WIDTH * U1_DATA2_FC_SIMD_FACTOR> pack_data = (
          data7_cast,
          data6_cast,
          data5_cast,
          data4_cast,
          data3_cast,
          data2_cast,
          data1_cast,
          data0_cast
        );
        data_write_to_fifo.data = pack_data;
      }

      fifo_transfer_out.write(data_write_to_fifo);
    }
    w++;
    if (w == w_bound){
      w = 0;
      h++;
      if (h == h_bound){
        h = 0;
        oo++;
        if (oo == LAYER_OUT_NUM_T / U1_DATA2_FC_SIMD_FACTOR){
          oo = 0;
          done = 1;
        }
      }
    }
  }

}

void U1_Data2WriteDataLast(
  U1_data_t2 buffer[U1_DATA2_FC_GROUP_FACTOR][U1_DATA2_BUF_SIZE / U1_DATA2_FC_SIMD_FACTOR][U1_DATA2_FC_SIMD_FACTOR],
  stream<U1_Data2TransferChannelType> &fifo_transfer_out,
  unsigned int engine_id,
  uint LAYER_OUT_NUM_T,
  uint LAYER_IN_IMG_H_T,
  uint LAYER_IN_IMG_W_T,
  uint LAYER_COL_IL_FACTOR,
  uint LAYER_STRIDE,
  uint LAYER_TCONV_STRIDE
){
#pragma HLS INLINE off

  bool LAST_ENGINE = (engine_id == 14 / U1_DATA2_FC_SPLIT_FACTOR - 1);

  bool more_to_read_from_buffer = true;
  bool more_to_collect_from_sys_arr = true;
  bool data_is_from_local_buffer;
  bool data_is_from_external_buffer;
  ap_uint<9> oo = 0;
  ap_uint<7> h = 0;
  ap_uint<7> h_bound = LAYER_IN_IMG_H_T*LAYER_TCONV_STRIDE / LAYER_STRIDE;
  ap_uint<10> w = 0;
  ap_uint<10> w_bound = LAYER_IN_IMG_W_T*LAYER_TCONV_STRIDE / LAYER_STRIDE;
  LAYER_COL_IL_FACTOR *= LAYER_TCONV_STRIDE;
  bool done = 0;

  while(!done){
#pragma HLS PIPELINE II=1
    ap_uint<19> local_buf_idx = h * LAYER_COL_IL_FACTOR * LAYER_OUT_NUM_T + (w % LAYER_COL_IL_FACTOR) * LAYER_OUT_NUM_T + oo * U1_DATA2_FC_SIMD_FACTOR;
    if (w >= engine_id * LAYER_COL_IL_FACTOR){
      ap_uint<10> collector_id = w / LAYER_COL_IL_FACTOR;
      data_is_from_local_buffer = (collector_id == engine_id);
      data_is_from_external_buffer = !data_is_from_local_buffer;

      U1_Data2TransferChannelType data_write_to_fifo;

      if (data_is_from_external_buffer){
      } else {
        U1_data_t2 data0 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][0];
        ap_uint<U1_DATA2_WIDTH> data0_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data0);
        U1_data_t2 data1 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][1];
        ap_uint<U1_DATA2_WIDTH> data1_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data1);
        U1_data_t2 data2 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][2];
        ap_uint<U1_DATA2_WIDTH> data2_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data2);
        U1_data_t2 data3 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][3];
        ap_uint<U1_DATA2_WIDTH> data3_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data3);
        U1_data_t2 data4 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][4];
        ap_uint<U1_DATA2_WIDTH> data4_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data4);
        U1_data_t2 data5 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][5];
        ap_uint<U1_DATA2_WIDTH> data5_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data5);
        U1_data_t2 data6 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][6];
        ap_uint<U1_DATA2_WIDTH> data6_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data6);
        U1_data_t2 data7 = buffer[0][local_buf_idx / U1_DATA2_FC_SIMD_FACTOR][7];
        ap_uint<U1_DATA2_WIDTH> data7_cast = Reinterpret<ap_uint<U1_DATA2_WIDTH> >(data7);
        ap_uint<U1_DATA2_WIDTH * U1_DATA2_FC_SIMD_FACTOR> pack_data = (
          data7_cast,
          data6_cast,
          data5_cast,
          data4_cast,
          data3_cast,
          data2_cast,
          data1_cast,
          data0_cast
        );
        data_write_to_fifo.data = pack_data;
      }

      fifo_transfer_out.write(data_write_to_fifo);
    }
    w++;
    if (w == w_bound){
      w = 0;
      h++;
      if (h == h_bound){
        h = 0;
        oo++;
        if (oo == LAYER_OUT_NUM_T / U1_DATA2_FC_SIMD_FACTOR){
          oo = 0;
          done = 1;
        }
      }
    }
  }

}

void U1_Data2ReadData0(
  U1_data_t2 buffer[U1_DATA2_FC_GROUP_FACTOR][U1_DATA2_BUF_SIZE / U1_DATA2_FC_SIMD_FACTOR][U1_DATA2_FC_SIMD_FACTOR],
  stream<U1_Data2PEChannelType> &fifo_collect_0,
  uint LAYER_IN_IMG_H_T,
  uint LAYER_ROW_IL_FACTOR,
  uint LAYER_COL_IL_FACTOR,
  uint LAYER_STRIDE,
  uint LAYER_TCONV_STRIDE
){
#pragma HLS INLINE off

  bool more_to_collect_from_sys_arr = true;
  ap_uint<3> buffer_gs_id = 0;
  ap_uint<16> buffer_read_counter = 0;
  ap_uint<5> c0_counter = 0;
  ap_uint<4> c1_counter = 0;
  ap_uint<4> c2_counter = 0;
  ap_uint<4> c3_counter = 0;
  ap_uint<10> col_counter = 0;
  ap_uint<10> row_counter = 0;
  ap_uint<7> c0_counter_bound = LAYER_IN_IMG_H_T / LAYER_STRIDE;

  while(more_to_collect_from_sys_arr){
#pragma HLS PIPELINE II=1
    ap_uint<20> offset0 = c0_counter * LAYER_COL_IL_FACTOR*LAYER_TCONV_STRIDE*LAYER_TCONV_STRIDE * U1_SA_ROWS * LAYER_ROW_IL_FACTOR;
    ap_uint<20> offset1 = c2_counter*LAYER_TCONV_STRIDE * U1_SA_ROWS * LAYER_ROW_IL_FACTOR;
    ap_uint<20> offset2 = ((U1_SA_ROWS - 1 - c3_counter) * LAYER_ROW_IL_FACTOR + c1_counter);
    ap_uint<20> offset3 = U1_SA_ROWS*(row_counter*LAYER_COL_IL_FACTOR*LAYER_ROW_IL_FACTOR*LAYER_TCONV_STRIDE+LAYER_ROW_IL_FACTOR*col_counter);

    ap_uint<16> buffer_ind_to_collect_from_sys_arr = offset0 + offset1 + offset2 + offset3;
    U1_Data2PEChannelType data_to_collect_0;
    data_to_collect_0 = fifo_collect_0.read();
    buffer[0][buffer_ind_to_collect_from_sys_arr / U1_DATA2_FC_SIMD_FACTOR][buffer_ind_to_collect_from_sys_arr % U1_DATA2_FC_SIMD_FACTOR] = data_to_collect_0.data;

    // counter logic
    c0_counter++;
    if (c0_counter == c0_counter_bound){
      c0_counter = 0;
      c1_counter++;
      if (c1_counter == LAYER_ROW_IL_FACTOR){
        c1_counter = 0;
        c2_counter++;
        if (c2_counter == LAYER_COL_IL_FACTOR){
          c2_counter = 0;
          col_counter++;
          if (col_counter == LAYER_TCONV_STRIDE){
            col_counter = 0;
            row_counter++;
            if (row_counter == LAYER_TCONV_STRIDE){
              row_counter = 0;
              c3_counter++;
              if (c3_counter == U1_SA_ROWS){
                c3_counter = 0;
                more_to_collect_from_sys_arr = false;
              }
            }
          }
        }
      }
    }
  }
}

void U1_DataCollect2Engine0(
  stream<U1_Data2TransferChannelType> &fifo_transfer_in,
  stream<U1_Data2TransferChannelType> &fifo_transfer_out,
  stream<U1_Data2PEChannelType> &fifo_collect_0,
  unsigned int engine_id,
  stream<uint> &fifo_config_in0, // from PE
  stream<uint> &fifo_config_in1, // from other engines
  stream<uint> &fifo_config_out
){
#pragma HLS DATA_PACK variable=fifo_transfer_in
#pragma HLS DATA_PACK variable=fifo_transfer_out
#pragma HLS DATA_PACK variable=fifo_collect_0
#pragma HLS INLINE off

  uint LAYER_OUT_NUM_T_prev;
  uint LAYER_IN_IMG_H_T_prev;
  uint LAYER_IN_IMG_W_T_prev;
  uint LAYER_COL_IL_FACTOR_prev;
  uint LAYER_STRIDE_prev;
  uint LAYER_TCONV_STRIDE_prev;
  uint task_iter = 0;
  // read in configurations
  uint LAYER_IN_NUM_T = fifo_config_in0.read();
  uint LAYER_OUT_NUM_T = fifo_config_in0.read();
  uint LAYER_IN_IMG_H_T = fifo_config_in0.read();
  uint LAYER_IN_IMG_W_T = fifo_config_in0.read();
  uint LAYER_FILTER_S_H = fifo_config_in0.read();
  uint LAYER_FILTER_S_W = fifo_config_in0.read();
  uint LAYER_TASK_NUM1 = fifo_config_in0.read();
  uint LAYER_TASK_NUM2 = fifo_config_in0.read();
  uint LAYER_LOCAL_ACCUM_NUM = fifo_config_in0.read();
  uint LAYER_LOCAL_REG_NUM = fifo_config_in0.read();
  uint LAYER_ROW_IL_FACTOR = fifo_config_in0.read();
  uint LAYER_COL_IL_FACTOR = fifo_config_in0.read();
  uint LAYER_STRIDE = fifo_config_in0.read();
  uint LAYER_BATCH = fifo_config_in0.read();

  uint LAYER_CONV_TYPE = fifo_config_in0.read();
  uint FILTER_D0_H = fifo_config_in0.read();
  uint FILTER_D0_W = fifo_config_in0.read();
  uint FILTER_D1_H = fifo_config_in0.read();
  uint FILTER_D1_W = fifo_config_in0.read();
  uint LAYER_DILATION_RATE = fifo_config_in0.read();
  uint LAYER_TCONV_STRIDE = fifo_config_in0.read();
  uint K_NUM = fifo_config_in0.read();
  ap_uint<32> KH = fifo_config_in0.read();
  ap_uint<32> KW = fifo_config_in0.read();
  // dummpy read
  LAYER_IN_NUM_T = fifo_config_in1.read();
  LAYER_OUT_NUM_T = fifo_config_in1.read();
  LAYER_IN_IMG_H_T = fifo_config_in1.read();
  LAYER_IN_IMG_W_T = fifo_config_in1.read();
  LAYER_FILTER_S_H = fifo_config_in1.read();
  LAYER_FILTER_S_W = fifo_config_in1.read();
  LAYER_TASK_NUM1 = fifo_config_in1.read();
  LAYER_TASK_NUM2 = fifo_config_in1.read();
  LAYER_LOCAL_ACCUM_NUM = fifo_config_in1.read();
  LAYER_LOCAL_REG_NUM = fifo_config_in1.read();
  LAYER_ROW_IL_FACTOR = fifo_config_in1.read();
  LAYER_COL_IL_FACTOR = fifo_config_in1.read();
  LAYER_STRIDE = fifo_config_in1.read();
  LAYER_BATCH = fifo_config_in1.read();

  LAYER_CONV_TYPE = fifo_config_in1.read();
  FILTER_D0_H = fifo_config_in1.read();
  FILTER_D0_W = fifo_config_in1.read();
  FILTER_D1_H = fifo_config_in1.read();
  FILTER_D1_W = fifo_config_in1.read();
  LAYER_DILATION_RATE = fifo_config_in1.read();
  LAYER_TCONV_STRIDE = fifo_config_in1.read();
  K_NUM = fifo_config_in1.read();
  KH = fifo_config_in1.read();
  KW = fifo_config_in1.read();
  // write out configurations
  fifo_config_out.write(LAYER_IN_NUM_T);
  fifo_config_out.write(LAYER_OUT_NUM_T);
  fifo_config_out.write(LAYER_IN_IMG_H_T);
  fifo_config_out.write(LAYER_IN_IMG_W_T);
  fifo_config_out.write(LAYER_FILTER_S_H);
  fifo_config_out.write(LAYER_FILTER_S_W);
  fifo_config_out.write(LAYER_TASK_NUM1);
  fifo_config_out.write(LAYER_TASK_NUM2);
  fifo_config_out.write(LAYER_LOCAL_ACCUM_NUM);
  fifo_config_out.write(LAYER_LOCAL_REG_NUM);
  fifo_config_out.write(LAYER_ROW_IL_FACTOR);
  fifo_config_out.write(LAYER_COL_IL_FACTOR);
  fifo_config_out.write(LAYER_STRIDE);
  fifo_config_out.write(LAYER_BATCH);

  fifo_config_out.write(LAYER_CONV_TYPE);
  fifo_config_out.write(FILTER_D0_H);
  fifo_config_out.write(FILTER_D0_W);
  fifo_config_out.write(FILTER_D1_H);	
  fifo_config_out.write(FILTER_D1_W);	
  fifo_config_out.write(LAYER_DILATION_RATE);
  fifo_config_out.write(LAYER_TCONV_STRIDE);
  fifo_config_out.write(K_NUM);
  fifo_config_out.write(KH);
  fifo_config_out.write(KW);
  U1_data_t2 ping_buffer[U1_DATA2_FC_GROUP_FACTOR][U1_DATA2_BUF_SIZE / U1_DATA2_FC_SIMD_FACTOR][U1_DATA2_FC_SIMD_FACTOR];
  U1_data_t2 pong_buffer[U1_DATA2_FC_GROUP_FACTOR][U1_DATA2_BUF_SIZE / U1_DATA2_FC_SIMD_FACTOR][U1_DATA2_FC_SIMD_FACTOR];
  #if U1_DataCollect2Engine0_MEM == 0
    #pragma HLS bind_storage variable=ping_buffer type=RAM_T2P impl=BRAM
    #pragma HLS bind_storage variable=pong_buffer type=RAM_T2P impl=BRAM
  #elif U1_DataCollect2Engine0_MEM == 1
    #pragma HLS bind_storage variable=ping_buffer type=RAM_T2P impl=URAM
    #pragma HLS bind_storage variable=pong_buffer type=RAM_T2P impl=URAM
  #endif
#pragma HLS ARRAY_PARTITION variable=ping_buffer dim=3 complete
#pragma HLS ARRAY_PARTITION variable=pong_buffer dim=3 complete
#pragma HLS DATA_PACK variable=ping_buffer
#pragma HLS DATA_PACK variable=pong_buffer

  unsigned int initial_round = 0;
  bool done = 0;
  ap_uint<3> layer_iter = 0;
  bool layer_start = 0;
  while(!done){
    if (layer_start){
      // read in configurations
      LAYER_IN_NUM_T = fifo_config_in0.read();
      LAYER_OUT_NUM_T = fifo_config_in0.read();
      LAYER_IN_IMG_H_T = fifo_config_in0.read();
      LAYER_IN_IMG_W_T = fifo_config_in0.read();
      LAYER_FILTER_S_H = fifo_config_in0.read();
      LAYER_FILTER_S_W = fifo_config_in0.read();
      LAYER_TASK_NUM1 = fifo_config_in0.read();
      LAYER_TASK_NUM2 = fifo_config_in0.read();
      LAYER_LOCAL_ACCUM_NUM = fifo_config_in0.read();
      LAYER_LOCAL_REG_NUM = fifo_config_in0.read();
      LAYER_ROW_IL_FACTOR = fifo_config_in0.read();
      LAYER_COL_IL_FACTOR = fifo_config_in0.read();
      LAYER_STRIDE = fifo_config_in0.read();
      LAYER_BATCH = fifo_config_in0.read();

      LAYER_CONV_TYPE = fifo_config_in0.read();
      FILTER_D0_H = fifo_config_in0.read();
      FILTER_D0_W = fifo_config_in0.read();
      FILTER_D1_H = fifo_config_in0.read();
      FILTER_D1_W = fifo_config_in0.read();
      LAYER_DILATION_RATE = fifo_config_in0.read();
      LAYER_TCONV_STRIDE = fifo_config_in0.read();
      K_NUM = fifo_config_in0.read();
      KH = fifo_config_in0.read();
      KW = fifo_config_in0.read();
      // dummpy read
      LAYER_IN_NUM_T = fifo_config_in1.read();
      LAYER_OUT_NUM_T = fifo_config_in1.read();
      LAYER_IN_IMG_H_T = fifo_config_in1.read();
      LAYER_IN_IMG_W_T = fifo_config_in1.read();
      LAYER_FILTER_S_H = fifo_config_in1.read();
      LAYER_FILTER_S_W = fifo_config_in1.read();
      LAYER_TASK_NUM1 = fifo_config_in1.read();
      LAYER_TASK_NUM2 = fifo_config_in1.read();
      LAYER_LOCAL_ACCUM_NUM = fifo_config_in1.read();
      LAYER_LOCAL_REG_NUM = fifo_config_in1.read();
      LAYER_ROW_IL_FACTOR = fifo_config_in1.read();
      LAYER_COL_IL_FACTOR = fifo_config_in1.read();
      LAYER_STRIDE = fifo_config_in1.read();
      LAYER_BATCH = fifo_config_in1.read();

      LAYER_CONV_TYPE = fifo_config_in1.read();
      FILTER_D0_H = fifo_config_in1.read();
      FILTER_D0_W = fifo_config_in1.read();
      FILTER_D1_H = fifo_config_in1.read();
      FILTER_D1_W = fifo_config_in1.read();
      LAYER_DILATION_RATE = fifo_config_in1.read();
      LAYER_TCONV_STRIDE = fifo_config_in1.read();
      K_NUM = fifo_config_in1.read();
      KH = fifo_config_in1.read();
      KW = fifo_config_in1.read();
      // write out configurations
      fifo_config_out.write(LAYER_IN_NUM_T);
      fifo_config_out.write(LAYER_OUT_NUM_T);
      fifo_config_out.write(LAYER_IN_IMG_H_T);
      fifo_config_out.write(LAYER_IN_IMG_W_T);
      fifo_config_out.write(LAYER_FILTER_S_H);
      fifo_config_out.write(LAYER_FILTER_S_W);
      fifo_config_out.write(LAYER_TASK_NUM1);
      fifo_config_out.write(LAYER_TASK_NUM2);
      fifo_config_out.write(LAYER_LOCAL_ACCUM_NUM);
      fifo_config_out.write(LAYER_LOCAL_REG_NUM);
      fifo_config_out.write(LAYER_ROW_IL_FACTOR);
      fifo_config_out.write(LAYER_COL_IL_FACTOR);
      fifo_config_out.write(LAYER_STRIDE);
      fifo_config_out.write(LAYER_BATCH);

      fifo_config_out.write(LAYER_CONV_TYPE);
      fifo_config_out.write(FILTER_D0_H);
      fifo_config_out.write(FILTER_D0_W);
      fifo_config_out.write(FILTER_D1_H);	
      fifo_config_out.write(FILTER_D1_W);	
      fifo_config_out.write(LAYER_DILATION_RATE);
      fifo_config_out.write(LAYER_TCONV_STRIDE);
      fifo_config_out.write(K_NUM);
      fifo_config_out.write(KH);
      fifo_config_out.write(KW);
      layer_start = 0;
    }

    if (initial_round == 0){
      U1_Data2ReadData0(
        ping_buffer,
        fifo_collect_0,
        LAYER_IN_IMG_H_T,
        LAYER_ROW_IL_FACTOR,
        LAYER_COL_IL_FACTOR,
        LAYER_STRIDE,
        LAYER_TCONV_STRIDE
      );
    } else {
      if (initial_round % 2 == 1){
        U1_Data2ReadData0(
          pong_buffer,
          fifo_collect_0,
          LAYER_IN_IMG_H_T,
          LAYER_ROW_IL_FACTOR,
          LAYER_COL_IL_FACTOR,
          LAYER_STRIDE,
          LAYER_TCONV_STRIDE
        );
        U1_Data2WriteData0(ping_buffer, fifo_transfer_in, fifo_transfer_out, engine_id, LAYER_OUT_NUM_T_prev, LAYER_IN_IMG_H_T_prev, LAYER_IN_IMG_W_T_prev, LAYER_COL_IL_FACTOR_prev, LAYER_STRIDE_prev, LAYER_TCONV_STRIDE_prev);
      } else {
        U1_Data2ReadData0(
          ping_buffer,
          fifo_collect_0,
          LAYER_IN_IMG_H_T,
          LAYER_ROW_IL_FACTOR,
          LAYER_COL_IL_FACTOR,
          LAYER_STRIDE,
          LAYER_TCONV_STRIDE
        );
        U1_Data2WriteData0(pong_buffer, fifo_transfer_in, fifo_transfer_out, engine_id, LAYER_OUT_NUM_T_prev, LAYER_IN_IMG_H_T_prev, LAYER_IN_IMG_W_T_prev, LAYER_COL_IL_FACTOR_prev, LAYER_STRIDE_prev, LAYER_TCONV_STRIDE_prev);
      }
    }
    initial_round++;
    LAYER_OUT_NUM_T_prev = LAYER_OUT_NUM_T;
    LAYER_IN_IMG_H_T_prev = LAYER_IN_IMG_H_T;
    LAYER_IN_IMG_W_T_prev = LAYER_IN_IMG_W_T;
    LAYER_COL_IL_FACTOR_prev = LAYER_COL_IL_FACTOR;
    LAYER_STRIDE_prev = LAYER_STRIDE;
    LAYER_TCONV_STRIDE_prev = LAYER_TCONV_STRIDE;

    task_iter += 1;
    if (task_iter == LAYER_TASK_NUM2){
      task_iter = 0;
      layer_iter += 1;
      layer_start = 1;
      if (layer_iter == LAYER_BATCH){
        layer_iter = 0;
        done = 1;
      }
    }
  }

  if (initial_round % 2 == 1){
    U1_Data2WriteData0(ping_buffer, fifo_transfer_in, fifo_transfer_out, engine_id, LAYER_OUT_NUM_T_prev, LAYER_IN_IMG_H_T_prev, LAYER_IN_IMG_W_T_prev, LAYER_COL_IL_FACTOR_prev, LAYER_STRIDE_prev, LAYER_TCONV_STRIDE_prev);
  } else {
    U1_Data2WriteData0(pong_buffer, fifo_transfer_in, fifo_transfer_out, engine_id, LAYER_OUT_NUM_T_prev, LAYER_IN_IMG_H_T_prev, LAYER_IN_IMG_W_T_prev, LAYER_COL_IL_FACTOR_prev, LAYER_STRIDE_prev, LAYER_TCONV_STRIDE_prev);
  }
}

void U1_DataCollect2Engine0_wrapper(
  stream<U1_Data2TransferChannelType> &fifo_transfer_in,
  stream<U1_Data2TransferChannelType> &fifo_transfer_out,
  stream<U1_Data2PEChannelType> &fifo_collect_0,
  unsigned int engine_id,
  stream<uint> &fifo_config_in0,
  stream<uint> &fifo_config_in1,
  stream<uint> &fifo_config_out
){
  U1_DataCollect2Engine0(
    fifo_transfer_in,
    fifo_transfer_out,
    fifo_collect_0,
    engine_id,
    fifo_config_in0,
    fifo_config_in1,
    fifo_config_out
  );
}

void U1_DataCollect2EngineLast(
  stream<U1_Data2TransferChannelType> &fifo_transfer_out,
  stream<U1_Data2PEChannelType> &fifo_collect_0,
  unsigned int engine_id,
  stream<uint> &fifo_config_in0,
  stream<uint> &fifo_config_out
){
#pragma HLS DATA_PACK variable=fifo_transfer_out
#pragma HLS DATA_PACK variable=fifo_collect_0
#pragma HLS INLINE off

  uint LAYER_OUT_NUM_T_prev;
  uint LAYER_IN_IMG_H_T_prev;
  uint LAYER_IN_IMG_W_T_prev;
  uint LAYER_COL_IL_FACTOR_prev;
  uint LAYER_STRIDE_prev;
  uint LAYER_TCONV_STRIDE_prev;
  uint task_iter = 0;
  // read in configurations
  uint LAYER_IN_NUM_T = fifo_config_in0.read();
  uint LAYER_OUT_NUM_T = fifo_config_in0.read();
  uint LAYER_IN_IMG_H_T = fifo_config_in0.read();
  uint LAYER_IN_IMG_W_T = fifo_config_in0.read();
  uint LAYER_FILTER_S_H = fifo_config_in0.read();
  uint LAYER_FILTER_S_W = fifo_config_in0.read();
  uint LAYER_TASK_NUM1 = fifo_config_in0.read();
  uint LAYER_TASK_NUM2 = fifo_config_in0.read();
  uint LAYER_LOCAL_ACCUM_NUM = fifo_config_in0.read();
  uint LAYER_LOCAL_REG_NUM = fifo_config_in0.read();
  uint LAYER_ROW_IL_FACTOR = fifo_config_in0.read();
  uint LAYER_COL_IL_FACTOR = fifo_config_in0.read();
  uint LAYER_STRIDE = fifo_config_in0.read();
  uint LAYER_BATCH = fifo_config_in0.read();

  uint LAYER_CONV_TYPE = fifo_config_in0.read();
  uint FILTER_D0_H = fifo_config_in0.read();
  uint FILTER_D0_W = fifo_config_in0.read();
  uint FILTER_D1_H = fifo_config_in0.read();
  uint FILTER_D1_W = fifo_config_in0.read();
  uint LAYER_DILATION_RATE = fifo_config_in0.read();
  uint LAYER_TCONV_STRIDE = fifo_config_in0.read();
  uint K_NUM = fifo_config_in0.read();
  ap_uint<32> KH = fifo_config_in0.read();
  ap_uint<32> KW = fifo_config_in0.read();
  // write out configurations
  fifo_config_out.write(LAYER_IN_NUM_T);
  fifo_config_out.write(LAYER_OUT_NUM_T);
  fifo_config_out.write(LAYER_IN_IMG_H_T);
  fifo_config_out.write(LAYER_IN_IMG_W_T);
  fifo_config_out.write(LAYER_FILTER_S_H);
  fifo_config_out.write(LAYER_FILTER_S_W);
  fifo_config_out.write(LAYER_TASK_NUM1);
  fifo_config_out.write(LAYER_TASK_NUM2);
  fifo_config_out.write(LAYER_LOCAL_ACCUM_NUM);
  fifo_config_out.write(LAYER_LOCAL_REG_NUM);
  fifo_config_out.write(LAYER_ROW_IL_FACTOR);
  fifo_config_out.write(LAYER_COL_IL_FACTOR);
  fifo_config_out.write(LAYER_STRIDE);
  fifo_config_out.write(LAYER_BATCH);

  fifo_config_out.write(LAYER_CONV_TYPE);
  fifo_config_out.write(FILTER_D0_H);
  fifo_config_out.write(FILTER_D0_W);
  fifo_config_out.write(FILTER_D1_H);	
  fifo_config_out.write(FILTER_D1_W);	
  fifo_config_out.write(LAYER_DILATION_RATE);
  fifo_config_out.write(LAYER_TCONV_STRIDE);
  fifo_config_out.write(K_NUM);
  fifo_config_out.write(KH);
  fifo_config_out.write(KW);
  U1_data_t2 ping_buffer[U1_DATA2_FC_GROUP_FACTOR][U1_DATA2_BUF_SIZE / U1_DATA2_FC_SIMD_FACTOR][U1_DATA2_FC_SIMD_FACTOR];
  U1_data_t2 pong_buffer[U1_DATA2_FC_GROUP_FACTOR][U1_DATA2_BUF_SIZE / U1_DATA2_FC_SIMD_FACTOR][U1_DATA2_FC_SIMD_FACTOR];
  #if U1_DataCollect2EngineLast_MEM == 0
    #pragma HLS bind_storage variable=ping_buffer type=RAM_T2P impl=BRAM
    #pragma HLS bind_storage variable=pong_buffer type=RAM_T2P impl=BRAM
  #elif U1_DataCollect2EngineLast_MEM == 1
    #pragma HLS bind_storage variable=ping_buffer type=RAM_T2P impl=URAM
    #pragma HLS bind_storage variable=pong_buffer type=RAM_T2P impl=URAM
  #endif
#pragma HLS ARRAY_PARTITION variable=ping_buffer dim=3 complete
#pragma HLS ARRAY_PARTITION variable=pong_buffer dim=3 complete
#pragma HLS DATA_PACK variable=ping_buffer
#pragma HLS DATA_PACK variable=pong_buffer

  unsigned int initial_round = 0;
  bool done = 0;
  ap_uint<3> layer_iter = 0;
  bool layer_start = 0;
  while(!done){
    if (layer_start){
      // read in configurations
      LAYER_IN_NUM_T = fifo_config_in0.read();
      LAYER_OUT_NUM_T = fifo_config_in0.read();
      LAYER_IN_IMG_H_T = fifo_config_in0.read();
      LAYER_IN_IMG_W_T = fifo_config_in0.read();
      LAYER_FILTER_S_H = fifo_config_in0.read();
      LAYER_FILTER_S_W = fifo_config_in0.read();
      LAYER_TASK_NUM1 = fifo_config_in0.read();
      LAYER_TASK_NUM2 = fifo_config_in0.read();
      LAYER_LOCAL_ACCUM_NUM = fifo_config_in0.read();
      LAYER_LOCAL_REG_NUM = fifo_config_in0.read();
      LAYER_ROW_IL_FACTOR = fifo_config_in0.read();
      LAYER_COL_IL_FACTOR = fifo_config_in0.read();
      LAYER_STRIDE = fifo_config_in0.read();
      LAYER_BATCH = fifo_config_in0.read();

      LAYER_CONV_TYPE = fifo_config_in0.read();
      FILTER_D0_H = fifo_config_in0.read();
      FILTER_D0_W = fifo_config_in0.read();
      FILTER_D1_H = fifo_config_in0.read();
      FILTER_D1_W = fifo_config_in0.read();
      LAYER_DILATION_RATE = fifo_config_in0.read();
      LAYER_TCONV_STRIDE = fifo_config_in0.read();
      K_NUM = fifo_config_in0.read();
      KH = fifo_config_in0.read();
      KW = fifo_config_in0.read();
      // write out configurations
      fifo_config_out.write(LAYER_IN_NUM_T);
      fifo_config_out.write(LAYER_OUT_NUM_T);
      fifo_config_out.write(LAYER_IN_IMG_H_T);
      fifo_config_out.write(LAYER_IN_IMG_W_T);
      fifo_config_out.write(LAYER_FILTER_S_H);
      fifo_config_out.write(LAYER_FILTER_S_W);
      fifo_config_out.write(LAYER_TASK_NUM1);
      fifo_config_out.write(LAYER_TASK_NUM2);
      fifo_config_out.write(LAYER_LOCAL_ACCUM_NUM);
      fifo_config_out.write(LAYER_LOCAL_REG_NUM);
      fifo_config_out.write(LAYER_ROW_IL_FACTOR);
      fifo_config_out.write(LAYER_COL_IL_FACTOR);
      fifo_config_out.write(LAYER_STRIDE);
      fifo_config_out.write(LAYER_BATCH);

      fifo_config_out.write(LAYER_CONV_TYPE);
      fifo_config_out.write(FILTER_D0_H);
      fifo_config_out.write(FILTER_D0_W);
      fifo_config_out.write(FILTER_D1_H);	
      fifo_config_out.write(FILTER_D1_W);	
      fifo_config_out.write(LAYER_DILATION_RATE);
      fifo_config_out.write(LAYER_TCONV_STRIDE);
      fifo_config_out.write(K_NUM);
      fifo_config_out.write(KH);
      fifo_config_out.write(KW);
      layer_start = 0;
    }

    if (initial_round == 0){
      U1_Data2ReadData0(
        ping_buffer,
        fifo_collect_0,
        LAYER_IN_IMG_H_T,
        LAYER_ROW_IL_FACTOR,
        LAYER_COL_IL_FACTOR,
        LAYER_STRIDE,
        LAYER_TCONV_STRIDE
      );
    } else {
      if (initial_round % 2 == 1){
        U1_Data2ReadData0(
          pong_buffer,
          fifo_collect_0,
          LAYER_IN_IMG_H_T,
          LAYER_ROW_IL_FACTOR,
          LAYER_COL_IL_FACTOR,
          LAYER_STRIDE,
          LAYER_TCONV_STRIDE
        );
        U1_Data2WriteDataLast(ping_buffer, fifo_transfer_out, engine_id, LAYER_OUT_NUM_T_prev, LAYER_IN_IMG_H_T_prev, LAYER_IN_IMG_W_T_prev, LAYER_COL_IL_FACTOR_prev, LAYER_STRIDE_prev, LAYER_TCONV_STRIDE_prev);
      } else {
        U1_Data2ReadData0(
          ping_buffer,
          fifo_collect_0,
          LAYER_IN_IMG_H_T,
          LAYER_ROW_IL_FACTOR,
          LAYER_COL_IL_FACTOR,
          LAYER_STRIDE,
          LAYER_TCONV_STRIDE
        );
        U1_Data2WriteDataLast(pong_buffer, fifo_transfer_out, engine_id, LAYER_OUT_NUM_T_prev, LAYER_IN_IMG_H_T_prev, LAYER_IN_IMG_W_T_prev, LAYER_COL_IL_FACTOR_prev, LAYER_STRIDE_prev, LAYER_TCONV_STRIDE_prev);
      }
    }
    initial_round++;
    LAYER_OUT_NUM_T_prev = LAYER_OUT_NUM_T;
    LAYER_IN_IMG_H_T_prev = LAYER_IN_IMG_H_T;
    LAYER_IN_IMG_W_T_prev = LAYER_IN_IMG_W_T;
    LAYER_COL_IL_FACTOR_prev = LAYER_COL_IL_FACTOR;
    LAYER_STRIDE_prev = LAYER_STRIDE;
    LAYER_TCONV_STRIDE_prev = LAYER_TCONV_STRIDE;
    task_iter += 1;
    if (task_iter == LAYER_TASK_NUM2){
      task_iter = 0;
      layer_iter += 1;
      layer_start = 1;
      if (layer_iter == LAYER_BATCH){
        layer_iter = 0;
        done = 1;
      }
    }
  }

  if (initial_round % 2 == 1){
    U1_Data2WriteDataLast(ping_buffer, fifo_transfer_out, engine_id, LAYER_OUT_NUM_T_prev, LAYER_IN_IMG_H_T_prev, LAYER_IN_IMG_W_T_prev, LAYER_COL_IL_FACTOR_prev, LAYER_STRIDE_prev, LAYER_TCONV_STRIDE_prev);
  } else {
    U1_Data2WriteDataLast(pong_buffer, fifo_transfer_out, engine_id, LAYER_OUT_NUM_T_prev, LAYER_IN_IMG_H_T_prev, LAYER_IN_IMG_W_T_prev, LAYER_COL_IL_FACTOR_prev, LAYER_STRIDE_prev, LAYER_TCONV_STRIDE_prev);
  }
}

