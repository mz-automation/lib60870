import sys
import os
import unittest
import logging
from unittest import mock
import datetime
import time

sys.path.insert(1, '../')
from lib60870.information_object import *
from lib60870.CP16Time2a import CP16Time2a
from lib60870.CP24Time2a import CP24Time2a
from lib60870.CP56Time2a import CP56Time2a
from lib60870.lib60870 import TypeID, QualityDescriptor

class InformationObjectTest(unittest.TestCase):
    def setUp(self):
        ioa = 0
        self.sut = InformationObject(ioa)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_type_id(), TypeID.INVALID)
        self.assertEqual(self.sut.get_object_address(), 0)
        self.assertEqual(self.sut.get_value(), None)
        self.assertEqual(self.sut.get_timestamp(), None)

    def test_equal(self):
        self.assertEqual(InformationObject(40), InformationObject(40))

    def test_not_equal(self):
        self.assertNotEqual(InformationObject(40), InformationObject(39))

class SinglePointInformationTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        self.sut = SinglePointInformation(ioa, value, quality)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_type_id(), TypeID.M_SP_NA_1)
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), QualityDescriptor.IEC60870_QUALITY_GOOD)
        self.assertEqual(self.sut.get_timestamp(), None)


    def test_equal(self):
        self.assertEqual(self.sut, self.sut)

    def test_not_equal(self):
        self.assertNotEqual(self.sut, SinglePointInformation(40, 1, QualityDescriptor.IEC60870_QUALITY_GOOD))

    def test_not_equal_double(self):
        ioa = 400
        value = 0
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        sut = DoublePointInformation(ioa, value, quality)
        self.assertNotEqual(sut, self.sut)


class SinglePointWithCP24Time2aTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        timestamp = CP24Time2a()
        self.sut = SinglePointWithCP24Time2a(ioa, value, quality, timestamp)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_type_id(), TypeID.M_SP_TA_1)
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), QualityDescriptor.IEC60870_QUALITY_GOOD)
        self.assertEqual(self.sut.get_timestamp(), CP24Time2a())


class DoublePointInformationTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        self.sut = DoublePointInformation(ioa, value, quality)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_type_id(), TypeID.M_DP_NA_1)
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), QualityDescriptor.IEC60870_QUALITY_GOOD)


class DoublePointWithCP24Time2aTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        timestamp = CP24Time2a()
        self.sut = DoublePointWithCP24Time2a(ioa, value, quality, timestamp)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_type_id(), TypeID.M_DP_TA_1)
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), QualityDescriptor.IEC60870_QUALITY_GOOD)
        self.assertEqual(self.sut.get_timestamp(), CP24Time2a())


class StepPositionInformationTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        is_transient = True
        self.sut = StepPositionInformation(ioa, value, is_transient, quality)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_type_id(), TypeID.M_ST_NA_1)
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), QualityDescriptor.IEC60870_QUALITY_GOOD)
        self.assertTrue(self.sut.is_transient())


class StepPositionWithCP24Time2aTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        is_transient = False
        timestamp = CP24Time2a()
        self.sut = StepPositionWithCP24Time2a(ioa, value, is_transient, quality, timestamp)

    def test_init(self):
        pass

    def test_initial_values(self):
        self.assertEqual(self.sut.get_type_id(), TypeID.M_ST_TA_1)
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), QualityDescriptor.IEC60870_QUALITY_GOOD)
        self.assertFalse(self.sut.is_transient())
        self.assertEqual(self.sut.get_timestamp(), CP24Time2a())


class SinglePointWithCP56Time2aTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        timestamp = CP56Time2a()
        self.sut = SinglePointWithCP56Time2a(ioa, value, quality, timestamp)

    def test_init(self):
        pass

    def test_pointer(self):
        self.assertIsInstance(self.sut.pointer, pSinglePointWithCP56Time2a)

    def test_initial_values(self):
        self.assertEqual(self.sut.get_type_id(), TypeID.M_SP_TB_1)
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), QualityDescriptor.IEC60870_QUALITY_GOOD)
        self.assertEqual(self.sut.get_timestamp(), CP56Time2a())


if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(name)s:%(levelname)s:%(message)s', level=logging.DEBUG)
    unittest.main()
