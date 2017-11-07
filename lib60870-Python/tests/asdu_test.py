import sys
import os
import unittest
import logging
from unittest import mock
import datetime
import time

sys.path.insert(1, '../')
from lib60870.asdu import ASDU, ASDU_create_from_buffer
from lib60870.common import ConnectionParameters, pConnectionParameters
from lib60870.lib60870 import CauseOfTransmission, TypeID
from lib60870.information_object import pMeasuredValueScaled
import ctypes


class ASDUTest(unittest.TestCase):
    def test_init(self):
        sut = ASDU()

    def test_set_test(self):
        sut = ASDU()
        sut.set_test(True)
        self.assertTrue(sut.is_test())

    def test_set_negative(self):
        sut = ASDU()
        sut.set_negative(True)
        self.assertTrue(sut.is_negative())

    def test_get_oa(self):
        sut = ASDU()
        self.assertEqual(sut.get_oa(), -1)

    def test_set_cot(self):
        sut = ASDU()
        sut.set_cot(CauseOfTransmission.ACTIVATION)
        self.assertEqual(sut.get_cot(), CauseOfTransmission.ACTIVATION)

    def test_set_ca(self):
        sut = ASDU()
        sut.set_ca(2)
        self.assertEqual(sut.get_ca(), 2)

    def test_get_type_id(self):
        sut = ASDU()
        self.assertEqual(sut.get_type_id(), TypeID.INVALID)

    def test_is_sequence(self):
        sut = ASDU()
        self.assertFalse(sut.is_sequence())

    def test_get_number_of_elements(self):
        sut = ASDU()
        self.assertEqual(sut.get_number_of_elements(), 0)

    def test_get_element_0_does_not_exist(self):
        sut = ASDU()
        self.assertIsNone(sut.get_element(0))

    def test_create_from_buffer(self):
        parameters = ConnectionParameters(
            sizeOfTypeId=1,
            sizeOfVSQ=1,
            sizeOfCOT=2,
            originatorAddress=0,
            sizeOfCA=2,
            sizeOfIOA=3)
        data = b"d\001\a\000\001\000\000\000\000\024"
        sut = ASDU_create_from_buffer(parameters, data, 10)
        self.assertEqual(sut.get_type_id(), TypeID.C_IC_NA_1)
        self.assertEqual(sut.get_number_of_elements(), 1)
        element_0 = sut.get_upcasted_element(0)
        self.assertEqual(element_0.get_type_id(), TypeID.C_IC_NA_1)
        self.assertEqual(element_0.get_qoi(), 20)

    def test_get_elements(self):
        parameters = ConnectionParameters(
            sizeOfTypeId=1,
            sizeOfVSQ=1,
            sizeOfCOT=2,
            originatorAddress=0,
            sizeOfCA=2,
            sizeOfIOA=3)
        data = b"d\001\a\000\001\000\000\000\000\024"
        sut = ASDU_create_from_buffer(parameters, data, len(data))
        elements = sut.get_elements()
        self.assertEqual(len(elements), 1)

    def test_get_multiple_elements(self):
        parameters = ConnectionParameters(
            sizeOfTypeId=1,
            sizeOfVSQ=1,
            sizeOfCOT=2,
            originatorAddress=0,
            sizeOfCA=2,
            sizeOfIOA=3)
        data = b'\x0b\x83\x03\x00\x01\x00\x01\x00\x00\x00\x80\xf1\n\x00\xf1\xfb\xff\xf1'
        sut = ASDU_create_from_buffer(parameters, data, len(data))
        elements = sut.get_elements()
        self.assertEqual(len(elements), 3)
        for element in elements:
            self.assertEqual(element.get_type_id(), TypeID.M_ME_NB_1)
            self.assertIs(element.get_pointer_type(), pMeasuredValueScaled)


if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(name)s:%(levelname)s:%(message)s', level=logging.DEBUG)
    unittest.main()
