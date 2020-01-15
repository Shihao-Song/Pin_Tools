import numpy as np
import pandas as pd
from keras.utils import np_utils
np.random.seed(10)

# Import MNIST
from keras.datasets import mnist
(X_train_image, y_train_label), (X_test_image, y_test_label) = mnist.load_data()
