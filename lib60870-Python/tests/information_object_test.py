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

    def test_clone(self):
        clone = self.sut.clone()
        self.assertEqual(clone, self.sut)
        self.assertIsNot(clone, self.sut)


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

    def test_equal(self):
        self.assertEqual(self.sut, self.sut)

    def test_clone(self):
        clone = self.sut.clone()
        self.assertEqual(clone, self.sut)
        self.assertIsNot(clone, self.sut)


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

    def test_equal(self):
        self.assertEqual(self.sut, self.sut)

    def test_clone(self):
        clone = self.sut.clone()
        self.assertEqual(clone, self.sut)
        self.assertIsNot(clone, self.sut)


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

    def test_equal(self):
        self.assertEqual(self.sut, self.sut)

    def test_clone(self):
        clone = self.sut.clone()
        self.assertEqual(clone, self.sut)
        self.assertIsNot(clone, self.sut)


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

    def test_equal(self):
        self.assertEqual(self.sut, self.sut)

    def test_clone(self):
        clone = self.sut.clone()
        self.assertEqual(clone, self.sut)
        self.assertIsNot(clone, self.sut)


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

    def test_equal(self):
        self.assertEqual(self.sut, self.sut)

    def test_clone(self):
        clone = self.sut.clone()
        self.assertEqual(clone, self.sut)
        self.assertIsNot(clone, self.sut)


class SinglePointWithCP56Time2aTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = 0
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        self.timestamp = CP56Time2a()
        #self.sut = SinglePointWithCP56Time2a(ioa, value, quality, timestamp)
        self.sut = SinglePointWithCP56Time2a(ioa, value, quality)

    def test_init(self):
        pass

    def test_pointer(self):
        self.assertIsInstance(self.sut.pointer, pSinglePointWithCP56Time2a)

    def test_initial_values(self):
        self.assertEqual(self.sut.get_type_id(), TypeID.M_SP_TB_1)
        self.assertEqual(self.sut.get_object_address(), 400)
        self.assertEqual(self.sut.get_value(), 0)
        self.assertEqual(self.sut.get_quality(), QualityDescriptor.IEC60870_QUALITY_GOOD)
        self.assertEqual(self.sut.get_timestamp(), self.timestamp)

    def test_clone(self):
        clone = self.sut.clone()
        self.assertEqual(clone, self.sut)
        self.assertIsNot(clone, self.sut)


class SetpointCommandNormalizedTest(unittest.TestCase):
    def setUp(self):
        ioa = 2400
        value = 10
        ql = QualityDescriptor.IEC60870_QUALITY_GOOD
        self.sut = SetpointCommandNormalized(ioa, value, selectCommand=False, ql=ql, as_scaled=True)

    def test_get_scaled(self):
        val = self.sut.get_value(as_scaled=True)
        self.assertEqual(val, 10)



class MeasuredValueNormalizedTest(unittest.TestCase):
    def setUp(self):
        ioa = 2400
        value = 1
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        self.sut = MeasuredValueNormalized(ioa, value, quality)

    def test_init(self):
        pass

    def test_get_value(self):
        self.assertEqual(self.sut.get_value(), 1)
        self.assertEqual(self.sut.get_quality(), QualityDescriptor.IEC60870_QUALITY_GOOD)

    def test_equal(self):
        self.assertEqual(self.sut, self.sut)

    def test_clone(self):
        clone = self.sut.clone()
        self.assertEqual(clone, self.sut)
        self.assertIsNot(clone, self.sut)

    def test_scaled(self):
        self.sut.set_value(200, as_scaled=True)
        val = self.sut.get_value(as_scaled=True)
        self.assertEqual(val, 200)


class SetpointCommandScaledTest(unittest.TestCase):
    def setUp(self):
        ioa = 400
        value = -100
        quality = QualityDescriptor.IEC60870_QUALITY_GOOD
        self.sut = SetpointCommandScaled(ioa, value, selectCommand=False, ql=quality)

    def test_init(self):
        pass

    def test_get_value(self):
        self.assertEqual(self.sut.get_value(), -100)
        self.assertEqual(self.sut.get_ql(), 0)
        self.assertEqual(self.sut.is_select(), False)

    def test_equal(self):
        self.assertEqual(self.sut, self.sut)

    def test_clone(self):
        clone = self.sut.clone()
        self.assertEqual(clone, self.sut)
        self.assertIsNot(clone, self.sut)


class LookupTests(unittest.TestCase):
    def test_lookup_io_type(self):
        type_id = TypeID.M_SP_TB_1
        io_type = get_io_type_from_type_id(type_id)
        self.assertEqual(io_type, SinglePointWithCP56Time2a)

    def test_lookup_type_id(self):
        name = "SinglePointWithCP56Time2a"
        type_id = get_type_id_from_name(name)
        self.assertEqual(type_id, TypeID.M_SP_TB_1)


class TestHasCP56Time2a(unittest.TestCase):
    def test_SinglePointInformation(self):
        self.assertFalse(SinglePointInformation.has_CP56Time2a())

    def test_SinglePointWithCP56Time2a(self):
        self.assertTrue(SinglePointWithCP56Time2a.has_CP56Time2a())

    def test_SinglePointWithCP24Time2a(self):
        self.assertFalse(SinglePointWithCP24Time2a.has_CP56Time2a())

    def test_SingleCommand(self):
        self.assertFalse(SingleCommand.has_CP56Time2a())

    def test_SingleCommandWithCP56Time2a(self):
        self.assertTrue(SingleCommandWithCP56Time2a.has_CP56Time2a())


if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(name)s:%(levelname)s:%(message)s', level=logging.DEBUG)
    unittest.main()
