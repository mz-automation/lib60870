import logging
import ctypes
import time
import enum
from ctypes import *

from lib60870.common import *
from lib60870.CP56Time2a import CP56Time2a, pCP56Time2a
from lib60870.asdu import pASDU
from lib60870.information_object import pInformationObject
from lib60870 import lib60870

logger = logging.getLogger(__name__)
lib = lib60870.get_library()

# type aliases
pT104Slave = ctypes.c_void_p
pMasterConnection = ctypes.c_void_p
c_enum = c_int


class ServerMode(enum.Enum):
    SINGLE_REDUNDANCY_GROUP = 0
    CONNECTION_IS_REDUNDANCY_GROUP = 1

    @property
    def c_enum(self):
        return ctypes.c_int(self.value)


class MasterConnection():
    def __init__(self, pointer):
        self._pointer = pointer

    def send_asdu(self, asdu):
        lib.MasterConnection_sendASDU.restype = c_bool
        return lib.MasterConnection_sendASDU(self.pointer, asdu.pointer)

    def send_act_con(self, asdu, negative=False):
        lib.MasterConnection_sendACT_CON.restype = c_bool
        return lib.MasterConnection_sendACT_CON(self.pointer, asdu.pointer, c_bool(negative))

    def send_act_term(self, asdu):
        lib.MasterConnection_sendACT_TERM.restype = c_bool
        return(lib.MasterConnection_sendACT_TERM(self.pointer, asdu.pointer))

    def close(self):
        lib.MasterConnection_close(self.pointer)

    def deactivate(self):
        lib.MasterConnection_deactivate(self.pointer)

    @property
    def pointer(self):
        return pMasterConnection(self._pointer)


class T104Slave():
    def __init__(self, parameters=None, max_low_prio_queue_size=128, max_high_prio_queue_size=128):
        logger.debug("calling T104Slave_create()")
        lib.T104Slave_create.restype = pT104Slave
        self.con = pT104Slave(
            lib.T104Slave_create(
                parameters,
                c_int(max_low_prio_queue_size),
                c_int(max_high_prio_queue_size)
            )
        )

    def __del__(self):
        self.destroy()

    def set_local_address(self, ip):
        logger.debug("calling T104Slave_setLocalAddress()")
        lib.T104Slave_setLocalAddress(self.con, c_char_p(ip))

    def set_local_port(self, port):
        logger.debug("calling T104Slave_setLocalPort()")
        lib.T104Slave_setLocalPort(self.con, c_int(port))

    def get_connection_parameters(self):
        logger.debug("calling Slave_getConnectionParameters()")
        lib.Slave_getConnectionParameters.restype = pConnectionParameters
        return lib.Slave_getConnectionParameters(self.con).contents

    def get_open_connections(self):
        logger.debug("calling T104Slave_getOpenConnections()")
        lib.T104Slave_getOpenConnections.restype = c_int
        return lib.T104Slave_getOpenConnections(self.con)

    def set_max_open_connections(self, num_connections):
        logger.debug("calling T104Slave_setMaxOpenConnections()")
        lib.T104Slave_setMaxOpenConnections(self.con, c_int(num_connections))

    def set_server_mode(self, mode):
        logger.debug("calling T104Slave_setServerMode()")
        lib.T104Slave_setServerMode(self.con, mode.c_value)

    def start(self):
        logger.debug("calling Slave_start()")
        lib.Slave_start(self.con)
        return self.is_running()

    def stop(self):
        logger.debug("calling Slave_stop()")
        lib.Slave_stop(self.con)

    def is_running(self):
        logger.debug("calling Slave_isRunning()")
        lib.Slave_isRunning.restype = c_bool
        return lib.Slave_isRunning(self.con)

    def enqueue_asdu(self, asdu):
        logger.debug("calling Slave_enqueueASDU()")
        lib.Slave_enqueueASDU(self.con, asdu.pointer)

    def destroy(self):
        logger.debug("calling Slave_destroy()")
        lib.Slave_destroy(self.con)

    def set_connection_request_handler(self, callback, parameter=None):
        logger.debug("setting connection request handler")
        ConnectionRequestHandler = ctypes.CFUNCTYPE(c_bool, c_void_p, c_char_p)

        def wrapper(parameter, conn):
            logger.debug("connection request event: {} {}".format(parameter, conn))
            return callback(parameter, conn)

        self._connection_request_handler = ConnectionRequestHandler(wrapper)
        lib.T104Slave_setConnectionRequestHandler(self.con, self._connection_request_handler, parameter)

    def set_interrogation_handler(self, callback, parameter=None):
        logger.debug("setting interrogation callback")
        InterrogationHandler = ctypes.CFUNCTYPE(c_bool, c_void_p, pMasterConnection, pASDU, c_uint8)

        def wrapper(parameter, connection, asdu, qoi):
            logger.debug("interrogation event: {} {} {} {}".format(parameter, connection, asdu, qoi))
            connection = MasterConnection(connection)
            return callback(parameter, connection, asdu.contents, qoi)

        self._interrogation_handler = InterrogationHandler(wrapper)
        lib.Slave_setInterrogationHandler(self.con, self._interrogation_handler, parameter)

    def set_counter_interrogation_handler(self, callback, parameter=None):
        logger.debug("setting counter interrogation callback")
        CounterInterrogationHandler = ctypes.CFUNCTYPE(c_bool, c_void_p, pMasterConnection, pASDU, c_uint8)

        def wrapper(parameter, connection, asdu, qcc):
            logger.debug("counter interrogation event: {} {} {} {}".format(parameter, connection, asdu, qcc))
            connection = MasterConnection(connection)
            return callback(parameter, connection, asdu.contents, qcc)

        self._counter_interrogation_handler = CounterInterrogationHandler(wrapper)
        lib.Slave_setCounterInterrogationHandler(self.con, self._counter_interrogation_handler, parameter)

    def set_read_handler(self, callback, parameter=None):
        """
        Set Handler for read command (C_RD_NA_1 - 102)
        """
        logger.debug("setting read callback")
        ReadHandler = ctypes.CFUNCTYPE(c_bool, c_void_p, pMasterConnection, pASDU, c_uint8)

        def wrapper(parameter, connection, asdu, ioa):
            logger.debug("read event: {} {} {} {}".format(parameter, connection, asdu, ioa))
            connection = MasterConnection(connection)
            return callback(parameter, connection.contents, asdu.contents, ioa)

        self._read_handler = ReadHandler(wrapper)
        lib.Slave_setReadHandler(self.con, self._read_handler, parameter)

    def set_clock_synchronization_handler(self, callback, parameter=None):
        """
        Handler for clock synchronization command (C_CS_NA_1 - 103)
        """
        logger.debug("setting clock synchronization callback")
        ClockSynchronizationHandler = ctypes.CFUNCTYPE(c_bool, c_void_p, pMasterConnection, pASDU, pCP56Time2a)

        def wrapper(parameter, connection, asdu, newtime):
            logger.debug("clock sync event: {} {} {} {}".format(parameter, connection, asdu, newtime))
            connection = MasterConnection(connection)
            return callback(parameter, connection, asdu.contents, newtime.contents)

        self._clock_sync_handler = ClockSynchronizationHandler(wrapper)
        lib.Slave_setClockSyncHandler(self.con, self._clock_sync_handler, parameter)

    def set_asdu_handler(self, callback, parameter=None):
        """
        Handler for ASDUs that are not handled by other handlers (default handler)
        """
        logger.debug("setting asdu callback")
        ASDUHandler = ctypes.CFUNCTYPE(c_bool, c_void_p, pMasterConnection, pASDU)

        def wrapper(parameter, connection, asdu):
            logger.debug("asdu event: {} {} {}".format(parameter, connection, asdu))
            connection = MasterConnection(connection)
            return callback(parameter, connection, asdu.contents)

        self._asdu_handler = ASDUHandler(wrapper)
        lib.Slave_setASDUHandler(self.con, self._asdu_handler, parameter)
