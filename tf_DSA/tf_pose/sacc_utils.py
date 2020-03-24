import struct
import os, re

class Singleton:
    '''
    This singleton class is 
    inspired by and adapted from http://funhacks.net/2017/01/17/singleton/
    not thread safe according 
    '''
    _instance = None
    def __new__(cls):
        if not cls._instance:
            cls._instance = super(Singleton, cls).__new__(cls)
        return cls._instance

class Constants(Singleton):
    def __init__(self):
        # self.sa_dsa_path = os.environ["SA_DSA_DIR"];
        self.sa_dsa_path = '/home/accelerator/ice-ar/libsacc/';

        self.test_input_file_path = self.sa_dsa_path + "/data/input.bin";
        self.expect_output_file_path = self.sa_dsa_path + "/data/output.bin";
        self.custom_lib_path = '/curr/atefehSZ/research/libsacc/build/lib/libsacc.so';
        with open("/curr/atefehSZ/research/libsacc/inc/sacc_params.h", 'r') as fobj:
            for line in fobj:
                res1 = re.match( r'#define\s+LAYER1_IN_NUM\s+(.*)', line, re.I)
                res2 = re.match( r'#define\s+LAYER1_IN_H\s+(.*)', line, re.I)
                res3 = re.match( r'#define\s+LAYER1_IN_W\s+(.*)', line, re.I)

                res4 = re.match( r'#define\s+STAGE2L_OUT_H\s+(.*)', line, re.I)
                res5 = re.match( r'#define\s+STAGE2L_OUT_W\s+(.*)', line, re.I)
                res6 = re.match( r'#define\s+STAGE2R_OUT_NUM\s+(.*)', line, re.I)
                res7 = re.match( r'#define\s+STAGE2L_OUT_NUM\s+(.*)', line, re.I)
                res8 = re.match( r'#define\s+STAGE2L_OUT_H\s+(.*)', line, re.I)
                res9 = re.match( r'#define\s+STAGE2L_OUT_W\s+(.*)', line, re.I)

                if res1: self.LAYER1_IN_NUM = int(res1.group(1))
                if res2: self.LAYER1_IN_H = int(res2.group(1))
                if res3: self.LAYER1_IN_W = int(res3.group(1))
                if res4: self.STAGE2L_OUT_H = int(res4.group(1))
                if res5: self.STAGE2L_OUT_W = int(res5.group(1))
                if res6: self.STAGE2R_OUT_NUM = int(res6.group(1))
                if res7: self.STAGE2L_OUT_NUM = int(res7.group(1))
                if res8: self.STAGE2R_OUT_H = int(res8.group(1))
                if res9: self.STAGE2R_OUT_W = int(res9.group(1))

        self.original_image_size = self.LAYER1_IN_NUM * self.LAYER1_IN_H * self.LAYER1_IN_W
        self.processed_output_size = self.STAGE2L_OUT_H * self.STAGE2L_OUT_W * (self.STAGE2R_OUT_NUM + self.STAGE2L_OUT_NUM)

    def check(self):
        assert self.original_image_size != 0, 'input image size should > 0'
        assert self.processed_output_size != 0, 'output result size should > 0'

        assert self.STAGE2L_OUT_H != 0 and self.STAGE2L_OUT_H == self.STAGE2R_OUT_H, 'left stage and right stage height should be equal and > 0'
        assert self.STAGE2L_OUT_W != 0 and self.STAGE2L_OUT_W == self.STAGE2R_OUT_W, 'left stage and right stage height should be equal and > 0'



def load_test_input(test_data, c):
    print("INFO: Loading test input from: %{}".format(c.test_input_file_path))
    with open(c.test_input_file_path, 'rb') as fobj:
        for i in range(c.original_image_size):
            (num,) = struct.unpack('f', fobj.read(4))
            test_data.append(num)

def load_expect_output(expect_data, c):
    print("INFO: Loading test input from: %{}".format(c.expect_output_file_path))
    with open(c.expect_output_file_path, 'rb') as fobj:
        for i in range(c.processed_output_size):
            (num,) = struct.unpack('f', fobj.read(4))
            expect_data.append(num)
