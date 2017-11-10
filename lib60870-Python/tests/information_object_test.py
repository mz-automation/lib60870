import sys
import os
import unittest
import logging
from unittest import mock
import datetime
import time

sys.path.insert(1, '../')
from lib60870.information_object import *
from lib60870.CP24Time2a import *


class SinglePointInformationTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = 2
        self.sut = SinglePointInformation(ioa, value, quality)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), 2)


class SinglePointWithCP24Time2aTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = 2
        timestamp = CP24Time2a()
        self.sut = SinglePointWithCP24Time2a(ioa, value, quality, timestamp)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), 2)
        self.assertEqual(self.sut.get_timestamp(), CP24Time2a())


class DoublePointInformationTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = 2
        self.sut = DoublePointInformation(ioa, value, quality)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), 2)


class DoublePointWithCP24Time2aTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = 2
        timestamp = CP24Time2a()
        self.sut = DoublePointWithCP24Time2a(ioa, value, quality, timestamp)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), 2)
        self.assertEqual(self.sut.get_timestamp(), CP24Time2a())


class StepPositionInformationTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = 2
        is_transient = True
        self.sut = StepPositionInformation(ioa, value, is_transient, quality)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), 2)
        self.assertTrue(self.sut.is_transient())


class StepPositionWithCP24Time2aTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = 2
        is_transient = False
        timestamp = CP24Time2a()
        self.sut = StepPositionWithCP24Time2a(ioa, value, is_transient, quality, timestamp)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), 2)
        self.assertFalse(self.sut.is_transient())
        self.assertEqual(self.sut.get_timestamp(), CP24Time2a())


class SinglePointWithCP56Time2aTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = 2
        timestamp = CP56Time2a()
        self.sut = SinglePointWithCP56Time2a(ioa, value, quality, timestamp)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), 2)
        self.assertEqual(self.sut.get_timestamp(), CP56Time2a())


if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(name)s:%(levelname)s:%(message)s', level=logging.DEBUG)
    unittest.main()
