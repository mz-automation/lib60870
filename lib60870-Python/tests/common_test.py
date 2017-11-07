import sys
import os
import unittest
import logging
from unittest import mock
import datetime
import time

sys.path.insert(1, '../')
from lib60870.CP56Time2a import CP56Time2a
from lib60870.CP24Time2a import CP24Time2a
from lib60870.CP16Time2a import CP16Time2a


class CP56Time2aTest(unittest.TestCase):
    def test_creation_from_0(self):
        ts_ms = 0  # 1970-01-01 00:00:00.000
        sut = CP56Time2a()
        sut.from_timestamp(0)
        # self.assertEqual(sut.to_ms_timestamp(), ts_ms)
        self.assertEqual(sut.get_year(), 70)
        self.assertEqual(sut.get_month(), 1)
        self.assertEqual(sut.get_day_of_month(), 1)
        self.assertEqual(sut.get_hour(), 0)
        self.assertEqual(sut.get_minute(), 0)
        self.assertEqual(sut.get_second(), 0)

    def test_creation_from_2000_01_01(self):
        ts_ms = 946684800000  # 2000-01-01 00:00:00.000
        sut = CP56Time2a(ts_ms)
        self.assertEqual(sut.to_ms_timestamp(), ts_ms)
        self.assertEqual(sut.get_year(), 0)
        self.assertEqual(sut.get_month(), 1)
        self.assertEqual(sut.get_day_of_month(), 1)
        self.assertEqual(sut.get_hour(), 0)
        self.assertEqual(sut.get_minute(), 0)
        self.assertEqual(sut.get_second(), 0)

    def test_creation_from_now(self):
        now_ms = int(time.time() * 1000)
        sut = CP56Time2a(now_ms)
        self.assertEqual(sut.to_ms_timestamp(), now_ms)

    def test_set_year(self):
        ts_ms = 946684800000  # 2000-01-01 00:00:00.000
        sut = CP56Time2a(ts_ms)
        sut.set_year(12)
        self.assertEqual(sut.get_year(), 12)

    def test_set_month(self):
        ts_ms = 946684800000  # 2000-01-01 00:00:00.000
        sut = CP56Time2a(ts_ms)
        sut.set_month(12)
        self.assertEqual(sut.get_month(), 12)

    def test_set_summertime(self):
        ts_ms = 946684800000  # 2000-01-01 00:00:00.000
        sut = CP56Time2a(ts_ms)
        sut.set_summer_time(True)
        self.assertTrue(sut.is_summer_time())


class CP24Time2aTest(unittest.TestCase):
    def test_creation(self):
        sut = CP24Time2a()
        self.assertEqual(sut.get_minute(), 0)
        self.assertEqual(sut.get_second(), 0)
        self.assertEqual(sut.get_millisecond(), 0)

    def test_set_millisecond(self):
        sut = CP24Time2a()
        sut.set_millisecond(999)
        self.assertEqual(999, sut.get_millisecond())
        self.assertEqual(0, sut.get_second())

    def test_set_second(self):
        sut = CP24Time2a()
        sut.set_second(59)
        self.assertEqual(0, sut.get_millisecond())
        self.assertEqual(59, sut.get_second())


class CP16Time2aTest(unittest.TestCase):
    def test_creation(self):
        sut = CP16Time2a()
        self.assertEqual(sut.get_millisecond(), 0)

    def test_set_millisecond(self):
        sut = CP16Time2a()
        sut.set_millisecond(50000)
        self.assertEqual(50000, sut.get_millisecond())


if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(name)s:%(levelname)s:%(message)s', level=logging.DEBUG)
    unittest.main()
