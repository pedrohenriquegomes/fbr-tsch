import socket
import struct

# open socket
socket_handler = socket.socket(socket.AF_INET6,socket.SOCK_DGRAM)
socket_handler.bind(('',2001))

while True:
    
    # wait for a request
    request,dist_addr = socket_handler.recvfrom(1024)
    
    hisAddress     = dist_addr[0]
    hisPort        = dist_addr[1]
    data 		   = struct.unpack('<hh',request)
    lux        	   = data[0]
    rank		   = data[1]
    
    print 'received "{0} {3}" from [{1}]:{2}'.format(lux,hisAddress,hisPort,rank)
