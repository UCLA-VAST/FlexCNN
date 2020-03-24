import warnings
warnings.filterwarnings('ignore')
import numpy as np
import sys
import json
import code_template as tpl
import argparse
import desp_gen
import os

def generate_Loader(path, desp, config):
  code = []
  code.extend(tpl.disclaimer(desp, config))
  code.extend(tpl.header_include(desp, config))

  code.extend(tpl.loader(desp, config))

  with open(path, 'w') as f:
    for codeline in code:
      f.write(codeline)

def generate_DF(path, desp, config):
  code = []
  code.extend(tpl.disclaimer(desp, config))
  code.extend(tpl.header_include(desp, config))

  code.extend(tpl.df(desp, config))

  with open(path, 'w') as f:
    for codeline in code:
      f.write(codeline)

def generate_DC(path, desp, config):
  code = []
  code.extend(tpl.disclaimer(desp, config))
  code.extend(tpl.header_include(desp, config))

  code.extend(tpl.dc(desp, config))

  with open(path, 'w') as f:
    for codeline in code:
      f.write(codeline)

def generate_PE(path, desp, config):
  code = []
  code.extend(tpl.disclaimer(desp, config))
  code.extend(tpl.header_include(desp, config))

  code.extend(tpl.PE_MAC(desp, config))
  code.extend(tpl.op_transfer(desp, config))
  code.extend(tpl.compute(desp, config))
  code.extend(tpl.res_transfer(desp, config))
  code.extend(tpl.kernel(desp, config))

  with open(path, 'w') as f:
    for codeline in code:
      f.write(codeline)

def generate_tb(path, desp, config):
  code = []
  code.extend(tpl.disclaimer(desp, config))
  code.extend(tpl.header_include(desp, config))

  code.extend(tpl.tb(desp, config))

  with open(path, 'w') as f:
    for codeline in code:
      f.write(codeline)

def generate_header(path, desp, config):
  code = []
  code.extend(tpl.disclaimer(desp, config))

  code.extend(tpl.header(desp, config))

  with open(path, 'w') as f:
    for codeline in code:
      f.write(codeline)

def generate_top(path, desp, config):
  code = []
  code.extend(tpl.disclaimer(desp, config))
  code.extend(tpl.header_include(desp, config))

  code.extend(tpl.top(desp, config))

  with open(path, 'w') as f:
    for codeline in code:
      f.write(codeline)

def run(input_vsa, mode = 'CSIM'):
  config = {}
  config['MODE'] = mode

  pwd_dir  = os.path.dirname(os.path.realpath(__file__))
  if not os.path.exists(pwd_dir + '/output'):
    os.makedirs(pwd_dir + '/output')

  desp_gen.run(input_vsa)

  with open(pwd_dir + '/output/design_desp.json', 'r') as F:
    design_desp = json.loads(F.read())

  # problem-specific preprocessing
  # - MV
#  design_desp['ARRAY_SIZE'] = {}
#  design_desp['ARRAY_SIZE']['A'] = design_desp['PARAMETERS']['I'] * design_desp['PARAMETERS']['J']
#  design_desp['ARRAY_SIZE']['B'] = design_desp['PARAMETERS']['J']
#  design_desp['ARRAY_SIZE']['C'] = design_desp['PARAMETERS']['I']

  # tb_app.cpp
  generate_tb(pwd_dir + '/output/tb_app.cpp', design_desp, config)

  # top.cpp
  generate_top(pwd_dir + '/output/top.cpp', design_desp, config)

  # common_header.h
  generate_header(pwd_dir + '/output/common_header_U%s.h' %(design_desp['KERNEL_ID']), design_desp, config)

  # PE.cpp
  generate_PE(pwd_dir + '/output/2DPE_U%s.cpp' %(design_desp['KERNEL_ID']), design_desp, config)

  # 2DDataFeed.cpp
  generate_DF(pwd_dir + '/output/2DDataFeed_U%s.cpp' %(design_desp['KERNEL_ID']), design_desp, config)

  # 2DDataCollect.cpp
  generate_DC(pwd_dir + '/output/2DDataCollect_U%s.cpp' %(design_desp['KERNEL_ID']), design_desp, config)

  # 2DDataFeedCollect.cpp
  generate_Loader(pwd_dir + '/output/2DDataFeedCollect_U%s.cpp' %(design_desp['KERNEL_ID']), design_desp, config)

if __name__ == "__main__":

  parser = argparse.ArgumentParser(description='Generate HLS C code from VSA decriptors.')
  parser.add_argument('-i', '--input', metavar='INPUT', required=True, help='input virtual systolic array descriptor')
  parser.add_argument('-m', '--mode', metavar='MODE', required=False, help='code generation mode: CSIM, SYNTH', default='CSIM')

  args = parser.parse_args()

  run(args.input, args.mode)


