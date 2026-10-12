import fcntl
import socket
import struct

IP_ADDRESS_INTERFACES = ("wlan0", "eth0")
SIOCGIFADDR = 0x8915


def get_ip_address(interfaces):
  for interface in interfaces:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
      try:
        return socket.inet_ntoa(fcntl.ioctl(sock.fileno(), SIOCGIFADDR, struct.pack("256s", interface.encode()))[20:24])
      except OSError:
        continue
  return None
