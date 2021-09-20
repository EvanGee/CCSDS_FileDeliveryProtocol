
from Program import ftp_python
import os

#ftp_python.put_request("pictures/log.txt", "log.c", block=True)

print("RUNNING")
ftp_python.get_request("log.txt", "/home/evan/SAT/CCSDS_FileDeliveryProtocol/logreceived.txt", block=True)
#ftp_python.put_request("pictures/pic.jpeg", "sat_path.c", block=True)

