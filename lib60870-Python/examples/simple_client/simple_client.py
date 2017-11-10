import logging
import time

from lib60870 import lib60870
from lib60870.lib60870 import IEC60870ConnectionEvent, CauseOfTransmission
from lib60870.T104Connection import T104Connection
from lib60870.information_object import ClockSynchronizationCommand, InformationObject, SingleCommand

logging.basicConfig(format='%(asctime)s %(name)s:%(levelname)s:%(message)s', level=logging.INFO)

ip = b"localhost"


def connection_handler(parameters, event):
    if event == IEC60870ConnectionEvent.IEC60870_CONNECTION_CLOSED:
        logging.info("Disconnected")
    elif event == IEC60870ConnectionEvent.IEC60870_CONNECTION_OPENED:
        logging.info("Connected")
    elif event == IEC60870ConnectionEvent.IEC60870_CONNECTION_STARTDT_CON_RECEIVED:
        logging.info("StartDT con received")
    elif event == IEC60870ConnectionEvent.IEC60870_CONNECTION_STOPDT_CON_RECEIVED:
        logging.info("StopDT con received")


def asdu_received_handler(asdu):
    logging.info(asdu.get_cot())
    for element in asdu.get_elements():
        logging.info(element)
        try:
            logging.info("value: {}".format(element.get_value()))
        except AttributeError:
            pass
    return True


client = T104Connection(ip)
client.set_connection_handler(connection_handler)
client.set_asdu_received_handler(asdu_received_handler)


if client.connect():
    client.send_start_dt()
    time.sleep(5)
    client.send_interrogation_command(ca=1)
    time.sleep(5)
    command = SingleCommand(5000, True, False, 0)
    client.send_control_command(cot=CauseOfTransmission.ACTIVATION, ca=1, command=command)
    client.send_clock_sync_command(ca=1)
    time.sleep(1)
    client.disconnect()
else:
    logging.info("Not connected")
