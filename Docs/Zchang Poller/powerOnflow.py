#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Power-on flow with Steps 2 & 3 replaced by Section 4.10 (accumulative totals).

Sequence:
 1) Status: A5 01 03 00 01 CRC  -> print status code
 2) 4.10 Accumulative totals (origin 0x60, len 0x0C) -> print TOTAL (vol & sale)
 3) (no offline bag/records)
 4) Poll status every 100 ms until 0x00 (idle)

Notes:
 - Read frames exactly by expected length.
 - Error frame: A5 | addr | (func|0x80) | code | CRC  (5 bytes total)
 - Normal reply: A5 | addr | func | len | data(len) | CRC
"""

import serial, time
from typing import Optional, Tuple

# ---------- CONFIG ----------
COM_PORT  = "COM3"    # change if needed
BAUD      = 4800
TIMEOUT_S = 0.15
ADDR      = 0x01
POLL_MS   = 100
CMD_GAP_S = 0.02
# ----------------------------

CRC8_TAB = bytes([
    0x00,0x5E,0xBC,0xE2,0x61,0x3F,0xDD,0x83,0xC2,0x9C,0x7E,0x20,0xA3,0xFD,0x1F,0x41,
    0x9D,0xC3,0x21,0x7F,0xFC,0xA2,0x40,0x1E,0x5F,0x01,0xE3,0xBD,0x3E,0x60,0x82,0xDC,
    0x23,0x7D,0x9F,0xC1,0x42,0x1C,0xFE,0xA0,0xE1,0xBF,0x5D,0x03,0x80,0xDE,0x3C,0x62,
    0xBE,0xE0,0x02,0x5C,0xDF,0x81,0x63,0x3D,0x7C,0x22,0xC0,0x9E,0x1D,0x43,0xA1,0xFF,
    0x46,0x18,0xFA,0xA4,0x27,0x79,0x9B,0xC5,0x84,0xDA,0x38,0x66,0xE5,0xBB,0x59,0x07,
    0xDB,0x85,0x67,0x39,0xBA,0xE4,0x06,0x58,0x19,0x47,0xA5,0xFB,0x78,0x26,0xC4,0x9A,
    0x65,0x3B,0xD9,0x87,0x04,0x5A,0xB8,0xE6,0xA7,0xF9,0x1B,0x45,0xC6,0x98,0x7A,0x24,
    0xF8,0xA6,0x44,0x1A,0x99,0xC7,0x25,0x7B,0x3A,0x64,0x86,0xD8,0x5B,0x05,0xE7,0xB9,
    0x8C,0xD2,0x30,0x6E,0xED,0xB3,0x51,0x0F,0x4E,0x10,0xF2,0xAC,0x2F,0x71,0x93,0xCD,
    0x11,0x4F,0xAD,0xF3,0x70,0x2E,0xCC,0x92,0xD3,0x8D,0x6F,0x31,0xB2,0xEC,0x0E,0x50,
    0xAF,0xF1,0x13,0x4D,0xCE,0x90,0x72,0x2C,0x6D,0x33,0xD1,0x8F,0x0C,0x52,0xB0,0xEE,
    0x32,0x6C,0x8E,0xD0,0x53,0x0D,0xEF,0xB1,0xF0,0xAE,0x4C,0x12,0x91,0xCF,0x2D,0x73,
    0xCA,0x94,0x76,0x28,0xAB,0xF5,0x17,0x49,0x08,0x56,0xB4,0xEA,0x69,0x37,0xD5,0x8B,
    0x57,0x09,0xEB,0xB5,0x36,0x68,0x8A,0xD4,0x95,0xCB,0x29,0x77,0xF4,0xAA,0x48,0x16,
    0xE9,0xB7,0x55,0x0B,0x88,0xD6,0x34,0x6A,0x2B,0x75,0x97,0xC9,0x4A,0x14,0xF6,0xA8,
    0x74,0x2A,0xC8,0x96,0x15,0x4B,0xA9,0xF7,0xB6,0xE8,0x0A,0x54,0xD7,0x89,0x6B,0x35
])

def crc8(payload: bytes) -> int:
    c = 0
    for b in payload:
        c = CRC8_TAB[c ^ b]
    return c & 0xFF

def bcd_to_float(b: bytes, decimals: int = 2) -> float:
    digits = "".join(f"{x:02X}" for x in b).lstrip("0") or "0"
    if len(digits) <= decimals:
        digits = digits.zfill(decimals + 1)
    return float(f"{int(digits[:-decimals])}.{digits[-decimals:]}")

# ---- frame builders ----
def build_read(addr: int, origin: int, length: int) -> bytes:
    # Master: A5 | addr | 03 | origin | len | CRC8(addr..len)
    body = bytes([addr & 0xFF, 0x03, origin & 0xFF, length & 0xFF])
    return b"\xA5" + body + bytes([crc8(body)])

# ---- IO helpers ----
def read_exact(ser: serial.Serial, n: int, timeout_s: float) -> bytes:
    end = time.perf_counter() + timeout_s
    buf = bytearray()
    while len(buf) < n and time.perf_counter() < end:
        chunk = ser.read(n - len(buf))
        if chunk:
            buf.extend(chunk)
        else:
            time.sleep(0.0005)
    return bytes(buf)

def read_reply(ser: serial.Serial, expect_len: int, note: str) -> Optional[bytes]:
    resp = read_exact(ser, expect_len, TIMEOUT_S)
    if not resp:
        print(f"[WARN] no reply ({note})")
        return None
    # Error reply: A5 | addr | (func|0x80) | code | CRC (5 bytes)
    if len(resp) == 5 and resp[0] == 0xA5 and (resp[2] & 0x80):
        print(f"[ERROR] func={resp[2]:02X} code={resp[3]:02X} raw={resp.hex()} ({note})")
        return None
    if resp[0] != 0xA5:
        print(f"[WARN] bad preamble: {resp.hex()} ({note})")
        return None
    addr, func, ln = resp[1], resp[2], resp[3]
    data = resp[4:4+ln]
    crc_rx = resp[-1]
    if crc8(bytes([addr, func, ln]) + data) != crc_rx:
        print(f"[WARN] crc mismatch: {resp.hex()} ({note})")
        return None
    if len(data) != ln:
        print(f"[WARN] len mismatch: {resp.hex()} ({note})")
        return None
    return resp

# ---- protocol ops ----
def read_status(ser: serial.Serial) -> Optional[int]:
    # origin 0x00, len 0x01
    pkt = build_read(ADDR, 0x60, 0x0C)
    ser.reset_input_buffer(); ser.write(pkt)
    # time.sleep(CMD_GAP_S)
    # Expect: 1 + 3 + 1 + 1 = 6
    resp = read_reply(ser, 6, "status")
    return resp[4] if resp else None

def read_accum_totals(ser: serial.Serial) -> Optional[Tuple[float,float]]:
    # 4.10: origin 0x60, len 0x0C
    pkt = build_read(ADDR, 0x60, 0x0C)
    ser.reset_input_buffer(); ser.write(pkt)
    time.sleep(CMD_GAP_S)
    # Expect: 1 + 3 + 12 + 1 = 17
    resp = read_reply(ser, 17, "accum_totals(4.10)")
    if not resp:
        return None
    data = resp[4:4+0x0C]
    vol  = bcd_to_float(data[0:6], 2)
    sale = bcd_to_float(data[6:12], 2)
    return vol, sale

def wait_until_idle(ser: serial.Serial):
    while True:
        st = read_status(ser)
        if st is None:
            print("[WARN] status failed; retrying…")
        else:
            # print(f"[status] 0x{st:02X}")
            if st == 0x00:
                print("[OK] nozzle idle")
                return
        time.sleep(POLL_MS / 1000.0)

# ---- main ----
def main():
    while True:
        with serial.Serial(COM_PORT, BAUD, timeout=0.1) as ser:
            # Step 1: status
            st = read_status(ser)
            print(st)
            if st is None:
                print("[ERR] initial status read failed")
            else:
                print(f"[status] 0x{st:02X}")
                # pass

            # Steps 2 & 3 (replaced): 4.10 totals
            # totals = read_accum_totals(ser)
            # print("TOTALS:", totals)
            # if totals:
            #     v, s = totals
            #     print(f"[TOTAL] volume={v:.2f} L  sale={s:.2f}")
            # else:
            #     print("[TOTAL] not available (4.10 returned error or bad frame)")

            # Step 4: poll until idle (every 100 ms)
            # wait_until_idle(ser)
        time.sleep(POLL_MS / 1000.0)

if __name__ == "__main__":
    main()
