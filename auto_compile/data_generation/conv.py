## Imports
from ast import walk
import os
import sys
import random
import math

import numpy as np
import matplotlib.pyplot as plt

import tensorflow as tf
from tensorflow import keras
import tensorflow as tf
from tensorflow.keras.activations import relu
from tensorflow.keras.initializers import GlorotUniform, GlorotNormal
from tensorflow.keras.layers import Layer, Conv2D, Softmax, BatchNormalization, \
		LayerNormalization, ReLU, Lambda, Conv2DTranspose, MaxPooling2D, concatenate, add
# from tensorflow_addons.layers import InstanceNormalization
from tensorflow.keras import Model

seed = 2019
# random.seed = seed
random.seed(2019)
# np.random.seed = seed
# tf.seed = seed


def TCONV(IN_NUM, OUT_NUM, IN_H, IN_W, K, S):

	inputs = keras.layers.Input((IN_H, IN_W, IN_NUM))
	tconv = Conv2DTranspose(OUT_NUM, K, strides=S, use_bias=False,
																			dtype=tf.float32, padding='valid', data_format='channels_last',
																			kernel_initializer=GlorotNormal(), bias_initializer=tf.initializers.Constant(0))(inputs)
	model = keras.models.Model(inputs, tconv)
	return model

def DCONV(IN_NUM, OUT_NUM, IN_H, IN_W, K, dilation_rate):
  inputs = keras.layers.Input((IN_H, IN_W, IN_NUM))
  dconv = Conv2D(OUT_NUM, K, dilation_rate=dilation_rate, use_bias=False, 
                  dtype=tf.float32, padding='same', kernel_initializer=GlorotNormal())(inputs)
  model = keras.models.Model(inputs, dconv)
  return model

def NCONV(IN_NUM, OUT_NUM, IN_H, IN_W, K):
  inputs = keras.layers.Input((IN_H, IN_W, IN_NUM))
  conv = Conv2D(OUT_NUM, K, use_bias=False, 
                  dtype=tf.float32, padding='same', kernel_initializer=GlorotNormal())(inputs)
  model = keras.models.Model(inputs, conv)
  return model

def reorder(arr, in_num, out_num, K, order=[8, 6, 7, 2, 0, 1, 5, 3, 4]):
	arr = np.moveaxis(arr, [1, 2, 3], [2, 3, 1])
	arr = arr.reshape((in_num*out_num, K*K))
	arr = np.array([[arr[j][i] for i in order] for j in range(len(arr))])
	arr = arr.reshape((out_num, in_num, K, K))
	return np.moveaxis(arr, [1, 2, 3], [3, 1, 2])       


convType = 'TCONV'
IN_NUM = 16
OUT_NUM = 3
IN_H = 36
IN_W = 36
K = 2
TS = 2
dilation_rate = 1
if(convType=='NCONV'):
  model = NCONV(IN_NUM, OUT_NUM, IN_H, IN_W, K)
elif(convType=='DCONV'):
  model = DCONV(IN_NUM, OUT_NUM, IN_H, IN_W, K, dilation_rate)
elif(convType=='TCONV'):
  model = TCONV(IN_NUM, OUT_NUM, IN_H, IN_W, K, TS)

model.compile(optimizer="adam", loss="binary_crossentropy", metrics=["acc"])
model.summary()
layer = model.get_layer(index=1)

ws = layer.get_weights()
for o in range(OUT_NUM):
  for i in range(IN_NUM):
    for k_h in range(K):
      for k_w in range(K):
        ws[0][k_h][k_w][o][i] = random.random()-0.5

inputs = np.ones((1,IN_H,IN_W,IN_NUM))
for i in range(IN_NUM):
  for h in range(IN_H):
    for w in range(IN_W):
      inputs[0][h][w][i] = random.random()-0.5
# for o in range(OUT_NUM):
# 	w[1][o] = 0
# w[1].tofile(prj_path+"/data/biases.dat", sep="\n", format="%s")

if(TS==2):
  if(K==2):
    order = [0,1,2,3]
  elif(K==3):
    order = [8,6,7,2,0,1,5,3,4]
  elif(K==4):
    order = [10,8,11,9,2,0,3,1,14,12,15,13,6,4,7,5]
  elif(K==5):
    order = [24,22,20,23,21,14,12,10,13,11,4,2,0,3,1,19,17,15,18,16,9,7,5,8,6]
elif(TS==3):
  if(K==3):
    order = [0,1,2,3,4,5,6,7,8]
  elif(K==4):
    order = [15,12,13,14,3,0,1,2,7,4,5,6,11,8,9,10]
  elif(K==5):
    order = [18,15,17,19,16,3,0,2,4,1,13,10,12,14,11,23,20,22,24,21,8,5,7,9,6]
elif(TS==4):
  if(K==4):
    order = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]

weights = ws[0]
weights = np.swapaxes(weights,1,2)
weights = np.swapaxes(weights,0,1)
if(convType=='NCONV' or convType=='DCONV'): 
  weights = np.swapaxes(weights,0,3)
if(convType=='TCONV'):
  print(weights.shape)
  print(IN_NUM, OUT_NUM, K, TS)
  weights = reorder(weights, IN_NUM, OUT_NUM, K, order)
  print(weights.shape)
in_num = IN_NUM
out_num = OUT_NUM
filter_s = K
in_num_t = 16
out_num_t = 8
weights_reorg = np.zeros((int(math.ceil(float(out_num) / out_num_t)), int(math.ceil(float(in_num) / in_num_t)), out_num_t, filter_s, filter_s, in_num_t))
for o1 in range(int(math.ceil(float(out_num) / out_num_t))):
    for i1 in range(int(math.ceil(float(in_num) / in_num_t))):
        for o2 in range(out_num_t):
            for p in range(filter_s):
                for q in range(filter_s):
                    for i2 in range(in_num_t):
                        L2 = o1*out_num_t+o2
                        L1 = i1*in_num_t+i2
                        if (o1 * out_num_t + o2 < out_num) and (i1 * in_num_t + i2 < in_num):
                            weights_reorg[o1][i1][o2][p][q][i2] = weights[L2][p][q][L1]
                        else:
                            weights_reorg[o1][i1][o2][p][q][i2] = float(0.0)
# weights_reorg.tofile(prj_path+"/data/weights.dat", sep="\n", format="%s")

# layer.set_weights(ws)

# ouptuts = model.predict(inputs)
# inputs = np.swapaxes(inputs,2,3)
# inputs = np.swapaxes(inputs,1,2)

# inputs.tofile(prj_path+"/data/inputs.dat", sep="\n", format="%s")

# ouptuts = np.swapaxes(ouptuts,2,3)
# ouptuts = np.swapaxes(ouptuts,1,2)

# print('output shape before: ', ouptuts.shape)
# if(TS==2):
#   if(K-2>0):
#     ouptuts = ouptuts[:,:,:-(K-2),:-(K-2)]
#     print(ouptuts.shape)
# elif(TS==3):
#   if(K==4):
#     ouptuts = ouptuts[:,:,:-(K-3),:-(K-3)]
#   elif(K==5):
#     ouptuts = ouptuts[:,:,1:-(K-4),1:-(K-4)]
# print('output shape after: ', ouptuts.shape)
# ouptuts.tofile(prj_path+"/data/L1_outputs.dat", sep="\n", format="%s")

