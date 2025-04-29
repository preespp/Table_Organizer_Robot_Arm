#  #!/usr/bin/env python3
'''
for TCP
'''
# import socket, struct, os

# HOST = "0.0.0.0"      # listen on every interface
# PORT = 8080

# def recv_all(sock, length):
#     data = b''
#     while len(data) < length:
#         chunk = sock.recv(length - len(data))
#         if not chunk:
#             raise EOFError("socket closed")
#         data += chunk
#     return data

# os.makedirs("frames", exist_ok=True)
# frame_no = 0

# with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
#     s.bind((HOST, PORT))
#     s.listen(1)
#     print(f"[PC] waiting on {HOST}:{PORT}")

#     while True:
#         conn, addr = s.accept()
#         with conn:
#             print("[PC] connection from", addr)

#             # ---- Step 1: read 4-byte length ----
#             raw = recv_all(conn, 4)
#             frame_len = struct.unpack("!I", raw)[0]      # network→host
#             print("[PC] incoming length:", frame_len)

#             # ---- Step 2: send single-byte ACK ----
#             conn.sendall(b'O')

#             # ---- Step 3: read JPEG bytes ----
#             jpeg = recv_all(conn, frame_len)

#             # ---- quick validity check ----
#             if not (jpeg.startswith(b'\xFF\xD8') and jpeg.endswith(b'\xFF\xD9')):
#                 print("‼  invalid JPEG markers – frame skipped")
#                 continue

#             filename = f"frames/frame_{frame_no:04d}.jpg"
#             with open(filename, "wb") as f:
#                 f.write(jpeg)
#             print("[PC] saved", filename)

#             frame_no += 1

#!/usr/bin/env python3
"""
for UDP
"""

import socket, struct, time, os
from collections import defaultdict, namedtuple

HOST, PORT     = "0.0.0.0", 5005
TIMEOUT_MS     = 300        # give up on a frame if no packet for 300 ms
DIR            = "frames_udp"

PacketHdr = struct.Struct("<4H")             # frame_id, pkt_id, total_pkts, len
FrameBuf  = namedtuple("FrameBuf", "ts pkts total data")

os.makedirs(DIR, exist_ok=True)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((HOST, PORT))
sock.settimeout(0.2)

frames = {}                                   # frame_id -> FrameBuf
next_save = 0

print(f"[PC] listening on {HOST}:{PORT}")

while True:
    try:
        datagram, _ = sock.recvfrom(8 + 1024)
    except socket.timeout:
        pass                                  # just fall through to cleanup
    else:
        fid, pid, tot, plen = PacketHdr.unpack_from(datagram)
        payload = datagram[8:8+plen]

        fb = frames.get(fid)
        if fb is None:
            fb = FrameBuf(ts=time.time(), pkts=0, total=tot,
                          data=[None]*tot)
            frames[fid] = fb

        if fb.data[pid] is None:              # first arrival of this pkt
            fb.data[pid] = payload
            fb = fb._replace(pkts=fb.pkts+1, ts=time.time())
            frames[fid] = fb

        # complete?
        if fb.pkts == fb.total:
            jpeg = b''.join(fb.data)
            if jpeg.startswith(b'\xff\xd8') and jpeg.endswith(b'\xff\xd9'):
                name = f"{DIR}/frame_{next_save:04d}.jpg"
                with open(name, "wb") as f:
                    f.write(jpeg)
                print(f"[PC] saved {name} ({len(jpeg)} B)")
                next_save += 1
            else:
                print(f"[PC] frame {fid} bad markers – dropped")
            del frames[fid]

    # ---- cleanup old frames ----
    now = time.time()
    stale = [fid for fid, fb in frames.items() if now - fb.ts > TIMEOUT_MS/1000]
    for fid in stale:
        print(f"[PC] frame {fid} timed-out – dropped ({frames[fid].pkts}/{frames[fid].total})")
        del frames[fid]
