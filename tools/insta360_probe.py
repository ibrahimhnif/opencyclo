#!/usr/bin/env python3
"""Probe an Insta360 camera's control protocol (Ace Pro 2 verified).

Mode switching is SetOptions (cmd 7) with video_sub_mode (41) / photo_sub_mode
(40), plus main_mode (107) to cross the photo/video boundary -- NOT
SetPhotographyOptions, which the camera rejects. This script drives that over
BLE (default) or WiFi, so the exact FreeFrame sub-mode value can be found on
real hardware (see docs/insta360-camera-control.md, Phase A).

  python3 tools/insta360_probe.py --get                 # read current sub-modes
  python3 tools/insta360_probe.py --video-sub 0          # switch to normal video
  python3 tools/insta360_probe.py --sweep                # try video_sub_mode 0..24, read back
  python3 tools/insta360_probe.py --host 192.168.42.1    # force the WiFi transport

BLE needs `pip install bleak`. WiFi needs the camera AP joined (default
password 88888888 on older cameras).
"""

import argparse
import socket
import struct
import sys
import time

SYNC = bytes([0x06, 0x00, 0x00]) + b"syNceNdinS"
KEEPALIVE = bytes([0x05, 0x00, 0x00])

CMD_SET_OPTIONS = 7
CMD_GET_OPTIONS = 8

OPT_PHOTO_SUB_MODE = 40
OPT_VIDEO_SUB_MODE = 41
OPT_MAIN_MODE = 107

SERVICE = "0000be80-0000-1000-8000-00805f9b34fb"
WRITE = "0000be81-0000-1000-8000-00805f9b34fb"
NOTIFY = "0000be82-0000-1000-8000-00805f9b34fb"


def varint(value):
    out = bytearray()
    while True:
        b = value & 0x7F
        value >>= 7
        out.append(b | 0x80 if value else b)
        if not value:
            return bytes(out)


def frame(payload):
    return struct.pack("<i", len(payload) + 4) + payload


def command(cmd_id, seq, body=b""):
    h = bytes([0x04, 0x00, 0x00]) + struct.pack("<H", cmd_id) + bytes([0x02])
    h += seq.to_bytes(3, "little") + bytes([0x80, 0x00, 0x00])
    return frame(h + body)


def packed(option_types):
    body = b"".join(varint(t) for t in option_types)
    return bytes([0x0a, len(body)]) + body


def set_option(option_type, value):
    """SetOptions{option_types=[t], value{t=v}}."""
    inner = varint(option_type << 3) + varint(value)
    return bytes([0x0a, len(varint(option_type))]) + varint(option_type) + bytes([0x12, len(inner)]) + inner


def parse_frame(raw):
    """raw = full frame with length prefix -> (code, body) or None."""
    if len(raw) < 16:
        return None
    total = struct.unpack_from("<i", raw, 0)[0]
    p = raw[4:total]
    if len(p) < 12 or p[0] != 0x04:
        return None
    return struct.unpack_from("<H", p, 3)[0], p[12:]


def unpack_sub_modes(body):
    """Pull every varint field out of a GetOptions value message."""
    out = {}
    i = 0
    while i < len(body):
        key = 0
        shift = 0
        while i < len(body):
            b = body[i]; i += 1
            key |= (b & 0x7F) << shift
            if not (b & 0x80):
                break
            shift += 7
        field, wire = key >> 3, key & 7
        if wire == 0:
            val = 0; shift = 0
            while i < len(body):
                b = body[i]; i += 1
                val |= (b & 0x7F) << shift
                if not (b & 0x80):
                    break
                shift += 7
            out[field] = val
        elif wire == 2:
            ln = body[i]; i += 1
            sub = body[i:i + ln]; i += ln
            out.update(unpack_sub_modes(sub))
        else:
            break
    return out


class WifiCam:
    def __init__(self, host, port, timeout):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.seq = 0
        self.buf = b""
        self.responses = {}

    def handshake(self):
        self.sock.sendall(frame(SYNC))
        self.sock.sendall(frame(KEEPALIVE))

    def _pump(self):
        try:
            data = self.sock.recv(4096)
        except socket.timeout:
            return
        if not data:
            raise ConnectionError("camera closed the connection")
        self.buf += data
        while len(self.buf) >= 4:
            total = struct.unpack_from("<i", self.buf, 0)[0]
            if total < 4 or len(self.buf) < total:
                break
            raw, self.buf = self.buf[:total], self.buf[total:]
            r = parse_frame(raw)
            if r:
                self.responses[int.from_bytes(raw[10:13], "little")] = r

    def exchange(self, cmd_id, body, wait=1.2):
        seq = self.seq
        self.seq += 1
        self.sock.sendall(command(cmd_id, seq, body))
        deadline = time.time() + wait
        while time.time() < deadline:
            self._pump()
            if seq in self.responses:
                return self.responses.pop(seq)
            time.sleep(0.03)
        return None

    def close(self):
        self.sock.close()


class BleCam:
    def __init__(self, timeout):
        self.timeout = timeout
        self.seq = 0
        self.responses = {}

    def _on_notify(self, _c, data):
        r = parse_frame(bytes(data))
        if r:
            self.responses[int.from_bytes(bytes(data)[10:13], "little")] = r

    async def run(self, action):
        import asyncio
        from bleak import BleakScanner, BleakClient
        target = None

        def cb(d, a):
            nonlocal target
            svcs = [u.lower() for u in (a.service_uuids or [])]
            if SERVICE in svcs or (d.name or "").lower().startswith("ace"):
                target = d

        async with BleakScanner(detection_callback=cb):
            await asyncio.sleep(6)
        if not target:
            print("camera not found")
            return
        async with BleakClient(target) as client:
            self.client = client
            self.write_ch = [x for s in client.services for x in s.characteristics if x.uuid.lower() == WRITE][0]
            await client.start_notify(NOTIFY, self._on_notify)
            await client.write_gatt_char(self.write_ch, frame(SYNC), response=True)
            await asyncio.sleep(0.3)
            await client.write_gatt_char(self.write_ch, frame(KEEPALIVE), response=True)
            await asyncio.sleep(0.4)
            await action(self)
            try:
                await client.stop_notify(NOTIFY)
            except Exception:
                pass

    async def exchange(self, cmd_id, body):
        import asyncio
        seq = self.seq
        self.seq += 1
        self.responses.pop(seq, None)
        await self.client.write_gatt_char(self.write_ch, command(cmd_id, seq, body), response=True)
        await asyncio.sleep(1.0)
        await self.client.write_gatt_char(self.write_ch, frame(KEEPALIVE), response=True)
        await asyncio.sleep(0.1)
        return self.responses.pop(seq, None)


def report(tag, r):
    if r is None:
        print(f"{tag}: no response")
        return
    code, body = r
    print(f"{tag}: code={code} sub-modes={unpack_sub_modes(body) or '-'} body={body.hex()}")


def plan(args):
    steps = [("get", CMD_GET_OPTIONS, packed([OPT_PHOTO_SUB_MODE, OPT_VIDEO_SUB_MODE]))]
    if args.main is not None:
        steps.append((f"set main_mode={args.main}", CMD_SET_OPTIONS, set_option(OPT_MAIN_MODE, args.main)))
    if args.video_sub is not None:
        steps.append((f"set video_sub_mode={args.video_sub}", CMD_SET_OPTIONS, set_option(OPT_VIDEO_SUB_MODE, args.video_sub)))
        steps.append(("get", CMD_GET_OPTIONS, packed([OPT_PHOTO_SUB_MODE, OPT_VIDEO_SUB_MODE])))
    if args.photo_sub is not None:
        steps.append((f"set photo_sub_mode={args.photo_sub}", CMD_SET_OPTIONS, set_option(OPT_PHOTO_SUB_MODE, args.photo_sub)))
        steps.append(("get", CMD_GET_OPTIONS, packed([OPT_PHOTO_SUB_MODE, OPT_VIDEO_SUB_MODE])))
    if args.sweep:
        for v in range(0, 25):
            steps.append((f"sweep video_sub_mode={v}", CMD_SET_OPTIONS, set_option(OPT_VIDEO_SUB_MODE, v)))
            steps.append(("  get", CMD_GET_OPTIONS, packed([OPT_PHOTO_SUB_MODE, OPT_VIDEO_SUB_MODE])))
    return steps


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", help="use WiFi TCP to this host (else BLE)")
    ap.add_argument("--port", type=int, default=6666)
    ap.add_argument("--timeout", type=float, default=6.0)
    ap.add_argument("--get", action="store_true", help="(default) read the current sub-modes")
    ap.add_argument("--video-sub", type=int, help="set video_sub_mode (41)")
    ap.add_argument("--photo-sub", type=int, help="set photo_sub_mode (40)")
    ap.add_argument("--main", type=int, help="set main_mode (107): 0=video 1=photo")
    ap.add_argument("--sweep", action="store_true", help="try video_sub_mode 0..24 and read back")
    args = ap.parse_args()
    steps = plan(args)

    if args.host:
        cam = WifiCam(args.host, args.port, args.timeout)
        cam.handshake()
        try:
            for tag, cmd_id, body in steps:
                report(tag, cam.exchange(cmd_id, body))
        finally:
            cam.close()
        return 0

    async def action(cam):
        for tag, cmd_id, body in steps:
            report(tag, await cam.exchange(cmd_id, body))

    import asyncio
    asyncio.run(BleCam(args.timeout).run(action))
    return 0


if __name__ == "__main__":
    sys.exit(main())
