from ROI import roi_begin as roi_begin
from ROI import roi_end as roi_end
import numpy as np
import pandas as pd
from keras.utils import np_utils
np.random.seed(10)

# Import MNIST
from keras.datasets import mnist
(train_image, train_label), (test_image, test_label) = mnist.load_data()

print('train data size: ', len(train_image))
print('test data size: ', len(test_image))

print('train image: ', train_image.shape)
print('train label: ', train_label.shape)

train_image_norm = train_image.reshape(60000, 784).astype('float32') / 255
test_image_norm = test_image.reshape(10000, 784).astype('float32') / 255
print('train image norm: ', train_image_norm.shape)
print('test image norm: ', test_image_norm.shape)

train_label_one_hot = np_utils.to_categorical(train_label)
test_label_one_hot = np_utils.to_categorical(test_label)


