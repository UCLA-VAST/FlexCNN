import argparse
import logging
import sys
import time
import os
import os.path
import yappi

from tf_pose import common
import cv2
import numpy as np
from tf_pose.estimator import TfPoseEstimator, TfPoseEstimatorSacc, PoseEstimator
from tf_pose.networks import get_graph_path, model_wh
import unittest as ut # added by Tong
import logging
import math

from tf_pose import slidingwindow as sw

import cv2
import numpy as np
#import tensorflow as tf
import time

from tf_pose import common
from tf_pose.common import CocoPart
from tf_pose.tensblur.smoother import Smoother
from tensorflow.python.client import timeline

from multiprocessing import Queue
from threading import Thread
from multiprocessing import Process
from tf_pose import sacc_utils 


logger = logging.getLogger('TfPoseEstimatorRun')
logger.handlers.clear()
logger.setLevel(logging.DEBUG)
ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
formatter = logging.Formatter('[%(asctime)s] [%(name)s] [%(levelname)s] %(message)s')
ch.setFormatter(formatter)
logger.addHandler(ch)



class OpenPoseInfer(object):
     def __init__(self, args, device='CPU'):
        self.device = device
        self.args = args
        self.w, self.h = model_wh(args.resize)
        self.fps = 0
        self.num_run = 16.0
        self.pack_factor = 16
        self.total_frame = 0
        self.resize_to_default=(self.w > 0 and self.h > 0)
        self.target_size = (self.w, self.h)
        if device == 'CPU':
            if self.w == 0 or self.h == 0:
                self.e = TfPoseEstimator(get_graph_path(args.model), target_size=(432, 368))
            else:
                self.e = TfPoseEstimator(get_graph_path(args.model), target_size=(self.w, self.h))
        elif device == 'FPGA':
            self.out = None
            self.input_q = Queue()
            self.output_q = Queue()
            self.out_est = Queue()
            self.out_pack = Queue()
            self.input_write = Queue()
            self.info_accel = Queue()
            self.display = 0
            self.inference_num = 0
            self.info_in_s_accel = Queue()
            self.info_in_s_pack = Queue()
            self.info_in_s_est = Queue()
            self.info_in_s_disp = Queue()
            self.t = Process(target=self.worker_accel, args=(self.out_pack, self.output_q, self.info_accel, self.info_in_s_accel, self.info_in_s_est))
            self.t2 = Process(target=self.worker_disp, args=(self.out_est, self.info_in_s_disp))
            self.t3 = Process(target=self.worker_pack, args=(self.input_q, self.out_pack, self.info_in_s_pack, self.info_in_s_accel))
            self.t5 = Process(target=self.worker_est_draw, args=(self.output_q, self.out_est, self.info_in_s_est, self.info_in_s_disp))
            self.t4 = Process(target=self.worker_write, args=(self.input_write,))
            self.t3.start()
            self.t.start()
            self.t5.start()
            self.t2.start()
            #t4.start()
            

     def worker_pack(self, in_q, out_q, info_in_stop, info_out_stop):
           import tensorflow as tf
           packed_images = []
           packed = 0
           frames = 0
           num_frames = 1e10
           stop = False
           while True:
             #print("pack")
             if not info_in_stop.empty():
               stop = True
               num_frames = info_in_stop.get()
             if not in_q.empty():
               image = in_q.get()
               frames += 1
               if self.resize_to_default:
                 image = self._get_scaled_img(image, None)[0][0]
               packed_images.append(image)
               packed += 1
               #print(packed)
             if len(packed_images) == self.pack_factor:
               #print("sent")
               out_q.put(packed_images)
               packed_images = []
             if frames == num_frames:
               break
           if stop:
             info_out_stop.put(1)
             
             
     def worker_disp(self, q2, info_in_stop):
            import tensorflow as tf
            if self.args.display:
              cv2.namedWindow('tf-pose-estimation result', cv2.WINDOW_NORMAL)
              cv2.resizeWindow('tf-pose-estimation result', 384, 384)
            frames = 0
            frame_width = 384
            frame_height = 384
            self.out = cv2.VideoWriter('output_{}.avi'.format(self.args.device),cv2.VideoWriter_fourcc('M','J','P','G'), 10, (frame_width, frame_height))
            begin = time.time()
            start = begin
            stop = False
            while (not stop or not q2.empty()):
              if not info_in_stop.empty():
                stop = True
                info_in_stop.get()
              if self.args.display:
                   if not q2.empty():
                     image6 = q2.get()
                     frames += 1
                     
                     start = time.time()
                     cv2.imshow('tf-pose-estimation result', image6)
                     self.out.write(image6)
              if cv2.waitKey(1) == 27:
                   break 
              
     def worker_write(self, q2):
            import tensorflow as tf
            frame_width = 384
            frame_height = 384
            self.out = cv2.VideoWriter('output_{}.avi'.format(self.args.device),cv2.VideoWriter_fourcc('M','J','P','G'), 10, (frame_width, frame_height))
            while True:
              
              if not q2.empty():
                image6 = q2.get()
                self.out.write(image6)
              
     
     def worker_accel(self, input_q, output_q, info_out, info_in_stop, info_out_stop):
            accel_start = time.time()
            import tensorflow as tf
            self.target_size = (384, 384)
            target_size = self.target_size
            self.constants = sacc_utils.Constants() 
            core_config = tf.ConfigProto()
            core_config.gpu_options.allow_growth = True
            self.persistent_sess = tf.Session(config=core_config)
            self.tensor_image = tf.placeholder(tf.float32, shape=(1, 384, 384, 3)) 
            self.tensor_image2 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image3 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image4 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image5 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image6 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image7 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image8 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            
            self.tensor_image0 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3)) 
            self.tensor_image20 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image30 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image40 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image50 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image60 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image70 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.tensor_image80 = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
            self.sacc_module = tf.load_op_library(self.constants.custom_lib_path)
    
            result = self.sacc_module.sacc([self.tensor_image, self.tensor_image2, self.tensor_image3, self.tensor_image4, self.tensor_image5, self.tensor_image6, self.tensor_image7, self.tensor_image8, self.tensor_image0, self.tensor_image20, self.tensor_image30, self.tensor_image40, self.tensor_image50, self.tensor_image60, self.tensor_image70, self.tensor_image80])
            self.sacc_result = result[0]
            
            self.tensor_heatMat = self.sacc_result[:, :, :, :19]
            self.tensor_pafMat = self.sacc_result[:, :, :, 19:]
    
            self.upsample_size = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size')
            self.tensor_heatMat_up = tf.image.resize_area(self.sacc_result[:, :, :, :19], self.upsample_size,
                                                          align_corners=False, name='upsample_heatmat')
            self.tensor_pafMat_up = tf.image.resize_area(self.sacc_result[:, :, :, 19:], self.upsample_size,
                                                         align_corners=False, name='upsample_pafmat')
            # below is the peak calculation part
            smoother = Smoother({'data': self.tensor_heatMat_up}, 5, 3.0)
            gaussian_heatMat = smoother.get_output()
    
            max_pooled_in_tensor = tf.nn.pool(gaussian_heatMat, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks = tf.where(tf.equal(gaussian_heatMat, max_pooled_in_tensor), gaussian_heatMat,
                                         tf.zeros_like(gaussian_heatMat))
    
            self.heatMat = self.pafMat = None
            
            ################
            self.sacc_result2 = result[1]
            
            self.tensor_heatMat2 = self.sacc_result2[:, :, :, :19]
            self.tensor_pafMat2 = self.sacc_result2[:, :, :, 19:]
    
            self.upsample_size2 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size2')
            self.tensor_heatMat_up2 = tf.image.resize_area(self.sacc_result2[:, :, :, :19], self.upsample_size2,
                                                          align_corners=False, name='upsample_heatmat2')
            self.tensor_pafMat_up2 = tf.image.resize_area(self.sacc_result2[:, :, :, 19:], self.upsample_size2,
                                                         align_corners=False, name='upsample_pafmat2')
            # below is the peak calculation part
            smoother2 = Smoother({'data': self.tensor_heatMat_up2}, 5, 3.0)
            gaussian_heatMat2 = smoother2.get_output()
    
            max_pooled_in_tensor2 = tf.nn.pool(gaussian_heatMat2, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks2 = tf.where(tf.equal(gaussian_heatMat2, max_pooled_in_tensor2), gaussian_heatMat2,
                                         tf.zeros_like(gaussian_heatMat2))
    
            self.heatMat2 = self.pafMat2 = None
            
            
            ###############
            
            self.sacc_result3 = result[2]
            
            self.tensor_heatMat3 = self.sacc_result3[:, :, :, :19]
            self.tensor_pafMat3 = self.sacc_result3[:, :, :, 19:]
    
            self.upsample_size3 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size3')
            self.tensor_heatMat_up3 = tf.image.resize_area(self.sacc_result3[:, :, :, :19], self.upsample_size3,
                                                          align_corners=False, name='upsample_heatmat3')
            self.tensor_pafMat_up3 = tf.image.resize_area(self.sacc_result3[:, :, :, 19:], self.upsample_size3,
                                                         align_corners=False, name='upsample_pafmat3')
            # below is the peak calculation part
            smoother3 = Smoother({'data': self.tensor_heatMat_up3}, 5, 3.0)
            gaussian_heatMat3 = smoother3.get_output()
    
            max_pooled_in_tensor3 = tf.nn.pool(gaussian_heatMat3, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks3 = tf.where(tf.equal(gaussian_heatMat3, max_pooled_in_tensor3), gaussian_heatMat3,
                                         tf.zeros_like(gaussian_heatMat3))
    
            self.heatMat3 = self.pafMat3 = None
            
            ##############
            
            self.sacc_result4 = result[3]
            
            self.tensor_heatMat4 = self.sacc_result4[:, :, :, :19]
            self.tensor_pafMat4 = self.sacc_result4[:, :, :, 19:]
    
            self.upsample_size4 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size4')
            self.tensor_heatMat_up4 = tf.image.resize_area(self.sacc_result4[:, :, :, :19], self.upsample_size4,
                                                          align_corners=False, name='upsample_heatmat4')
            self.tensor_pafMat_up4 = tf.image.resize_area(self.sacc_result4[:, :, :, 19:], self.upsample_size4,
                                                         align_corners=False, name='upsample_pafmat4')
            # below is the peak calculation part
            smoother4 = Smoother({'data': self.tensor_heatMat_up4}, 5, 3.0)
            gaussian_heatMat4 = smoother4.get_output()
    
            max_pooled_in_tensor4 = tf.nn.pool(gaussian_heatMat4, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks4 = tf.where(tf.equal(gaussian_heatMat4, max_pooled_in_tensor4), gaussian_heatMat4,
                                         tf.zeros_like(gaussian_heatMat4))
    
            self.heatMat4 = self.pafMat4 = None
            
            ###############
            
            self.sacc_result5 = result[4]
            
            self.tensor_heatMat5 = self.sacc_result5[:, :, :, :19]
            self.tensor_pafMat5 = self.sacc_result5[:, :, :, 19:]
    
            self.upsample_size5 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size5')
            self.tensor_heatMat_up5 = tf.image.resize_area(self.sacc_result5[:, :, :, :19], self.upsample_size5,
                                                          align_corners=False, name='upsample_heatmat5')
            self.tensor_pafMat_up5 = tf.image.resize_area(self.sacc_result5[:, :, :, 19:], self.upsample_size5,
                                                         align_corners=False, name='upsample_pafmat5')
            # below is the peak calculation part
            smoother5 = Smoother({'data': self.tensor_heatMat_up5}, 5, 3.0)
            gaussian_heatMat5 = smoother5.get_output()
    
            max_pooled_in_tensor5 = tf.nn.pool(gaussian_heatMat5, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks5 = tf.where(tf.equal(gaussian_heatMat5, max_pooled_in_tensor5), gaussian_heatMat5,
                                         tf.zeros_like(gaussian_heatMat5))
    
            self.heatMat5 = self.pafMat5 = None
            
            ##############
            
            self.sacc_result6 = result[5]
            
            self.tensor_heatMat6 = self.sacc_result6[:, :, :, :19]
            self.tensor_pafMat6 = self.sacc_result6[:, :, :, 19:]
    
            self.upsample_size6 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size6')
            self.tensor_heatMat_up6 = tf.image.resize_area(self.sacc_result6[:, :, :, :19], self.upsample_size6,
                                                          align_corners=False, name='upsample_heatmat6')
            self.tensor_pafMat_up6 = tf.image.resize_area(self.sacc_result6[:, :, :, 19:], self.upsample_size6,
                                                         align_corners=False, name='upsample_pafmat6')
            # below is the peak calculation part
            smoother6 = Smoother({'data': self.tensor_heatMat_up6}, 5, 3.0)
            gaussian_heatMat6 = smoother6.get_output()
    
            max_pooled_in_tensor6 = tf.nn.pool(gaussian_heatMat6, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks6 = tf.where(tf.equal(gaussian_heatMat6, max_pooled_in_tensor6), gaussian_heatMat6,
                                         tf.zeros_like(gaussian_heatMat6))
    
            self.heatMat6 = self.pafMat6 = None
            
            ##############
            
            self.sacc_result7 = result[6]
            
            self.tensor_heatMat7 = self.sacc_result7[:, :, :, :19]
            self.tensor_pafMat7 = self.sacc_result7[:, :, :, 19:]
    
            self.upsample_size7 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size7')
            self.tensor_heatMat_up7 = tf.image.resize_area(self.sacc_result7[:, :, :, :19], self.upsample_size7,
                                                          align_corners=False, name='upsample_heatmat7')
            self.tensor_pafMat_up7 = tf.image.resize_area(self.sacc_result7[:, :, :, 19:], self.upsample_size7,
                                                         align_corners=False, name='upsample_pafmat7')
            # below is the peak calculation part
            smoother7 = Smoother({'data': self.tensor_heatMat_up7}, 5, 3.0)
            gaussian_heatMat7 = smoother7.get_output()
    
            max_pooled_in_tensor7 = tf.nn.pool(gaussian_heatMat7, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks7 = tf.where(tf.equal(gaussian_heatMat7, max_pooled_in_tensor7), gaussian_heatMat7,
                                         tf.zeros_like(gaussian_heatMat7))
    
            self.heatMat7 = self.pafMat7 = None
            
            ##############
            
            self.sacc_result8 = result[7]
            
            self.tensor_heatMat8 = self.sacc_result8[:, :, :, :19]
            self.tensor_pafMat8 = self.sacc_result8[:, :, :, 19:]
    
            self.upsample_size8 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size8')
            self.tensor_heatMat_up8 = tf.image.resize_area(self.sacc_result8[:, :, :, :19], self.upsample_size8,
                                                          align_corners=False, name='upsample_heatmat8')
            self.tensor_pafMat_up8 = tf.image.resize_area(self.sacc_result8[:, :, :, 19:], self.upsample_size8,
                                                         align_corners=False, name='upsample_pafmat8')
            # below is the peak calculation part
            smoother8 = Smoother({'data': self.tensor_heatMat_up8}, 5, 3.0)
            gaussian_heatMat8 = smoother8.get_output()
    
            max_pooled_in_tensor8 = tf.nn.pool(gaussian_heatMat8, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks8 = tf.where(tf.equal(gaussian_heatMat8, max_pooled_in_tensor8), gaussian_heatMat8,
                                         tf.zeros_like(gaussian_heatMat8))
    
            self.heatMat8 = self.pafMat8 = None
            
            self.sacc_result0 = result[8]
        
            self.tensor_heatMat0 = self.sacc_result[:, :, :, :19]
            self.tensor_pafMat0 = self.sacc_result[:, :, :, 19:]
    
            self.upsample_size0 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size0')
            self.tensor_heatMat_up0 = tf.image.resize_area(self.sacc_result0[:, :, :, :19], self.upsample_size0,
                                                          align_corners=False, name='upsample_heatmat0')
            self.tensor_pafMat_up0 = tf.image.resize_area(self.sacc_result0[:, :, :, 19:], self.upsample_size0,
                                                         align_corners=False, name='upsample_pafmat0')
            # below is the peak calculation part
            smoother0 = Smoother({'data': self.tensor_heatMat_up0}, 5, 3.0)
            gaussian_heatMat0 = smoother0.get_output()
    
            max_pooled_in_tensor0 = tf.nn.pool(gaussian_heatMat0, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks0 = tf.where(tf.equal(gaussian_heatMat0, max_pooled_in_tensor0), gaussian_heatMat0,
                                         tf.zeros_like(gaussian_heatMat0))
    
            self.heatMat0 = self.pafMat0 = None
            
            ################
            self.sacc_result20 = result[9]
            
            self.tensor_heatMat20 = self.sacc_result20[:, :, :, :19]
            self.tensor_pafMat20 = self.sacc_result20[:, :, :, 19:]
    
            self.upsample_size20 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size20')
            self.tensor_heatMat_up20 = tf.image.resize_area(self.sacc_result20[:, :, :, :19], self.upsample_size20,
                                                          align_corners=False, name='upsample_heatmat20')
            self.tensor_pafMat_up20 = tf.image.resize_area(self.sacc_result20[:, :, :, 19:], self.upsample_size20,
                                                         align_corners=False, name='upsample_pafmat20')
            # below is the peak calculation part
            smoother20 = Smoother({'data': self.tensor_heatMat_up20}, 5, 3.0)
            gaussian_heatMat20 = smoother20.get_output()
    
            max_pooled_in_tensor20 = tf.nn.pool(gaussian_heatMat20, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks20 = tf.where(tf.equal(gaussian_heatMat20, max_pooled_in_tensor20), gaussian_heatMat20,
                                         tf.zeros_like(gaussian_heatMat20))
    
            self.heatMat20 = self.pafMat20 = None
            
            
            ###############
            
            self.sacc_result30 = result[10]
            
            self.tensor_heatMat30 = self.sacc_result30[:, :, :, :19]
            self.tensor_pafMat30 = self.sacc_result30[:, :, :, 19:]
    
            self.upsample_size30 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size30')
            self.tensor_heatMat_up30 = tf.image.resize_area(self.sacc_result30[:, :, :, :19], self.upsample_size30,
                                                          align_corners=False, name='upsample_heatmat30')
            self.tensor_pafMat_up30 = tf.image.resize_area(self.sacc_result30[:, :, :, 19:], self.upsample_size30,
                                                         align_corners=False, name='upsample_pafmat30')
            # below is the peak calculation part
            smoother30 = Smoother({'data': self.tensor_heatMat_up30}, 5, 3.0)
            gaussian_heatMat30 = smoother30.get_output()
    
            max_pooled_in_tensor30 = tf.nn.pool(gaussian_heatMat30, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks30 = tf.where(tf.equal(gaussian_heatMat30, max_pooled_in_tensor30), gaussian_heatMat30,
                                         tf.zeros_like(gaussian_heatMat30))
    
            self.heatMat30 = self.pafMat30 = None
            
            ##############
            
            self.sacc_result40 = result[11]
            
            self.tensor_heatMat40 = self.sacc_result40[:, :, :, :19]
            self.tensor_pafMat40 = self.sacc_result40[:, :, :, 19:]
    
            self.upsample_size40 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size40')
            self.tensor_heatMat_up40 = tf.image.resize_area(self.sacc_result40[:, :, :, :19], self.upsample_size40,
                                                          align_corners=False, name='upsample_heatmat40')
            self.tensor_pafMat_up40 = tf.image.resize_area(self.sacc_result40[:, :, :, 19:], self.upsample_size40,
                                                         align_corners=False, name='upsample_pafmat40')
            # below is the peak calculation part
            smoother40 = Smoother({'data': self.tensor_heatMat_up40}, 5, 3.0)
            gaussian_heatMat40 = smoother40.get_output()
    
            max_pooled_in_tensor40 = tf.nn.pool(gaussian_heatMat40, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks40 = tf.where(tf.equal(gaussian_heatMat40, max_pooled_in_tensor40), gaussian_heatMat40,
                                         tf.zeros_like(gaussian_heatMat40))
    
            self.heatMat40 = self.pafMat40 = None
            
            ###############
            
            self.sacc_result50 = result[12]
            
            self.tensor_heatMat50 = self.sacc_result50[:, :, :, :19]
            self.tensor_pafMat50 = self.sacc_result50[:, :, :, 19:]
    
            self.upsample_size50 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size50')
            self.tensor_heatMat_up50 = tf.image.resize_area(self.sacc_result50[:, :, :, :19], self.upsample_size50,
                                                          align_corners=False, name='upsample_heatmat50')
            self.tensor_pafMat_up50 = tf.image.resize_area(self.sacc_result50[:, :, :, 19:], self.upsample_size50,
                                                         align_corners=False, name='upsample_pafmat50')
            # below is the peak calculation part
            smoother50 = Smoother({'data': self.tensor_heatMat_up50}, 5, 3.0)
            gaussian_heatMat50 = smoother50.get_output()
    
            max_pooled_in_tensor50 = tf.nn.pool(gaussian_heatMat50, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks50 = tf.where(tf.equal(gaussian_heatMat50, max_pooled_in_tensor50), gaussian_heatMat50,
                                         tf.zeros_like(gaussian_heatMat50))
    
            self.heatMat50 = self.pafMat50 = None
            
            ##############
            
            self.sacc_result60 = result[13]
            
            self.tensor_heatMat60 = self.sacc_result60[:, :, :, :19]
            self.tensor_pafMat60 = self.sacc_result60[:, :, :, 19:]
    
            self.upsample_size60 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size60')
            self.tensor_heatMat_up60 = tf.image.resize_area(self.sacc_result60[:, :, :, :19], self.upsample_size60,
                                                          align_corners=False, name='upsample_heatmat60')
            self.tensor_pafMat_up60 = tf.image.resize_area(self.sacc_result60[:, :, :, 19:], self.upsample_size60,
                                                         align_corners=False, name='upsample_pafmat60')
            # below is the peak calculation part
            smoother60 = Smoother({'data': self.tensor_heatMat_up60}, 5, 3.0)
            gaussian_heatMat60 = smoother60.get_output()
    
            max_pooled_in_tensor60 = tf.nn.pool(gaussian_heatMat60, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks60 = tf.where(tf.equal(gaussian_heatMat60, max_pooled_in_tensor60), gaussian_heatMat60,
                                         tf.zeros_like(gaussian_heatMat60))
    
            self.heatMat60 = self.pafMat60 = None
            
            ##############
            
            self.sacc_result70 = result[14]
            
            self.tensor_heatMat70 = self.sacc_result70[:, :, :, :19]
            self.tensor_pafMat70 = self.sacc_result70[:, :, :, 19:]
    
            self.upsample_size70 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size70')
            self.tensor_heatMat_up70 = tf.image.resize_area(self.sacc_result70[:, :, :, :19], self.upsample_size70,
                                                          align_corners=False, name='upsample_heatmat70')
            self.tensor_pafMat_up70 = tf.image.resize_area(self.sacc_result70[:, :, :, 19:], self.upsample_size70,
                                                         align_corners=False, name='upsample_pafmat70')
            # below is the peak calculation part
            smoother70 = Smoother({'data': self.tensor_heatMat_up70}, 5, 3.0)
            gaussian_heatMat70 = smoother70.get_output()
    
            max_pooled_in_tensor70 = tf.nn.pool(gaussian_heatMat70, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks70 = tf.where(tf.equal(gaussian_heatMat70, max_pooled_in_tensor70), gaussian_heatMat70,
                                         tf.zeros_like(gaussian_heatMat70))
    
            self.heatMat70 = self.pafMat70 = None
            
            ##############
            
            self.sacc_result80 = result[15]
            
            self.tensor_heatMat80 = self.sacc_result80[:, :, :, :19]
            self.tensor_pafMat80 = self.sacc_result80[:, :, :, 19:]
    
            self.upsample_size80 = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size80')
            self.tensor_heatMat_up80 = tf.image.resize_area(self.sacc_result80[:, :, :, :19], self.upsample_size80,
                                                          align_corners=False, name='upsample_heatmat80')
            self.tensor_pafMat_up80 = tf.image.resize_area(self.sacc_result80[:, :, :, 19:], self.upsample_size80,
                                                         align_corners=False, name='upsample_pafmat80')
            # below is the peak calculation part
            smoother80 = Smoother({'data': self.tensor_heatMat_up80}, 5, 3.0)
            gaussian_heatMat80 = smoother80.get_output()
    
            max_pooled_in_tensor80 = tf.nn.pool(gaussian_heatMat80, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
            self.tensor_peaks80 = tf.where(tf.equal(gaussian_heatMat80, max_pooled_in_tensor80), gaussian_heatMat80,
                                         tf.zeros_like(gaussian_heatMat80))
    
            self.heatMat80 = self.pafMat80 = None
    
            # warm-up
            self.persistent_sess.run(tf.variables_initializer(
                [v for v in tf.global_variables() if
                 v.name.split(':')[0] in [x.decode('utf-8') for x in
                                          self.persistent_sess.run(tf.report_uninitialized_variables())]
                 ])
            )
            
            self.persistent_sess.run(
            [self.tensor_peaks80, self.tensor_heatMat_up80, self.tensor_pafMat_up80, self.tensor_peaks70, self.tensor_heatMat_up70, self.tensor_pafMat_up70, self.tensor_peaks60, self.tensor_heatMat_up60, self.tensor_pafMat_up60, self.tensor_peaks50, self.tensor_heatMat_up50, self.tensor_pafMat_up50, self.tensor_peaks40, self.tensor_heatMat_up40, self.tensor_pafMat_up40, self.tensor_peaks30, self.tensor_heatMat_up30, self.tensor_pafMat_up30, self.tensor_peaks20, self.tensor_heatMat_up20, self.tensor_pafMat_up20,self.tensor_peaks0, self.tensor_heatMat_up0, self.tensor_pafMat_up0, self.tensor_peaks8, self.tensor_heatMat_up8, self.tensor_pafMat_up8, self.tensor_peaks7, self.tensor_heatMat_up7, self.tensor_pafMat_up7, self.tensor_peaks6, self.tensor_heatMat_up6, self.tensor_pafMat_up6, self.tensor_peaks5, self.tensor_heatMat_up5, self.tensor_pafMat_up5, self.tensor_peaks4, self.tensor_heatMat_up4, self.tensor_pafMat_up4, self.tensor_peaks3, self.tensor_heatMat_up3, self.tensor_pafMat_up3, self.tensor_peaks2, self.tensor_heatMat_up2, self.tensor_pafMat_up2,self.tensor_peaks, self.tensor_heatMat_up, self.tensor_pafMat_up], 
            feed_dict={
                self.tensor_image: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image2: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image3: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image4: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image5: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image6: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image7: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image8: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image0: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image20: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image30: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image40: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image50: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image60: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image70: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.tensor_image80: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.upsample_size: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size2: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size3: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size4: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size5: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size6: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size7: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size8: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size0: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size20: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size30: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size40: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size50: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size60: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size70: [target_size[1] // 2, target_size[0] // 2],
                self.upsample_size80: [target_size[1] // 2, target_size[0] // 2]
            }
           )
            
    
            frames = 0
            total_t = 0
            t_init = time.time()
            stop = False
            while (not stop or not input_q.empty()):
                reading = time.time()
                if not info_in_stop.empty():
                  stop = True
                  info_in_stop.get()
                  info_out.put(t_init - accel_start)
                if not input_q.empty():
                  img, img2, img3, img4, img5, img6, img7, img8, img0, img20, img30, img40, img50, img60, img70, img80 = input_q.get()
                  frames += 16
                  upsample_size=self.args.resize_out_ratio
                  upsample_size = [int(self.target_size[1] / 8 * upsample_size), int(self.target_size[0] / 8 * upsample_size)]
                  run_options = tf.RunOptions(trace_level=tf.RunOptions.FULL_TRACE)
                  run_metadata = tf.RunMetadata()
                  peaks80, heatMat_up80, pafMat_up80, peaks70, heatMat_up70, pafMat_up70, peaks60, heatMat_up60, pafMat_up60, peaks50, heatMat_up50, pafMat_up50, peaks40, heatMat_up40, pafMat_up40, peaks30, heatMat_up30, pafMat_up30, peaks20, heatMat_up20, pafMat_up20, peaks0, heatMat_up0, pafMat_up0, peaks8, heatMat_up8, pafMat_up8, peaks7, heatMat_up7, pafMat_up7, peaks6, heatMat_up6, pafMat_up6, peaks5, heatMat_up5, pafMat_up5, peaks4, heatMat_up4, pafMat_up4, peaks3, heatMat_up3, pafMat_up3, peaks2, heatMat_up2, pafMat_up2, peaks, heatMat_up, pafMat_up = self.persistent_sess.run( 
            [self.tensor_peaks80, self.tensor_heatMat_up80, self.tensor_pafMat_up80, self.tensor_peaks70, self.tensor_heatMat_up70, self.tensor_pafMat_up70, self.tensor_peaks60, self.tensor_heatMat_up60, self.tensor_pafMat_up60, self.tensor_peaks50, self.tensor_heatMat_up50, self.tensor_pafMat_up50, self.tensor_peaks40, self.tensor_heatMat_up40, self.tensor_pafMat_up40, self.tensor_peaks30, self.tensor_heatMat_up30, self.tensor_pafMat_up30, self.tensor_peaks20, self.tensor_heatMat_up20, self.tensor_pafMat_up20,self.tensor_peaks0, self.tensor_heatMat_up0, self.tensor_pafMat_up0, self.tensor_peaks8, self.tensor_heatMat_up8, self.tensor_pafMat_up8, self.tensor_peaks7, self.tensor_heatMat_up7, self.tensor_pafMat_up7, self.tensor_peaks6, self.tensor_heatMat_up6, self.tensor_pafMat_up6, self.tensor_peaks5, self.tensor_heatMat_up5, self.tensor_pafMat_up5, self.tensor_peaks4, self.tensor_heatMat_up4, self.tensor_pafMat_up4, self.tensor_peaks3, self.tensor_heatMat_up3, self.tensor_pafMat_up3, self.tensor_peaks2, self.tensor_heatMat_up2, self.tensor_pafMat_up2, self.tensor_peaks, self.tensor_heatMat_up, self.tensor_pafMat_up], feed_dict={
                self.tensor_image: [img], self.tensor_image2: [img2],self.tensor_image3: [img3], self.tensor_image4: [img4], self.tensor_image5: [img5], self.tensor_image6: [img6], self.tensor_image7: [img7], self.tensor_image8: [img8], self.tensor_image0: [img0], self.tensor_image20: [img20],self.tensor_image30: [img30], self.tensor_image40: [img40], self.tensor_image50: [img50], self.tensor_image60: [img60], self.tensor_image70: [img70], self.tensor_image80: [img80], self.upsample_size: upsample_size, self.upsample_size2: upsample_size, self.upsample_size3: upsample_size, self.upsample_size4: upsample_size,self.upsample_size5: upsample_size,self.upsample_size6: upsample_size,self.upsample_size7: upsample_size,self.upsample_size8: upsample_size, self.upsample_size0: upsample_size, self.upsample_size20: upsample_size, self.upsample_size30: upsample_size, self.upsample_size40: upsample_size,self.upsample_size50: upsample_size,self.upsample_size60: upsample_size,self.upsample_size70: upsample_size,self.upsample_size80: upsample_size
            })
                  peaks = peaks[0]
                  self.heatMat = heatMat_up[0]
                  self.pafMat = pafMat_up[0]
                  output_q.put(img)
                  output_q.put(peaks)
                  output_q.put(self.heatMat)
                  output_q.put(self.pafMat)
                  
                  peaks2 = peaks2[0]
                  self.heatMat2 = heatMat_up2[0]
                  self.pafMat2 = pafMat_up2[0]
                  output_q.put(img2)
                  output_q.put(peaks2)
                  output_q.put(self.heatMat2)
                  output_q.put(self.pafMat2)
                  
                  peaks3 = peaks3[0]
                  self.heatMat3 = heatMat_up3[0]
                  self.pafMat3 = pafMat_up3[0]
                  output_q.put(img3)
                  output_q.put(peaks3)
                  output_q.put(self.heatMat3)
                  output_q.put(self.pafMat3)
                  
                  peaks4 = peaks4[0]
                  self.heatMat4 = heatMat_up4[0]
                  self.pafMat4 = pafMat_up4[0]
                  output_q.put(img4)
                  output_q.put(peaks4)
                  output_q.put(self.heatMat4)
                  output_q.put(self.pafMat4)
                  
                  peaks5 = peaks5[0]
                  self.heatMat5 = heatMat_up5[0]
                  self.pafMat5 = pafMat_up5[0]
                  output_q.put(img5)
                  output_q.put(peaks5)
                  output_q.put(self.heatMat5)
                  output_q.put(self.pafMat5)
                  
                  peaks6 = peaks6[0]
                  self.heatMat6 = heatMat_up6[0]
                  self.pafMat6 = pafMat_up6[0]
                  output_q.put(img6)
                  output_q.put(peaks6)
                  output_q.put(self.heatMat6)
                  output_q.put(self.pafMat6)
                  
                  peaks7 = peaks7[0]
                  self.heatMat7 = heatMat_up7[0]
                  self.pafMat7 = pafMat_up7[0]
                  output_q.put(img7)
                  output_q.put(peaks7)
                  output_q.put(self.heatMat7)
                  output_q.put(self.pafMat7)
                  
                  peaks8 = peaks8[0]
                  self.heatMat8 = heatMat_up8[0]
                  self.pafMat8 = pafMat_up8[0]
                  output_q.put(img8)
                  output_q.put(peaks8)
                  output_q.put(self.heatMat8)
                  output_q.put(self.pafMat8)
                  
                  peaks0 = peaks0[0]
                  self.heatMat0 = heatMat_up0[0]
                  self.pafMat0 = pafMat_up0[0]
                  output_q.put(img0)
                  output_q.put(peaks0)
                  output_q.put(self.heatMat0)
                  output_q.put(self.pafMat0)
                  
                  peaks20 = peaks20[0]
                  self.heatMat20 = heatMat_up20[0]
                  self.pafMat20 = pafMat_up20[0]
                  output_q.put(img20)
                  output_q.put(peaks20)
                  output_q.put(self.heatMat20)
                  output_q.put(self.pafMat20)
                  
                  peaks30 = peaks30[0]
                  self.heatMat30 = heatMat_up30[0]
                  self.pafMat30 = pafMat_up30[0]
                  output_q.put(img30)
                  output_q.put(peaks30)
                  output_q.put(self.heatMat30)
                  output_q.put(self.pafMat30)
                  
                  peaks40 = peaks40[0]
                  self.heatMat40 = heatMat_up40[0]
                  self.pafMat40 = pafMat_up40[0]
                  output_q.put(img40)
                  output_q.put(peaks40)
                  output_q.put(self.heatMat40)
                  output_q.put(self.pafMat40)
                  
                  peaks50 = peaks50[0]
                  self.heatMat50 = heatMat_up50[0]
                  self.pafMat50 = pafMat_up50[0]
                  output_q.put(img50)
                  output_q.put(peaks50)
                  output_q.put(self.heatMat50)
                  output_q.put(self.pafMat50)
                  
                  peaks60 = peaks60[0]
                  self.heatMat60 = heatMat_up60[0]
                  self.pafMat60 = pafMat_up60[0]
                  output_q.put(img60)
                  output_q.put(peaks60)
                  output_q.put(self.heatMat60)
                  output_q.put(self.pafMat60)
                  
                  peaks70 = peaks70[0]
                  self.heatMat70 = heatMat_up70[0]
                  self.pafMat70 = pafMat_up70[0]
                  output_q.put(img70)
                  output_q.put(peaks70)
                  output_q.put(self.heatMat70)
                  output_q.put(self.pafMat70)
                  
                  peaks80 = peaks80[0]
                  self.heatMat80 = heatMat_up80[0]
                  self.pafMat80 = pafMat_up80[0]
                  output_q.put(img80)
                  output_q.put(peaks80)
                  output_q.put(self.heatMat80)
                  output_q.put(self.pafMat80)
                  
                  
                  self.num_run = 16.0
                  elapsed = time.time() - reading
                  total_t += elapsed
                  
            if stop:
              info_out_stop.put(1)
                  
  
     
     def worker_est_draw(self, in_q, out_q, info_in_stop, info_out_stop):
             frames = 0
             num_frame = 1e10
             stop = False
             while (not stop or not in_q.empty()):
                  if not info_in_stop.empty():
                    info_in_stop.get()
                    stop = True
                  image = in_q.get()
                  peaks = in_q.get()
                  self.heatMat = in_q.get()
                  self.pafMat = in_q.get()
                  humans = PoseEstimator.estimate_paf(peaks, self.heatMat, self.pafMat)
                  image = TfPoseEstimator.draw_humans(image, humans, imgcopy=False)
                  out_q.put(image)
                  frames += 1
             if stop:
               info_out_stop.put(1)
                  
     
     def _get_scaled_img(self, npimg, scale):
        get_base_scale = lambda s, w, h: max(self.target_size[0] / float(h), self.target_size[1] / float(w)) * s
        img_h, img_w = npimg.shape[:2]

        if scale is None:
            if npimg.shape[:2] != (self.target_size[1], self.target_size[0]):
                # resize
                npimg = cv2.resize(npimg, self.target_size, interpolation=cv2.INTER_CUBIC)
            return [npimg], [(0.0, 0.0, 1.0, 1.0)]
        elif isinstance(scale, float):
            # scaling with center crop
            base_scale = get_base_scale(scale, img_w, img_h)
            npimg = cv2.resize(npimg, dsize=None, fx=base_scale, fy=base_scale, interpolation=cv2.INTER_CUBIC)

            o_size_h, o_size_w = npimg.shape[:2]
            if npimg.shape[0] < self.target_size[1] or npimg.shape[1] < self.target_size[0]:
                newimg = np.zeros(
                    (max(self.target_size[1], npimg.shape[0]), max(self.target_size[0], npimg.shape[1]), 3),
                    dtype=np.uint8)
                newimg[:npimg.shape[0], :npimg.shape[1], :] = npimg
                npimg = newimg

            windows = sw.generate(npimg, sw.DimOrder.HeightWidthChannel, self.target_size[0], self.target_size[1], 0.2)

            rois = []
            ratios = []
            for window in windows:
                indices = window.indices()
                roi = npimg[indices]
                rois.append(roi)
                ratio_x, ratio_y = float(indices[1].start) / o_size_w, float(indices[0].start) / o_size_h
                ratio_w, ratio_h = float(indices[1].stop - indices[1].start) / o_size_w, float(
                    indices[0].stop - indices[0].start) / o_size_h
                ratios.append((ratio_x, ratio_y, ratio_w, ratio_h))

            return rois, ratios
        elif isinstance(scale, tuple) and len(scale) == 2:
            # scaling with sliding window : (scale, step)
            base_scale = get_base_scale(scale[0], img_w, img_h)
            npimg = cv2.resize(npimg, dsize=None, fx=base_scale, fy=base_scale, interpolation=cv2.INTER_CUBIC)
            o_size_h, o_size_w = npimg.shape[:2]
            if npimg.shape[0] < self.target_size[1] or npimg.shape[1] < self.target_size[0]:
                newimg = np.zeros(
                    (max(self.target_size[1], npimg.shape[0]), max(self.target_size[0], npimg.shape[1]), 3),
                    dtype=np.uint8)
                newimg[:npimg.shape[0], :npimg.shape[1], :] = npimg
                npimg = newimg

            window_step = scale[1]

            windows = sw.generate(npimg, sw.DimOrder.HeightWidthChannel, self.target_size[0], self.target_size[1],
                                  window_step)

            rois = []
            ratios = []
            for window in windows:
                indices = window.indices()
                roi = npimg[indices]
                rois.append(roi)
                ratio_x, ratio_y = float(indices[1].start) / o_size_w, float(indices[0].start) / o_size_h
                ratio_w, ratio_h = float(indices[1].stop - indices[1].start) / o_size_w, float(
                    indices[0].stop - indices[0].start) / o_size_h
                ratios.append((ratio_x, ratio_y, ratio_w, ratio_h))

            return rois, ratios
        elif isinstance(scale, tuple) and len(scale) == 3:
            # scaling with ROI : (want_x, want_y, scale_ratio)
            base_scale = get_base_scale(scale[2], img_w, img_h)
            npimg = cv2.resize(npimg, dsize=None, fx=base_scale, fy=base_scale, interpolation=cv2.INTER_CUBIC)
            ratio_w = self.target_size[0] / float(npimg.shape[1])
            ratio_h = self.target_size[1] / float(npimg.shape[0])

            want_x, want_y = scale[:2]
            ratio_x = want_x - ratio_w / 2.
            ratio_y = want_y - ratio_h / 2.
            ratio_x = max(ratio_x, 0.0)
            ratio_y = max(ratio_y, 0.0)
            if ratio_x + ratio_w > 1.0:
                ratio_x = 1. - ratio_w
            if ratio_y + ratio_h > 1.0:
                ratio_y = 1. - ratio_h

            roi = self._crop_roi(npimg, ratio_x, ratio_y)
            return [roi], [(ratio_x, ratio_y, ratio_w, ratio_h)]
     
     
     def _crop_roi(self, npimg, ratio_x, ratio_y):
        target_w, target_h = self.target_size
        h, w = npimg.shape[:2]
        x = max(int(w * ratio_x - .5), 0)
        y = max(int(h * ratio_y - .5), 0)
        cropped = npimg[y:y + target_h, x:x + target_w]

        cropped_h, cropped_w = cropped.shape[:2]
        if cropped_w < target_w or cropped_h < target_h:
            npblank = np.zeros((self.target_size[1], self.target_size[0], 3), dtype=np.uint8)

            copy_x, copy_y = (target_w - cropped_w) // 2, (target_h - cropped_h) // 2
            npblank[copy_y:copy_y + cropped_h, copy_x:copy_x + cropped_w] = cropped
        else:
            return cropped
     
     def apply_video(self):
        fps_time = 0
        
        cap = cv2.VideoCapture(self.args.video)
        
        frame_width_original = int(cap.get(3))
        frame_height_original = int(cap.get(4))
#        frame_width = 384
#        frame_height = 384
#        self.out = cv2.VideoWriter('output_{}.avi'.format(self.args.device),cv2.VideoWriter_fourcc('M','J','P','G'), 10, (frame_width, frame_height))
        run = 0
        

        if cap.isOpened() is False:
            print("Error opening video stream or file")
        prevImage = None
        diff = None
        skipped = 0
        frames = 0
        processed_frames = 0
        start = time.time()
        while cap.isOpened():
            run += 1.0
            num_run = 8.0
            ret_val, image = cap.read()
            if image is None:
              break
            frames += 1
            
            self.input_q.put(image)
            
        
        self.info_in_s_pack.put(frames)
        extra = 0        
        self.t3.join()
        self.t.join()
        self.t5.join()
        self.t2.join()
        end = time.time()
        t_init = self.info_accel.get()
        cap.release()  
        cv2.destroyAllWindows()

     def apply(self):
        # estimate human poses from a single image !
        self.image = common.read_imgfile(self.args.image, None, None)
        if self.image is None:
            logger.error('Image can not be read, path=%s' % args.image)
            sys.exit(-1)

        t = time.time()
        self.humans = self.e.inference(self.image, resize_to_default=(self.w > 0 and self.h > 0), upsample_size=self.args.resize_out_ratio)
        elapsed = time.time() - t

        logger.info('Inference %s via %s in %.4f seconds.' % (os.path.basename(self.args.image), self.device, elapsed))
        logger.info('Translated to FPS is %.4f.' % (1.0 / elapsed))

        self.image = TfPoseEstimator.draw_humans(self.image, self.humans, imgcopy=False)
        
        return self.image, self.humans        


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='tf-pose-estimation run')
    parser.add_argument('--video', type=str, default='')
    parser.add_argument('--model', type=str, default='mobilenet_thin',
                        help='cmu / mobilenet_thin / mobilenet_v2_large / mobilenet_v2_small')
    parser.add_argument('--resize', type=str, default='384x384',
                        help='if provided, resize images before they are processed. '
                             'default=0x0, Recommends : 432x368 or 656x368 or 1312x736 ')
    parser.add_argument('--resize-out-ratio', type=float, default=4.0,
                        help='if provided, resize heatmaps before they are post-processed. default=1.0')
    parser.add_argument('--thresh', type=str, default=0.15,
                        help='if provided, resize heatmaps before they are post-processed. default=0.15')
    parser.add_argument('--display', action='store_true',
                        help='whether output result to monitor')
    parser.add_argument('--device', type=str, default='CPU',
                        help='specify the inference device')
    parser.add_argument('--show-process', type=bool, default=False,
                        help='for debug purpose, if enabled, speed for inference is dropped.')
    parser.add_argument('--showBG', type=bool, default=True, help='False to show skeleton only.')
    args = parser.parse_args()
    

    engine = OpenPoseInfer(args, args.device)
    engine.apply_video()   
