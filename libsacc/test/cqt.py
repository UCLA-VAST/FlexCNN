import tensorflow as tf
from functools import reduce
import utils

class SaccTest(tf.test.TestCase):
  def SaccTest(self):
    c = utils.Constants()
    c.check()    
    expect_output = []
    test_input = []
    utils.load_test_input(test_input, c)
    self.assertEqual(c.original_image_size, len(test_input))
    utils.load_expect_output(expect_output, c)
    sacc_module = tf.load_op_library(c.custom_lib_path)
    with self.test_session():
      engine = sacc_module.sacc(test_input)
      result = engine.eval()
      # self.assertEqual(c.processed_output_size, reduce((lambda x, y: x * y), list(result.shape)))
      # self.assertAllCloseAccordingToType(expect_output, result.reshape(c.processed_output_size,), float_rtol=1e-03, float_atol=1e-03)

if __name__ == "__main__":
    tf.test.main()



