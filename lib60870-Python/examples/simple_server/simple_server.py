import logging
import time

from lib60870 import lib60870
from lib60870.lib60870 import IEC60870ConnectionEvent, CauseOfTransmission, QualityDescriptor, TypeID
from lib60870.T104Slave import T104Slave
from lib60870.information_object import *
from lib60870.asdu import ASDU

logging.basicConfig(format='%(asctime)s %(name)s:%(levelname)s:%(message)s', level=logging.INFO)


def clock_sync_callback(parameter, connection, asdu, newtime):
    logging.info("Process time sync command with time {}".format(newtime))
    return True


def connection_request_callback(parameter, conn):
    logging.info("New connection from {}".format(conn))
    return True


def interrogation_callback(parameter, connection, asdu, qoi):
    logging.info("Received interrogation for group {}".format(qoi))
    if qoi == 20:
        connection.send_act_con(asdu)

        new_asdu = ASDU(parameter, TypeID.M_ME_NB_1, False, CauseOfTransmission.INTERROGATED_BY_STATION, 0, 1)
        new_asdu.add_information_object(MeasuredValueScaled(100, -1, QualityDescriptor.IEC60870_QUALITY_GOOD))
        new_asdu.add_information_object(MeasuredValueScaled(101, 23, QualityDescriptor.IEC60870_QUALITY_GOOD))
        new_asdu.add_information_object(MeasuredValueScaled(102, 2300, QualityDescriptor.IEC60870_QUALITY_GOOD))
        connection.send_asdu(new_asdu)

        new_asdu = ASDU(parameter, TypeID.M_SP_NA_1, False, CauseOfTransmission.INTERROGATED_BY_STATION, 0, 1)
        new_asdu.add_information_object(SinglePointInformation(104, True, QualityDescriptor.IEC60870_QUALITY_GOOD))
        new_asdu.add_information_object(SinglePointInformation(105, False, QualityDescriptor.IEC60870_QUALITY_GOOD))
        connection.send_asdu(new_asdu)

        new_asdu = ASDU(parameter, TypeID.M_SP_NA_1, False, CauseOfTransmission.INTERROGATED_BY_STATION,  0, 1)
        new_asdu.add_information_object(SinglePointInformation(301, False, QualityDescriptor.IEC60870_QUALITY_GOOD))
        new_asdu.add_information_object(SinglePointInformation(302, True, QualityDescriptor.IEC60870_QUALITY_GOOD))
        new_asdu.add_information_object(SinglePointInformation(303, False, QualityDescriptor.IEC60870_QUALITY_GOOD))
        new_asdu.add_information_object(SinglePointInformation(304, True, QualityDescriptor.IEC60870_QUALITY_GOOD))
        new_asdu.add_information_object(SinglePointInformation(305, False, QualityDescriptor.IEC60870_QUALITY_GOOD))
        new_asdu.add_information_object(SinglePointInformation(306, True, QualityDescriptor.IEC60870_QUALITY_GOOD))
        new_asdu.add_information_object(SinglePointInformation(307, False, QualityDescriptor.IEC60870_QUALITY_GOOD))
        connection.send_asdu(new_asdu)

        connection.send_act_term(asdu)
    else:
        connection.send_act_con(asdu, True)
    return True


def asdu_callback(parameter, connection, asdu):
    logging.info("Received asdu {}".format(asdu))
    if asdu.get_type_id() == TypeID.C_SC_NA_1:
        logging.info("received single command\n")

        if asdu.get_cot == CauseOfTransmission.ACTIVATION:
            sc = asdu.elements[0]
            if sc.get_object_address() == 5000:
                logging.info("IOA: {} switch to {}".format(sc.get_object_address(), sc.get_state()))
                cot = CauseOfTransmission.ACTIVATION_CON
            else:
                cot = CauseOfTransmission.UNKNOWN_INFORMATION_OBJECT_ADDRESS
        else:
            cot = CauseOfTransmission.UNKNOWN_CAUSE_OF_TRANSMISSION
        asdu.set_cot(cot)
        connection.send_asdu(asdu)
        return True
    else:
        return False


def main():
    t104slave = T104Slave()
    t104slave.set_local_address(ip=b"localhost")
    connectionParameters = t104slave.get_connection_parameters()

    # Set up callbacks
    t104slave.set_connection_request_handler(connection_request_callback)
    t104slave.set_clock_synchronization_handler(clock_sync_callback)
    t104slave.set_interrogation_handler(interrogation_callback)
    t104slave.set_asdu_handler(asdu_callback)

    if not t104slave.start():
        raise RuntimeError("Server not started")

    try:
        scaledValue = 0
        while True:
            time.sleep(1)
            asdu = ASDU(connectionParameters, TypeID.M_ME_NB_1, False, CauseOfTransmission.PERIODIC, 0, 1, False, False)
            asdu.add_information_object(MeasuredValueScaled(110, scaledValue, QualityDescriptor.IEC60870_QUALITY_GOOD))
            scaledValue += 1
            t104slave.enqueue_asdu(asdu)
    except KeyboardInterrupt:
        pass
    finally:
        t104slave.stop()

if __name__ == "__main__":
    main()
