import logging
import math

import slidingwindow as sw

import cv2
import numpy as np
import tensorflow as tf
import time

from tf_pose import common
from tf_pose.common import CocoPart
from tf_pose.tensblur.smoother import Smoother
from tensorflow.python.client import timeline

from tf_pose import sacc_utils 


try:
    from tf_pose.pafprocess import pafprocess
except ModuleNotFoundError as e:
    print(e)
    print('you need to build c++ library for pafprocess. See : https://github.com/ildoonet/tf-pose-estimation/tree/master/tf_pose/pafprocess')
    exit(-1)

logger = logging.getLogger('TfPoseEstimator')
logger.handlers.clear()
logger.setLevel(logging.INFO)
ch = logging.StreamHandler()
formatter = logging.Formatter('[%(asctime)s] [%(name)s] [%(levelname)s] %(message)s')
ch.setFormatter(formatter)
logger.addHandler(ch)
logger.setLevel(logging.INFO)


def _round(v):
    return int(round(v))


def _include_part(part_list, part_idx):
    for part in part_list:
        if part_idx == part.part_idx:
            return True, part
    return False, None


class Human:
    """
    body_parts: list of BodyPart
    """
    __slots__ = ('body_parts', 'pairs', 'uidx_list', 'score')

    def __init__(self, pairs):
        self.pairs = []
        self.uidx_list = set()
        self.body_parts = {}
        for pair in pairs:
            self.add_pair(pair)
        self.score = 0.0

    @staticmethod
    def _get_uidx(part_idx, idx):
        return '%d-%d' % (part_idx, idx)

    def add_pair(self, pair):
        self.pairs.append(pair)
        self.body_parts[pair.part_idx1] = BodyPart(Human._get_uidx(pair.part_idx1, pair.idx1),
                                                   pair.part_idx1,
                                                   pair.coord1[0], pair.coord1[1], pair.score)
        self.body_parts[pair.part_idx2] = BodyPart(Human._get_uidx(pair.part_idx2, pair.idx2),
                                                   pair.part_idx2,
                                                   pair.coord2[0], pair.coord2[1], pair.score)
        self.uidx_list.add(Human._get_uidx(pair.part_idx1, pair.idx1))
        self.uidx_list.add(Human._get_uidx(pair.part_idx2, pair.idx2))

    def is_connected(self, other):
        return len(self.uidx_list & other.uidx_list) > 0

    def merge(self, other):
        for pair in other.pairs:
            self.add_pair(pair)

    def part_count(self):
        return len(self.body_parts.keys())

    def get_max_score(self):
        return max([x.score for _, x in self.body_parts.items()])

    def get_face_box(self, img_w, img_h, mode=0):
        """
        Get Face box compared to img size (w, h)
        :param img_w:
        :param img_h:
        :param mode:
        :return:
        """
        # SEE : https://github.com/ildoonet/tf-pose-estimation/blob/master/tf_pose/common.py#L13
        _NOSE = CocoPart.Nose.value
        _NECK = CocoPart.Neck.value
        _REye = CocoPart.REye.value
        _LEye = CocoPart.LEye.value
        _REar = CocoPart.REar.value
        _LEar = CocoPart.LEar.value

        _THRESHOLD_PART_CONFIDENCE = 0.2
        parts = [part for idx, part in self.body_parts.items() if part.score > _THRESHOLD_PART_CONFIDENCE]

        is_nose, part_nose = _include_part(parts, _NOSE)
        if not is_nose:
            return None

        size = 0
        is_neck, part_neck = _include_part(parts, _NECK)
        if is_neck:
            size = max(size, img_h * (part_neck.y - part_nose.y) * 0.8)

        is_reye, part_reye = _include_part(parts, _REye)
        is_leye, part_leye = _include_part(parts, _LEye)
        if is_reye and is_leye:
            size = max(size, img_w * (part_reye.x - part_leye.x) * 2.0)
            size = max(size,
                       img_w * math.sqrt((part_reye.x - part_leye.x) ** 2 + (part_reye.y - part_leye.y) ** 2) * 2.0)

        if mode == 1:
            if not is_reye and not is_leye:
                return None

        is_rear, part_rear = _include_part(parts, _REar)
        is_lear, part_lear = _include_part(parts, _LEar)
        if is_rear and is_lear:
            size = max(size, img_w * (part_rear.x - part_lear.x) * 1.6)

        if size <= 0:
            return None

        if not is_reye and is_leye:
            x = part_nose.x * img_w - (size // 3 * 2)
        elif is_reye and not is_leye:
            x = part_nose.x * img_w - (size // 3)
        else:  # is_reye and is_leye:
            x = part_nose.x * img_w - size // 2

        x2 = x + size
        if mode == 0:
            y = part_nose.y * img_h - size // 3
        else:
            y = part_nose.y * img_h - _round(size / 2 * 1.2)
        y2 = y + size

        # fit into the image frame
        x = max(0, x)
        y = max(0, y)
        x2 = min(img_w - x, x2 - x) + x
        y2 = min(img_h - y, y2 - y) + y

        if _round(x2 - x) == 0.0 or _round(y2 - y) == 0.0:
            return None
        if mode == 0:
            return {"x": _round((x + x2) / 2),
                    "y": _round((y + y2) / 2),
                    "w": _round(x2 - x),
                    "h": _round(y2 - y)}
        else:
            return {"x": _round(x),
                    "y": _round(y),
                    "w": _round(x2 - x),
                    "h": _round(y2 - y)}

    def get_upper_body_box(self, img_w, img_h):
        """
        Get Upper body box compared to img size (w, h)
        :param img_w:
        :param img_h:
        :return:
        """

        if not (img_w > 0 and img_h > 0):
            raise Exception("img size should be positive")

        _NOSE = CocoPart.Nose.value
        _NECK = CocoPart.Neck.value
        _RSHOULDER = CocoPart.RShoulder.value
        _LSHOULDER = CocoPart.LShoulder.value
        _THRESHOLD_PART_CONFIDENCE = 0.3
        parts = [part for idx, part in self.body_parts.items() if part.score > _THRESHOLD_PART_CONFIDENCE]
        part_coords = [(img_w * part.x, img_h * part.y) for part in parts if
                       part.part_idx in [0, 1, 2, 5, 8, 11, 14, 15, 16, 17]]

        if len(part_coords) < 5:
            return None

        # Initial Bounding Box
        x = min([part[0] for part in part_coords])
        y = min([part[1] for part in part_coords])
        x2 = max([part[0] for part in part_coords])
        y2 = max([part[1] for part in part_coords])

        # # ------ Adjust heuristically +
        # if face points are detcted, adjust y value

        is_nose, part_nose = _include_part(parts, _NOSE)
        is_neck, part_neck = _include_part(parts, _NECK)
        torso_height = 0
        if is_nose and is_neck:
            y -= (part_neck.y * img_h - y) * 0.8
            torso_height = max(0, (part_neck.y - part_nose.y) * img_h * 2.5)
        #
        # # by using shoulder position, adjust width
        is_rshoulder, part_rshoulder = _include_part(parts, _RSHOULDER)
        is_lshoulder, part_lshoulder = _include_part(parts, _LSHOULDER)
        if is_rshoulder and is_lshoulder:
            half_w = x2 - x
            dx = half_w * 0.15
            x -= dx
            x2 += dx
        elif is_neck:
            if is_lshoulder and not is_rshoulder:
                half_w = abs(part_lshoulder.x - part_neck.x) * img_w * 1.15
                x = min(part_neck.x * img_w - half_w, x)
                x2 = max(part_neck.x * img_w + half_w, x2)
            elif not is_lshoulder and is_rshoulder:
                half_w = abs(part_rshoulder.x - part_neck.x) * img_w * 1.15
                x = min(part_neck.x * img_w - half_w, x)
                x2 = max(part_neck.x * img_w + half_w, x2)

        # ------ Adjust heuristically -

        # fit into the image frame
        x = max(0, x)
        y = max(0, y)
        x2 = min(img_w - x, x2 - x) + x
        y2 = min(img_h - y, y2 - y) + y

        if _round(x2 - x) == 0.0 or _round(y2 - y) == 0.0:
            return None
        return {"x": _round((x + x2) / 2),
                "y": _round((y + y2) / 2),
                "w": _round(x2 - x),
                "h": _round(y2 - y)}

    def __str__(self):
        return ' '.join([str(x) for x in self.body_parts.values()])

    def __repr__(self):
        return self.__str__()


class BodyPart:
    """
    part_idx : part index(eg. 0 for nose)
    x, y: coordinate of body part
    score : confidence score
    """
    __slots__ = ('uidx', 'part_idx', 'x', 'y', 'score')

    def __init__(self, uidx, part_idx, x, y, score):
        self.uidx = uidx
        self.part_idx = part_idx
        self.x, self.y = x, y
        self.score = score

    def get_part_name(self):
        return CocoPart(self.part_idx)

    def __str__(self):
        return 'BodyPart:%d-(%.2f, %.2f) score=%.2f' % (self.part_idx, self.x, self.y, self.score)

    def __repr__(self):
        return self.__str__()


class PoseEstimator:
    def __init__(self):
        pass

    @staticmethod
    def estimate_paf(peaks, heat_mat, paf_mat):
        pafprocess.process_paf(peaks, heat_mat, paf_mat)

        humans = []
        for human_id in range(pafprocess.get_num_humans()):
            human = Human([])
            is_added = False

            for part_idx in range(18):
                c_idx = int(pafprocess.get_part_cid(human_id, part_idx))
                if c_idx < 0:
                    continue

                is_added = True
                human.body_parts[part_idx] = BodyPart(
                    '%d-%d' % (human_id, part_idx), part_idx,
                    float(pafprocess.get_part_x(c_idx)) / heat_mat.shape[1],
                    float(pafprocess.get_part_y(c_idx)) / heat_mat.shape[0],
                    pafprocess.get_part_score(c_idx)
                )

            if is_added:
                score = pafprocess.get_score(human_id)
                human.score = score
                humans.append(human)

        return humans


class TfPoseEstimator:
    # TODO : multi-scale

    def __init__(self, graph_path, target_size=(320, 240), tf_config=None):
        self.target_size = target_size

        # load graph
        logger.info('loading graph from %s(default size=%dx%d)' % (graph_path, target_size[0], target_size[1]))
        with tf.gfile.GFile(graph_path, 'rb') as f:
            graph_def = tf.GraphDef()
            graph_def.ParseFromString(f.read())

        self.graph = tf.get_default_graph()
        tf.import_graph_def(graph_def, name='TfPoseEstimator')
        self.persistent_sess = tf.Session(graph=self.graph, config=tf_config)

        # for op in self.graph.get_operations():
        #     print(op.name)
        # for ts in [n.name for n in tf.get_default_graph().as_graph_def().node]:
        #     print(ts)

        self.tensor_image = self.graph.get_tensor_by_name('TfPoseEstimator/image:0')
        self.tensor_output = self.graph.get_tensor_by_name('TfPoseEstimator/Openpose/concat_stage7:0')
        self.tensor_heatMat = self.tensor_output[:, :, :, :19]
        self.tensor_pafMat = self.tensor_output[:, :, :, 19:]
        self.upsample_size = tf.placeholder(dtype=tf.int32, shape=(2,), name='upsample_size')
        self.tensor_heatMat_up = tf.image.resize_area(self.tensor_output[:, :, :, :19], self.upsample_size,
                                                      align_corners=False, name='upsample_heatmat')
        self.tensor_pafMat_up = tf.image.resize_area(self.tensor_output[:, :, :, 19:], self.upsample_size,
                                                     align_corners=False, name='upsample_pafmat')
        # peak calculation part
        # smoother = Smoother({'data': self.tensor_heatMat_up}, 25, 3.0)
        smoother = Smoother({'data': self.tensor_heatMat_up}, 5, 3.0)
        gaussian_heatMat = smoother.get_output()

        max_pooled_in_tensor = tf.nn.pool(gaussian_heatMat, window_shape=(3, 3), pooling_type='MAX', padding='SAME')
        self.tensor_peaks = tf.where(tf.equal(gaussian_heatMat, max_pooled_in_tensor), gaussian_heatMat,
                                     tf.zeros_like(gaussian_heatMat))

        self.heatMat = self.pafMat = None

        # warm-up
        self.persistent_sess.run(tf.variables_initializer(
            [v for v in tf.global_variables() if
             v.name.split(':')[0] in [x.decode('utf-8') for x in
                                      self.persistent_sess.run(tf.report_uninitialized_variables())]
             ])
        )
        self.persistent_sess.run(
            [self.tensor_peaks, self.tensor_heatMat_up, self.tensor_pafMat_up],
            feed_dict={
                self.tensor_image: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.upsample_size: [target_size[1], target_size[0]]
            }
        )
        self.persistent_sess.run(
            [self.tensor_peaks, self.tensor_heatMat_up, self.tensor_pafMat_up],
            feed_dict={
                self.tensor_image: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.upsample_size: [target_size[1] // 2, target_size[0] // 2]
            }
        )
        self.persistent_sess.run(
            [self.tensor_peaks, self.tensor_heatMat_up, self.tensor_pafMat_up],
            feed_dict={
                self.tensor_image: [np.ndarray(shape=(target_size[1], target_size[0], 3), dtype=np.float32)],
                self.upsample_size: [target_size[1] // 4, target_size[0] // 4]
            }
        )

        # logs
        if self.tensor_image.dtype == tf.quint8:
            logger.info('quantization mode enabled.')

    def __del__(self):
        # self.persistent_sess.close()
        pass

    def get_flops(self):
        flops = tf.profiler.profile(self.graph, options=tf.profiler.ProfileOptionBuilder.float_operation())
        return flops.total_float_ops

    @staticmethod
    def _quantize_img(npimg):
        npimg_q = npimg + 1.0
        npimg_q /= (2.0 / 2 ** 8)
        # npimg_q += 0.5
        npimg_q = npimg_q.astype(np.uint8)
        return npimg_q

    @staticmethod
    def draw_humans(npimg, humans, imgcopy=False):
        if imgcopy:
            npimg = np.copy(npimg)
        image_h, image_w = npimg.shape[:2]
        centers = {}
        for human in humans:
            # draw point
            for i in range(common.CocoPart.Background.value):
                if i not in human.body_parts.keys():
                    continue

                body_part = human.body_parts[i]
                center = (int(body_part.x * image_w + 0.5), int(body_part.y * image_h + 0.5))
                centers[i] = center
                cv2.circle(npimg, center, 3, common.CocoColors[i], thickness=3, lineType=8, shift=0)

            # draw line
            for pair_order, pair in enumerate(common.CocoPairsRender):
                if pair[0] not in human.body_parts.keys() or pair[1] not in human.body_parts.keys():
                    continue

                # npimg = cv2.line(npimg, centers[pair[0]], centers[pair[1]], common.CocoColors[pair_order], 3)
                cv2.line(npimg, centers[pair[0]], centers[pair[1]], common.CocoColors[pair_order], 3)

        return npimg

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

    def inference(self, npimg, resize_to_default=True, upsample_size=1.0):
        if npimg is None:
            raise Exception('The image is not valid. Please check your image exists.')

        if resize_to_default:
            upsample_size = [int(self.target_size[1] / 8 * upsample_size), int(self.target_size[0] / 8 * upsample_size)]
        else:
            upsample_size = [int(npimg.shape[0] / 8 * upsample_size), int(npimg.shape[1] / 8 * upsample_size)]

        if self.tensor_image.dtype == tf.quint8:
            # quantize input image
            npimg = TfPoseEstimator._quantize_img(npimg)
            pass

        logger.debug('inference+ original shape=%dx%d' % (npimg.shape[1], npimg.shape[0]))
        
        img = npimg
        if resize_to_default:
            img = self._get_scaled_img(npimg, None)[0][0]
            
        t = time.time()
        run_options = tf.RunOptions(trace_level=tf.RunOptions.FULL_TRACE)
        run_metadata = tf.RunMetadata()
        peaks, heatMat_up, pafMat_up = self.persistent_sess.run(
            [self.tensor_peaks, self.tensor_heatMat_up, self.tensor_pafMat_up], feed_dict={
                self.tensor_image: [img], self.upsample_size: upsample_size
            }, options=run_options, run_metadata=run_metadata)
        
        tl = timeline.Timeline(run_metadata.step_stats)
        ctf = tl.generate_chrome_trace_format()
        with open('timeline.json', 'w') as f:
          f.write(ctf)

        peaks = peaks[0]
        self.heatMat = heatMat_up[0]
        self.pafMat = pafMat_up[0]
        logger.debug('inference- heatMat=%dx%d pafMat=%dx%d' % (
            self.heatMat.shape[1], self.heatMat.shape[0], self.pafMat.shape[1], self.pafMat.shape[0]))

        t = time.time()
        humans = PoseEstimator.estimate_paf(peaks, self.heatMat, self.pafMat)
        logger.debug('estimate time=%.5f' % (time.time() - t))
        return humans

class TfPoseEstimatorSacc(TfPoseEstimator):
    # TODO : multi-scale

    def __init__(self, graph_path, target_size=(320, 240), tf_config=None):

        self.target_size = target_size
        self.constants = sacc_utils.Constants()
        self.persistent_sess = tf.Session()
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
        
        ###############
        
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
                self.upsample_size: [target_size[1], target_size[0]],
                self.upsample_size2: [target_size[1], target_size[0]],
                self.upsample_size3: [target_size[1], target_size[0]],
                self.upsample_size4: [target_size[1], target_size[0]],
                self.upsample_size5: [target_size[1], target_size[0]],
                self.upsample_size6: [target_size[1], target_size[0]],
                self.upsample_size7: [target_size[1], target_size[0]],
                self.upsample_size8: [target_size[1], target_size[0]],
                self.upsample_size0: [target_size[1], target_size[0]],
                self.upsample_size20: [target_size[1], target_size[0]],
                self.upsample_size30: [target_size[1], target_size[0]],
                self.upsample_size40: [target_size[1], target_size[0]],
                self.upsample_size50: [target_size[1], target_size[0]],
                self.upsample_size60: [target_size[1], target_size[0]],
                self.upsample_size70: [target_size[1], target_size[0]],
                self.upsample_size80: [target_size[1], target_size[0]]
            }
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
                self.upsample_size: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size2: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size3: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size4: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size5: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size6: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size7: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size8: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size0: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size20: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size30: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size40: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size50: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size60: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size70: [target_size[1] // 4, target_size[0] // 4],
                self.upsample_size80: [target_size[1] // 4, target_size[0] // 4]
            }
        )
        

        # logs
        if self.tensor_image.dtype == tf.quint8:
            logger.info('quantization mode enabled.')

    def __del__(self):
        # self.persistent_sess.close()
        pass

    def get_flops(self):
        pass # hide superclass func
        #flops = tf.profiler.profile(self.graph, options=tf.profiler.ProfileOptionBuilder.float_operation())
        #return flops.total_float_ops
        
    def inference(self, img, img2, img3, img4, img5, img6, img7, img8, img0, img20, img30, img40, img50, img60, img70, img80, resize_to_default=True, upsample_size=1.0):
        #start = time.time()
        if img is None:
            raise Exception('The image is not valid. Please check your image exists.')

        if resize_to_default:
            upsample_size = [int(self.target_size[1] / 8 * upsample_size), int(self.target_size[0] / 8 * upsample_size)]
        else:
            upsample_size = [int(npimg.shape[0] / 8 * upsample_size), int(npimg.shape[1] / 8 * upsample_size)]

        if self.tensor_image.dtype == tf.quint8:
            # quantize input image
            img = TfPoseEstimator._quantize_img(img)
            img2 = TfPoseEstimator._quantize_img(img2)
            img3 = TfPoseEstimator._quantize_img(img3)
            img4 = TfPoseEstimator._quantize_img(img4)
            img5 = TfPoseEstimator._quantize_img(img5)
            img6 = TfPoseEstimator._quantize_img(img6)
            img7 = TfPoseEstimator._quantize_img(img7)
            img8 = TfPoseEstimator._quantize_img(img8)
            img0 = TfPoseEstimator._quantize_img(img0)
            img20 = TfPoseEstimator._quantize_img(img20)
            img30 = TfPoseEstimator._quantize_img(img30)
            img40 = TfPoseEstimator._quantize_img(img40)
            img50 = TfPoseEstimator._quantize_img(img50)
            img60 = TfPoseEstimator._quantize_img(img60)
            img70 = TfPoseEstimator._quantize_img(img70)
            img80 = TfPoseEstimator._quantize_img(img80)
            pass

        logger.debug('inference+ original shape=%dx%d' % (img.shape[1], img.shape[0]))
        

        peaks80, heatMat_up80, pafMat_up80, peaks70, heatMat_up70, pafMat_up70, peaks60, heatMat_up60, pafMat_up60, peaks50, heatMat_up50, pafMat_up50, peaks40, heatMat_up40, pafMat_up40, peaks30, heatMat_up30, pafMat_up30, peaks20, heatMat_up20, pafMat_up20, peaks0, heatMat_up0, pafMat_up0, peaks8, heatMat_up8, pafMat_up8, peaks7, heatMat_up7, pafMat_up7, peaks6, heatMat_up6, pafMat_up6, peaks5, heatMat_up5, pafMat_up5, peaks4, heatMat_up4, pafMat_up4, peaks3, heatMat_up3, pafMat_up3, peaks2, heatMat_up2, pafMat_up2, peaks, heatMat_up, pafMat_up = self.persistent_sess.run( 
            [self.tensor_peaks80, self.tensor_heatMat_up80, self.tensor_pafMat_up80, self.tensor_peaks70, self.tensor_heatMat_up70, self.tensor_pafMat_up70, self.tensor_peaks60, self.tensor_heatMat_up60, self.tensor_pafMat_up60, self.tensor_peaks50, self.tensor_heatMat_up50, self.tensor_pafMat_up50, self.tensor_peaks40, self.tensor_heatMat_up40, self.tensor_pafMat_up40, self.tensor_peaks30, self.tensor_heatMat_up30, self.tensor_pafMat_up30, self.tensor_peaks20, self.tensor_heatMat_up20, self.tensor_pafMat_up20,self.tensor_peaks0, self.tensor_heatMat_up0, self.tensor_pafMat_up0, self.tensor_peaks8, self.tensor_heatMat_up8, self.tensor_pafMat_up8, self.tensor_peaks7, self.tensor_heatMat_up7, self.tensor_pafMat_up7, self.tensor_peaks6, self.tensor_heatMat_up6, self.tensor_pafMat_up6, self.tensor_peaks5, self.tensor_heatMat_up5, self.tensor_pafMat_up5, self.tensor_peaks4, self.tensor_heatMat_up4, self.tensor_pafMat_up4, self.tensor_peaks3, self.tensor_heatMat_up3, self.tensor_pafMat_up3, self.tensor_peaks2, self.tensor_heatMat_up2, self.tensor_pafMat_up2, self.tensor_peaks, self.tensor_heatMat_up, self.tensor_pafMat_up], feed_dict={
                self.tensor_image: [img], self.tensor_image2: [img2],self.tensor_image3: [img3], self.tensor_image4: [img4], self.tensor_image5: [img5], self.tensor_image6: [img6], self.tensor_image7: [img7], self.tensor_image8: [img8], self.tensor_image0: [img0], self.tensor_image20: [img20],self.tensor_image30: [img30], self.tensor_image40: [img40], self.tensor_image50: [img50], self.tensor_image60: [img60], self.tensor_image70: [img70], self.tensor_image80: [img80], self.upsample_size: upsample_size, self.upsample_size2: upsample_size, self.upsample_size3: upsample_size, self.upsample_size4: upsample_size,self.upsample_size5: upsample_size,self.upsample_size6: upsample_size,self.upsample_size7: upsample_size,self.upsample_size8: upsample_size, self.upsample_size0: upsample_size, self.upsample_size20: upsample_size, self.upsample_size30: upsample_size, self.upsample_size40: upsample_size,self.upsample_size50: upsample_size,self.upsample_size60: upsample_size,self.upsample_size70: upsample_size,self.upsample_size80: upsample_size
            })
       
        peaks = peaks[0]
        self.heatMat = heatMat_up[0]
        self.pafMat = pafMat_up[0]
        
        peaks2 = peaks2[0]
        self.heatMat2 = heatMat_up2[0]
        self.pafMat2 = pafMat_up2[0]
        
        peaks3 = peaks3[0]
        self.heatMat3 = heatMat_up3[0]
        self.pafMat3 = pafMat_up3[0]
        
        peaks4 = peaks4[0]
        self.heatMat4 = heatMat_up4[0]
        self.pafMat4 = pafMat_up4[0]
        
        peaks5 = peaks5[0]
        self.heatMat5 = heatMat_up5[0]
        self.pafMat5 = pafMat_up5[0]
        
        peaks6 = peaks6[0]
        self.heatMat6 = heatMat_up6[0]
        self.pafMat6 = pafMat_up6[0]
        
        peaks8 = peaks8[0]
        self.heatMat8 = heatMat_up8[0]
        self.pafMat8 = pafMat_up8[0]
        
        peaks7 = peaks7[0]
        self.heatMat7 = heatMat_up7[0]
        self.pafMat7 = pafMat_up7[0]
        
        peaks0 = peaks0[0]
        self.heatMat0 = heatMat_up0[0]
        self.pafMat0 = pafMat_up0[0]
        
        peaks20 = peaks20[0]
        self.heatMat20 = heatMat_up20[0]
        self.pafMat20 = pafMat_up20[0]
        
        peaks30 = peaks30[0]
        self.heatMat30 = heatMat_up30[0]
        self.pafMat30 = pafMat_up30[0]
        
        peaks40 = peaks40[0]
        self.heatMat40 = heatMat_up40[0]
        self.pafMat40 = pafMat_up40[0]
        
        peaks50 = peaks50[0]
        self.heatMat50 = heatMat_up50[0]
        self.pafMat50 = pafMat_up50[0]
        
        peaks60 = peaks60[0]
        self.heatMat60 = heatMat_up60[0]
        self.pafMat60 = pafMat_up60[0]
        
        peaks80 = peaks80[0]
        self.heatMat80 = heatMat_up80[0]
        self.pafMat80 = pafMat_up80[0]
        
        peaks70 = peaks70[0]
        self.heatMat70 = heatMat_up70[0]
        self.pafMat70 = pafMat_up70[0]
        logger.debug('inference- heatMat=%dx%d pafMat=%dx%d' % (
            self.heatMat.shape[1], self.heatMat.shape[0], self.pafMat.shape[1], self.pafMat.shape[0]))

        t = time.time()
        logger.debug('estimate time=%.5f' % (time.time() - t))
        return peaks, self.heatMat, self.pafMat, peaks2, self.heatMat2, self.pafMat2, peaks3, self.heatMat3, self.pafMat3, peaks4, self.heatMat4, self.pafMat4, peaks5, self.heatMat5, self.pafMat5, peaks6, self.heatMat6, self.pafMat6, peaks7, self.heatMat7, self.pafMat7, peaks8, self.heatMat8, self.pafMat8, peaks0, self.heatMat0, self.pafMat0, peaks20, self.heatMat20, self.pafMat20, peaks30, self.heatMat30, self.pafMat30, peaks40, self.heatMat40, self.pafMat40, peaks50, self.heatMat50, self.pafMat50, peaks60, self.heatMat60, self.pafMat60, peaks70, self.heatMat70, self.pafMat70, peaks80, self.heatMat80, self.pafMat80
       
     



if __name__ == '__main__':
    import pickle

    f = open('./etcs/heatpaf1.pkl', 'rb')
    data = pickle.load(f)
    logger.info('size={}'.format(data['heatMat'].shape))
    f.close()

    t = time.time()
    humans = PoseEstimator.estimate_paf(data['peaks'], data['heatMat'], data['pafMat'])
    dt = time.time() - t;
    t = time.time()
    logger.info('elapsed #humans=%d time=%.8f' % (len(humans), dt))
