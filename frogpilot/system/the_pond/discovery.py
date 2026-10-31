import socket
import struct
import time

from openpilot.common.swaglog import cloudlog

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

IP_ADDRESS_INTERFACES = ("wlan0", "eth0")

MDNS_CHECK_INTERVAL = 10
MDNS_GROUP = "224.0.0.251"
MDNS_PORT = 5353
MDNS_QUERY_ATTEMPTS = 3
MDNS_QUERY_TIMEOUT = 1


def is_hostname_taken(name):
  query = struct.pack(">HHHHHH", 1, 0, 1, 0, 0, 0) + name + struct.pack(">HH", 1, 1)

  with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.settimeout(MDNS_QUERY_TIMEOUT)
    for _ in range(MDNS_QUERY_ATTEMPTS):
      sock.sendto(query, (MDNS_GROUP, MDNS_PORT))
      try:
        while True:
          data, source = sock.recvfrom(2048)
          if len(data) >= 12 and data[2] & 0x80 and name.lower() in data[12:].lower() and local_ip_for(source[0]) != source[0]:
            return True
      except TimeoutError:
        continue
  return False


def local_ip_for(address):
  with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    try:
      sock.connect((address, MDNS_PORT))
      return sock.getsockname()[0]
    except OSError:
      return None


def mdns_responder():
  name = b"".join(bytes([len(label)]) + label.encode("ascii") for label in frogpilot_variables.THE_POND_HOSTNAME.split(".")) + b"\0"
  header = struct.pack(">HHHHHH", 0, 0x8400, 0, 1, 0, 0)
  record = name + struct.pack(">HHIH", 1, 0x8001, 120, 4)

  claimed_address = None
  next_check = 0

  try:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
      sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
      sock.bind(("", MDNS_PORT))

      while True:
        try:
          if time.monotonic() >= next_check:
            next_check = time.monotonic() + MDNS_CHECK_INTERVAL

            for index, _ in socket.if_nameindex():
              try:
                sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, struct.pack("=4s4si", socket.inet_aton(MDNS_GROUP), socket.inet_aton("0.0.0.0"), index))
              except OSError:
                continue

            ip_address = utilities.get_ip_address(IP_ADDRESS_INTERFACES)
            if claimed_address is not None and ip_address != claimed_address:
              claimed_address = None

            if claimed_address is None and ip_address is not None and not is_hostname_taken(name):
              claimed_address = ip_address

          remaining = next_check - time.monotonic()
          if remaining <= 0:
            continue

          sock.settimeout(remaining)
          try:
            data, source = sock.recvfrom(2048)
          except TimeoutError:
            continue

          if claimed_address is None or len(data) < 12 or data[2] & 0x80 or name.lower() not in data[12:].lower():
            continue

          address = local_ip_for(source[0])
          if address is not None:
            if source[1] == MDNS_PORT:
              sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(address))
              sock.sendto(header + record + socket.inet_aton(address), (MDNS_GROUP, MDNS_PORT))
            else:
              question = data[12:12 + len(name) + 4]
              if data[4:6] != b"\0\1" or len(question) != len(name) + 4 or question[:-4].lower() != name.lower():
                continue

              query_type, query_class = struct.unpack(">HH", question[-4:])
              if query_type not in (1, 255) or query_class & 0x7FFF not in (1, 255):
                continue

              response_header = data[:2] + struct.pack(">HHHHH", 0x8400, 1, 1, 0, 0)
              response_record = name + struct.pack(">HHIH", 1, 1, 10, 4)
              sock.sendto(response_header + question + response_record + socket.inet_aton(address), source)
        except OSError:
          cloudlog.exception("The Pond: local discovery could not answer a query")
  except OSError:
    cloudlog.exception("The Pond: local discovery stopped")
