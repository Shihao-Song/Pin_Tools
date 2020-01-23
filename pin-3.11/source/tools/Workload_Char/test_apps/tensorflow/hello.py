# Disable GPU (we focus on the CPU side simulation)
import os
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

from ROI import roi_begin as roi_begin
from ROI import roi_end as roi_end
import tensorflow as tf

roi_begin()
hello = tf.constant("Hello, TensorFlow!")
sess = tf.Session()
print(sess.run(hello))
roi_end()
