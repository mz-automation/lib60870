import sys
import os
import unittest
import logging
from unittest import mock
import datetime
import time
import ctypes

sys.path.insert(1, '../')
from lib60870.lib60870 import *


class QualityDescriptorTest(unittest.TestCase):
    def test_init(self):
        pass

    def test_instanciate_good(self):
        sut = QualityDescriptor(0)
        self.assertEqual(sut, QualityDescriptor.IEC60870_QUALITY_GOOD)

    def test_instanciate_invalid(self):
        sut = QualityDescriptor(128)
        self.assertEqual(sut, QualityDescriptor.IEC60870_QUALITY_INVALID)

    def test_instanciate_invalid_overflow(self):
        sut = QualityDescriptor(128 + 1)
        self.assertEqual(sut, QualityDescriptor.IEC60870_QUALITY_INVALID | QualityDescriptor.IEC60870_QUALITY_OVERFLOW)
        self.assertEqual(str(sut), "129")
        #self.assertEqual(str(sut), "QualityDescriptor.IEC60870_QUALITY_INVALID|IEC60870_QUALITY_OVERFLOW")

    def test_convert_to_c_uint8(self):
        sut = QualityDescriptor(128)
        self.assertIsInstance(sut.c_value, ctypes.c_uint8)

    def test_conversion_from_c_uint8(self):
        value = ctypes.c_uint8(128)
        sut = QualityDescriptor.from_c(value)
        self.assertEqual(sut, QualityDescriptor.IEC60870_QUALITY_INVALID)

if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(name)s:%(levelname)s:%(message)s', level=logging.DEBUG)
    unittest.main()
