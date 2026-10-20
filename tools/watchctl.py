#!/usr/bin/env python3
"""Talk to the watch over its USB serial console.

    tools/watchctl.py log [--reset] [--seconds N]   print the log
    tools/watchctl.py cmd "<console command>"       run a console command
    tools/watchctl.py shot [out.png]                save a screenshot
    tools/watchctl.py press up|down|select|alt ...  inject button presses
    tools/watchctl.py synctime                      set the watch clock from this Mac

Needs pyserial (bundled with ESP-IDF's Python env; tools/watch.sh runs it there).
"""
import argparse
import base64
import glob
import re
import struct
import sys
import time
import zlib

import serial

PROMPT = b"watch> "
B64_LINE = re.compile(r"[A-Za-z0-9+/]+={0,2}")


def find_port():
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No watch found on /dev/cu.usbmodem*")
    return ports[0]


def open_port(port, reset=False):
    # Leave DTR/RTS as the OS sets them on open (both asserted). On the
    # ESP32-S3 USB-Serial/JTAG, dropping DTR while RTS is high resets the chip.
    ser = serial.Serial(port, 115200, timeout=0.1)
    if reset:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        ser.dtr = True
    return ser


def read_for(ser, seconds):
    end = time.time() + seconds
    out = bytearray()
    while time.time() < end:
        chunk = ser.read(4096)
        if chunk:
            out += chunk
            sys.stdout.write(chunk.decode("utf-8", "replace"))
            sys.stdout.flush()
    return bytes(out)


def ascii_escape(text):
    return "".join(
        c if ord(c) < 0x80 else ("\\\\u%04x" % ord(c) if ord(c) < 0x10000 else "\\\\U%08x" % ord(c))
        for c in text
    )


def run_command(ser, command, timeout=5.0, echo=True):
    ser.reset_input_buffer()
    # The REPL runs linenoise in "dumb" mode, which ends a line on CR and drops
    # non-ASCII bytes, so those go as \uNNNN escapes (decoded by the demo
    # command). The backslash is doubled for the console's argument splitter.
    line = ascii_escape(command).encode() + b"\r"
    # Small chunks: the watch's USB serial RX buffer overflows on long lines.
    for i in range(0, len(line), 64):
        ser.write(line[i : i + 64])
        ser.flush()
        time.sleep(0.01)
    out = bytearray()
    end = time.time() + timeout
    while time.time() < end:
        chunk = ser.read(4096)
        if chunk:
            out += chunk
            end = max(end, time.time() + 0.3)
            if out.rstrip().endswith(PROMPT.rstrip()):
                break
    text = out.decode("utf-8", "replace")
    if echo:
        print(text)
    return text


def png_rgb565(width, height, data, path):
    rows = []
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            (px,) = struct.unpack_from("<H", data, (y * width + x) * 2)
            r = (px >> 11) & 0x1F
            g = (px >> 5) & 0x3F
            b = px & 0x1F
            row += bytes(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31))
        rows.append(bytes(row))

    def chunk(tag, payload):
        c = struct.pack(">I", len(payload)) + tag + payload
        return c + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def grab_screenshot(ser):
    text = run_command(ser, "shot", timeout=10.0, echo=False)
    if "SHOT BEGIN" not in text:
        # The console now and then rejects the first line after the port opens.
        text = run_command(ser, "shot", timeout=10.0, echo=False)
    if "SHOT BEGIN" not in text:
        sys.exit("No screenshot data. Console output:\n" + text)
    _, rest = text.split("SHOT BEGIN", 1)
    dims, rest = rest.split("\n", 1)
    width, height = (int(v) for v in dims.split())
    payload = rest.split("SHOT END", 1)[0]
    # Log lines from other tasks (BLE, ...) can land between the data lines.
    lines = [l.strip() for l in payload.splitlines()]
    data = base64.b64decode("".join(l for l in lines if l and B64_LINE.fullmatch(l)))
    return width, height, data


def screenshot(ser, path):
    for attempt in range(2):
        width, height, data = grab_screenshot(ser)
        if len(data) == width * height * 2:
            png_rgb565(width, height, data, path)
            print(path)
            return
    sys.exit(f"Screenshot size mismatch: {len(data)} bytes for {width}x{height}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default=None)
    sub = p.add_subparsers(dest="action", required=True)

    lg = sub.add_parser("log")
    lg.add_argument("--reset", action="store_true")
    lg.add_argument("--seconds", type=float, default=5.0)

    cm = sub.add_parser("cmd")
    cm.add_argument("command")

    sh = sub.add_parser("shot")
    sh.add_argument("out", nargs="?", default=None)

    pr = sub.add_parser("press")
    pr.add_argument("buttons", nargs="+", choices=["up", "down", "select", "alt"])
    pr.add_argument("--hold", action="store_true", help="long press")

    sub.add_parser("synctime")

    args = p.parse_args()
    ser = open_port(args.port or find_port(), reset=getattr(args, "reset", False))

    if args.action == "log":
        read_for(ser, args.seconds)
    elif args.action == "cmd":
        run_command(ser, args.command)
    elif args.action == "shot":
        out = args.out or time.strftime("screenshots/shot-%Y%m%d-%H%M%S.png")
        import os

        os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
        screenshot(ser, out)
    elif args.action == "press":
        for b in args.buttons:
            run_command(ser, f"press {b}{' long' if args.hold else ''}", echo=False)
            time.sleep(0.25)
    elif args.action == "synctime":
        run_command(ser, f"time {int(time.time())}")


if __name__ == "__main__":
    main()
