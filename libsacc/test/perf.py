import tensorflow as tf
from functools import reduce
import utils
import time
import numpy as np
import random
import cProfile

class PerfTest:
    def __init__(self):
        self.c = utils.Constants()
        self.test_input = tf.placeholder(tf.float32, shape=(1, 384, 384, 3))
        self.sacc_module = tf.load_op_library(self.c.custom_lib_path)
        self.sacc_result = self.sacc_module.sacc([self.test_input])
        self.sesstion = tf.Session()    
        self.total_time = 0

    def run(self, i):
        data =  [random.randint(0, 255) for i in range(384 * 384 * 3)]
        data_input = np.array(data)
        data_input = np.reshape(data_input, (384, 384, 3))
        t = time.time()
        self.sesstion.run(self.sacc_result, feed_dict={self.test_input: [data_input]})
        elapsed = time.time() - t
        print('INFO: From Tf, iter {} inference time={} seconds'.format(i, elapsed), 'translated to {} FPS'.format(1.0 / elapsed))
        if i != 0:
            self.total_time += elapsed # first round contains the init time

if __name__ == "__main__":
    pt = PerfTest()
    iter_times = 10
    for i in range(iter_times):
        pt.run(i)
    print('INFO: From Tf, average inference time={}'.format(pt.total_time / (iter_times - 1)), 'translate FPS={}'.format(1.0 / (pt.total_time / (iter_times - 1))))
