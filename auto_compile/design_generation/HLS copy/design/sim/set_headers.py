import sys
import os
from pathlib import Path

def setHeaders(prj_path, start_layer, end_layer, layer_id_test, target_module, sl_option):
  # '/data/instructions.dat'
  inst_path = prj_path / 'data' / 'instructions.dat'
  instFile = open(inst_path, 'r')
  lines = instFile.readlines()
  insts = []
  for i in range(start_layer-1,end_layer):
    inst = []
    for line in range(0,6):
      instLine = lines[i*7+line].split()
      for num in instLine:
        inst.append(int(num))
    insts.append(inst)


  instDicts = []

  for inst in insts:
    instDict = {}
    instDict['IN_NUM_HW'	      ] = inst[0 ]
    instDict['OUT_NUM_HW'	      ] = inst[1 ]
    instDict['IN_H_HW'		      ] = inst[2 ]
    instDict['IN_W_HW'		      ] = inst[3 ]
    instDict['OUT_H_NP'         ] = inst[4 ]
    instDict['OUT_H_SP'         ] = inst[5 ]
    instDict['OUT_W_EP'         ] = inst[6 ]
    instDict['OUT_W_WP'         ] = inst[7 ]
    instDict['IN_NUM'			      ] = inst[8 ]
    instDict['OUT_NUM'		      ] = inst[9 ]
    instDict['IN_H'				      ] = inst[10]
    instDict['IN_W'				      ] = inst[11]
    instDict['OUT_H'			      ] = inst[12]
    instDict['OUT_W'			      ] = inst[13]
    instDict['CIN_OFFSET'	      ] = inst[14]
    instDict['WEIGHT_OFFSET'  	] = inst[15]
    instDict['BIAS_OFFSET'		  ] = inst[16]
    instDict['COUT_OFFSET'		  ] = inst[17]
    instDict['FILTER_S1'	    	] = inst[18]
    instDict['FILTER_S2_H'		  ] = inst[19]
    instDict['FILTER_S2_W'		  ] = inst[20]
    instDict['STRIDE'			      ] = inst[21]
    instDict['EN'				        ] = inst[22]
    instDict['PREV_CIN_OFFSET'	] = inst[23]
    instDict['IN_NUM_T'			    ] = inst[24]
    instDict['OUT_NUM_T'		    ] = inst[25]
    instDict['IN_H_T'			      ] = inst[26]
    instDict['IN_W_T'			      ] = inst[27]
    instDict['BATCH_NUM'		    ] = inst[28]
    instDict['TASK_NUM1'		    ] = inst[29]
    instDict['TASK_NUM2'		    ] = inst[30]
    instDict['LOCAL_ACCUM_NUM'	] = inst[31]
    instDict['LOCAL_REG_NUM'	  ] = inst[32]
    instDict['ROW_IL_FACTOR'	  ] = inst[33]
    instDict['COL_IL_FACTOR'	  ] = inst[34]
    instDict['CONV_TYPE'		    ] = inst[35]
    instDict['FILTER_D0_H'	    ] = inst[36]
    instDict['FILTER_D0_W'	    ] = inst[37]
    instDict['FILTER_D1_H'	    ] = inst[38]
    instDict['FILTER_D1_W'	    ] = inst[39]
    instDict['DILATION_RATE'	  ] = inst[40]
    instDict['TCONV_STRIDE'		  ] = inst[41]
    instDict['K_NUM'			      ] = inst[42]
    instDict['KH_KW'			      ] = inst[43]
    instDict['OUT_H_HW'] = instDict['OUT_H'] + instDict['OUT_H_NP'] + instDict['OUT_H_SP']
    instDict['OUT_W_HW'] = instDict['OUT_W'] + instDict['OUT_W_EP'] + instDict['OUT_W_WP']
    instDicts.append(instDict)


  first_inst = instDicts[0]
  last_inst = instDicts[-1]



#define START_LAYER 1
#define END_LAYER 1
  debugFile = open(prj_path / 'src' / 'debug.h', "w")
  print("#define DEBUG_"+target_module, file=debugFile)
  print("#define TARGET_INST "+str(layer_id_test), file=debugFile)
  print("#define SIMULATION", file=debugFile)
  #define DEBUG_engine
  #define SA_DEBUG
  # print("#define DEBUG_engine", file=debugFile)
  # print("#define SA_DEBUG", file=debugFile)

  inFile = open(prj_path / 'src' / 'cnn_sw.h', "r")
  outFile = open(prj_path / 'src' / 'temp.h', "w")
  for line in inFile:
      values = line.split()
      if(len(values)!=0):
        if(values[0]=="#define" and values[1]=="LAYER_NUM"):
          outFile.writelines("#define LAYER_NUM "+str(end_layer-start_layer+1)+"\n")
        elif(values[0]=="#define" and values[1]=="PRJ_PATH"):
          outFile.writelines("#define PRJ_PATH \""+str(prj_path.as_posix())+"\"\n")
        elif(values[0]=="#define" and values[1]=="STRIDE"):
          outFile.writelines("#define STRIDE "+str(instDict['STRIDE'])+"\n")
        elif(values[0]=="#define" and values[1]=="START_LAYER"):
          outFile.writelines("#define START_LAYER "+str(start_layer)+"\n")
        elif(values[0]=="#define" and values[1]=="END_LAYER"):
          outFile.writelines("#define END_LAYER "+str(end_layer)+"\n")
        elif(values[0]=="#define" and values[1]=="LAYER_ID_TEST"):
          outFile.writelines("#define LAYER_ID_TEST "+str(layer_id_test)+"\n")
        elif(values[0]=="#define" and values[1]=="SL_OPTION"):
          outFile.writelines("#define SL_OPTION "+str(sl_option)+"\n")
        elif(values[0]=="#define" and values[1]=="CIN_OFFSET"):
          outFile.writelines("#define CIN_OFFSET "+str(instDict['CIN_OFFSET'])+"\n")
        elif(values[0]=="#define" and values[1]=="FILTER_S2_H"):
          outFile.writelines("#define FILTER_S2_H "+str(instDict['FILTER_S2_H'])+"\n")
        elif(values[0]=="#define" and values[1]=="FILTER_S2_W"):
          outFile.writelines("#define FILTER_S2_W "+str(instDict['FILTER_S2_W'])+"\n")
        elif(values[0]=="#define" and values[1]=="OUTFILE"):
          outFile.writelines("#define OUTFILE \"/data/outputs/L"+str(end_layer)+"_outputs.dat\"\n")
        elif(values[0]=="#define" and values[1]=="OUT_OFFSET1"):
          h_padding = instDict['OUT_H_NP']
          w_padding = instDict['OUT_W_EP']
          offset = instDict['COUT_OFFSET']-((h_padding*instDict['OUT_W_HW']+w_padding)*instDict['OUT_NUM_T'])
          outFile.writelines("#define OUT_OFFSET1 "+str(int(offset))+"\n")
        elif(values[0]=="#define" and values[1]=="OUT_OFFSET2"):
          outFile.writelines("#define OUT_OFFSET2 "+str(instDict['COUT_OFFSET'])+"\n")
        elif(values[0]=="#define" and values[1]=="CHANGE_LAYOUT"):
          if(((instDict['OUT_W_HW'] == instDict['OUT_W']) or (instDict['OUT_W_HW'] == instDict['IN_W_T'])) and ((instDict['OUT_H_HW'] == instDict['OUT_H']) or (instDict['OUT_H_HW'] == instDict['IN_H_T']))):
            outFile.writelines("#define CHANGE_LAYOUT "+str(1)+"\n")
          else:
            outFile.writelines("#define CHANGE_LAYOUT "+str(0)+"\n")
        #these may cause issues be aware of them
        elif(values[0]=="#define" and values[1]=="OUT_H_T"):
          outFile.writelines("#define OUT_H_T "+str(instDict['IN_H_T']*instDict['TCONV_STRIDE'])+"\n")
        elif(values[0]=="#define" and values[1]=="OUT_W_T"):
          outFile.writelines("#define OUT_W_T "+str(instDict['IN_W_T']*instDict['TCONV_STRIDE'])+"\n")
        elif(values[0]=="#define" and values[1]=="OUT_H_NP"):
          outFile.writelines("#define OUT_H_NP "+str(instDict['OUT_H_NP'])+"\n")
        elif(values[0]=="#define" and values[1]=="OUT_H_SP"):
          outFile.writelines("#define OUT_H_SP "+str(instDict['OUT_H_SP'])+"\n")
        elif(values[0]=="#define" and values[1]=="OUT_W_EP"):
          outFile.writelines("#define OUT_W_EP "+str(instDict['OUT_W_EP'])+"\n")
        elif(values[0]=="#define" and values[1]=="OUT_W_WP"):
          outFile.writelines("#define OUT_W_WP "+str(instDict['OUT_W_WP'])+"\n")          
        elif(values[0]=="#define"):
          for key in instDicts[0]:
            if(values[1]==key):
              outFile.writelines(values[0] + " " + values[1] + " " + str(instDict[values[1]])+"\n")
        else:
          outFile.writelines(line)
  outFile.close()
  inFile.close()
  inFile = open(prj_path / 'src' / 'temp.h', "r")
  outFile = open(prj_path / 'src' / 'cnn_sw.h', "w")
  for line in inFile:
    outFile.writelines(line)
  outFile.close()
  inFile.close()
  os.remove(prj_path / 'src' / 'temp.h')