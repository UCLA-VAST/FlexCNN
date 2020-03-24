import argparse
import logging
import sys
import time
import os
import os.path

from tf_pose import common
import cv2
import numpy as np
from tf_pose.estimator import TfPoseEstimator, TfPoseEstimatorSacc
from tf_pose.networks import get_graph_path, model_wh
import unittest as ut # added by Tong

logger = logging.getLogger('TfPoseEstimatorRun')
logger.handlers.clear()
logger.setLevel(logging.DEBUG)
ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
formatter = logging.Formatter('[%(asctime)s] [%(name)s] [%(levelname)s] %(message)s')
ch.setFormatter(formatter)
logger.addHandler(ch)


def draw_pics(image, e, device='CPU'):
    try:
        import matplotlib.pyplot as plt

        fig = plt.figure()
        a = fig.add_subplot(2, 2, 1)
        a.set_title('Result')
        plt.imshow(cv2.cvtColor(image, cv2.COLOR_BGR2RGB))

        bgimg = cv2.cvtColor(image.astype(np.uint8), cv2.COLOR_BGR2RGB)
        bgimg = cv2.resize(bgimg, (e.heatMat.shape[1], e.heatMat.shape[0]), interpolation=cv2.INTER_AREA)

        # show network output
        a = fig.add_subplot(2, 2, 2)
        plt.imshow(bgimg, alpha=0.5)
        tmp = np.amax(e.heatMat[:, :, :-1], axis=2)
        plt.imshow(tmp, cmap=plt.cm.gray, alpha=0.5)
        plt.colorbar()

        tmp2 = e.pafMat.transpose((2, 0, 1))
        tmp2_odd = np.amax(np.absolute(tmp2[::2, :, :]), axis=0)
        tmp2_even = np.amax(np.absolute(tmp2[1::2, :, :]), axis=0)

        a = fig.add_subplot(2, 2, 3)
        a.set_title('Vectormap-x')
        # plt.imshow(CocoPose.get_bgimg(inp, target_size=(vectmap.shape[1], vectmap.shape[0])), alpha=0.5)
        plt.imshow(tmp2_odd, cmap=plt.cm.gray, alpha=0.5)
        plt.colorbar()

        a = fig.add_subplot(2, 2, 4)
        a.set_title('Vectormap-y')
        # plt.imshow(CocoPose.get_bgimg(inp, target_size=(vectmap.shape[1], vectmap.shape[0])), alpha=0.5)
        plt.imshow(tmp2_even, cmap=plt.cm.gray, alpha=0.5)
        plt.colorbar()
        plt.show()
        fig.savefig('result_{}.png'.format(device))
    except Exception as e:
        logger.warning('matplitlib error, %s' % e)
        cv2.imshow('result', image)
        cv2.waitKey()

class OpenPoseInfer(object):
     def __init__(self, args, device='CPU'):
        self.device = device
        self.args = args
        self.w, self.h = model_wh(args.resize)
        if device == 'CPU':
            if self.w == 0 or self.h == 0:
                self.e = TfPoseEstimator(get_graph_path(args.model), target_size=(432, 368))
            else:
                self.e = TfPoseEstimator(get_graph_path(args.model), target_size=(self.w, self.h))
        elif device == 'FPGA':
            if self.w == 0 or self.h == 0:
                self.e = TfPoseEstimatorSacc(get_graph_path(args.model), target_size=(432, 368))
            else:
                self.e = TfPoseEstimatorSacc(get_graph_path(args.model), target_size=(self.w, self.h))

        
     def apply(self):
        # estimate human poses from a single image !
        # TODO: make the image path dynamically change; Tong
        self.image = common.read_imgfile(self.args.image, None, None)
        if self.image is None:
            logger.error('Image can not be read, path=%s' % args.image)
            sys.exit(-1)

        t = time.time()
        self.humans, _, _, _ = self.e.inference(self.image, self.image, self.image, self.image, resize_to_default=(self.w > 0 and self.h > 0), upsample_size=self.args.resize_out_ratio)
        elapsed = time.time() - t

        logger.info('Inference %s via %s in %.4f seconds.' % (os.path.basename(self.args.image), self.device, elapsed))
        logger.info('Translated to FPS is %.4f.' % (2.0 / elapsed))

        self.image = TfPoseEstimator.draw_humans(self.image, self.humans, imgcopy=False)
        
        return self.image, self.humans        

class SaccTest(ut.TestCase):
  def testSacc(self):
    self.maxDiff = None
    engineCPU = OpenPoseInfer(args, 'CPU')
    engineFPGA = OpenPoseInfer(args, 'FPGA')
    _, humans_cpu = engineCPU.apply()
    _, humans_fpga = engineFPGA.apply()

    self.assertEqual(str(humans_cpu), str(humans_fpga))

    # draw_pics(image, engineCPU.e, 'CPU')
    # draw_pics(image, engineFPGA.e, 'FPGA')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='tf-pose-estimation run')
    parser.add_argument('--image', type=str, default='./images/p1.jpg')
    parser.add_argument('--model', type=str, default='mobilenet_v2_small',
                        help='cmu / mobilenet_thin / mobilenet_v2_large / mobilenet_v2_small')
    parser.add_argument('--resize', type=str, default='0x0',
                        help='if provided, resize images before they are processed. '
                             'default=0x0, Recommends : 432x368 or 656x368 or 1312x736 ')
    parser.add_argument('--resize-out-ratio', type=float, default=4.0,
                        help='if provided, resize heatmaps before they are post-processed. default=1.0')
    parser.add_argument('--test', action='store_true',
                        help='unitest compares FPGA result with CPU result')
    parser.add_argument('--device', type=str, default='CPU',
                        help='specify the inference device')
    # pass CLI options to unittest. refer to 
    # https://stackoverflow.com/questions/1029891/python-unittest-is-there-a-way-to-pass-command-line-options-to-the-app
    parser.add_argument('unittest_args', nargs='*') 
    args = parser.parse_args()

    if args.test:
        unittest_args = [sys.argv[0]] + args.unittest_args
        ut.main(argv=unittest_args)
    else:
        engine = OpenPoseInfer(args, args.device)
        image, h = engine.apply() 
        print(np.shape(h))
        draw_pics(image, engine.e, args.device)
