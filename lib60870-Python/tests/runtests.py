import unittest
import logging

if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(name)s:%(levelname)s:%(message)s', level=logging.CRITICAL)
    testsuite = unittest.TestLoader().discover('.', pattern="*_test.py")
    unittest.TextTestRunner(verbosity=1).run(testsuite)
