<!-- @format -->

# Device API for Firmware Team (HTTP/HTTPS)

This document is the firmware-side source of truth for device messaging.

Goal: keep all existing features unchanged, while using HTTP/HTTPS APIs.

---

## 1. Connection and identity

Device calls cloud APIs over HTTPS:

1. url: XELOFUEL_CLOUD_URL (https endpoint)
2. tls: XELOFUEL_CLOUD_TLS=true
3. username: xelofuel_device
4. password: XELOFUEL_DEVICE_PASSWORD
5. client_id: xelofuel-device-001
6. device_id: device-001 (or configured value)

Important behaviors:

1. use automatic retry with backoff,
2. keep same message_id on retries,
3. keep unsent sales in durable local storage.

Current forwarded host example:

1. XELOFUEL_CLOUD_URL=https://1bfgjlxn-18830.asse.devtunnels.ms
2. XELOFUEL_CLOUD_TLS=true

---

## 2. HTTP endpoint map

Base URL:

1. `XELOFUEL_CLOUD_URL` (for example `https://1bfgjlxn-18830.asse.devtunnels.ms`)

Device -> cloud:

1. `POST /api/v1/devices/{device_id}/sales/single` (returns ACK envelope)
2. `POST /api/v1/devices/{device_id}/sales/batch` (returns ACK envelope)
3. `POST /api/v1/devices/{device_id}/reports/unit-price` (returns 202)
4. `POST /api/v1/devices/{device_id}/reports/nozzle-status` (returns 202)
5. `POST /api/v1/devices/{device_id}/command-result` (returns 202)

Cloud -> device pull flow:

1. `GET /api/v1/devices/{device_id}/commands/next`
2. `200` with a command envelope when available
3. `204` when no command is pending

---

## 3. Request/response body model

All request and response bodies are UTF-8 JSON envelopes.

For sales endpoints:

1. request body: sales envelope (`sales.single` or `sales.batch`)
2. response body: ACK envelope (`type=ack`)

For reports and command-result endpoints:

1. request body: corresponding envelope
2. response body: `{"status":"accepted"}`

---

## 4. Application envelope (HTTP request/response body)

Normal message field order:

1. protocol_version
2. message_id
3. device_id
4. sent_at
5. type
6. data
7. crc8

ACK message adds before data:

1. response_to
2. status
3. code
4. retryable

All payloads are UTF-8 JSON with canonical formatting rules.

---

## 5. Message types

Device to cloud:

1. sales.single
2. sales.batch
3. unit_price.report
4. nozzle.status
5. ack (command execution result)

Cloud to device:

1. command.set_nozzle_id
2. command.set_unit_price
3. ack (store/validation result)

### 5.1 Example: single sale cloud ACK payload

For a successful `sales.single` message, the cloud ACK payload structure is:

```json
{
  "protocol_version": "1.0",
  "message_id": "A01-0000000001",
  "device_id": "device-001",
  "sent_at": "2026-08-04T16:19:20Z",
  "type": "ack",
  "response_to": "D01-0000000001",
  "status": "success",
  "code": "STORED",
  "retryable": false,
  "data": {
    "sale_id": "S01-0000000001",
    "duplicate": false
  },
  "crc8": "FC"
}
```

Field meaning (single sale ACK):

1. `response_to`: original device message id being acknowledged.
2. `status`: `success` means the sale is accepted by cloud logic.
3. `code`: `STORED` for first-time accepted sale, `DUPLICATE_ACCEPTED` for replay.
4. `data.sale_id`: the sale id that was processed.
5. `data.duplicate`: `false` for first store, `true` when already stored.

---

## 6. Reliability rules firmware must implement

1. Persist sale data before first send.
2. Retry schedule: 10s, 30s, 60s, then every 300s.
3. On retry, keep same message_id and payload bytes.
4. Accept ACK only if CRC is valid.
5. Remove pending sale only on terminal success codes:
   1. STORED
   2. BATCH_STORED
   3. DUPLICATE_ACCEPTED

---

## 7. CRC-8 specification

CRC is calculated over canonical payload bytes excluding crc8 field.

Algorithm in firmware:

1. start crc = 0x00
2. for each byte b in payload: crc = table[crc XOR b]
3. output uppercase hex with exactly 2 chars

### 7.1 CRC-8 lookup table (required)

Use this exact table:

```text
00 5E BC E2 61 3F DD 83 C2 9C 7E 20 A3 FD 1F 41
9D C3 21 7F FC A2 40 1E 5F 01 E3 BD 3E 60 82 DC
23 7D 9F C1 42 1C FE A0 E1 BF 5D 03 80 DE 3C 62
BE E0 02 5C DF 81 63 3D 7C 22 C0 9E 1D 43 A1 FF
46 18 FA A4 27 79 9B C5 84 DA 38 66 E5 BB 59 07
DB 85 67 39 BA E4 06 58 19 47 A5 FB 78 26 C4 9A
65 3B D9 87 04 5A B8 E6 A7 F9 1B 45 C6 98 7A 24
F8 A6 44 1A 99 C7 25 7B 3A 64 86 D8 5B 05 E7 B9
8C D2 30 6E ED B3 51 0F 4E 10 F2 AC 2F 71 93 CD
11 4F AD F3 70 2E CC 92 D3 8D 6F 31 B2 EC 0E 50
AF F1 13 4D CE 90 72 2C 6D 33 D1 8F 0C 52 B0 EE
32 6C 8E D0 53 0D EF B1 F0 AE 4C 12 91 CF 2D 73
CA 94 76 28 AB F5 17 49 08 56 B4 EA 69 37 D5 8B
57 09 EB B5 36 68 8A D4 95 CB 29 77 F4 AA 48 16
E9 B7 55 0B 88 D6 34 6A 2B 75 97 C9 4A 14 F6 A8
74 2A C8 96 15 4B A9 F7 B6 E8 0A 54 D7 89 6B 35
```

---

## 8. ACK codes firmware should handle

1. STORED
2. BATCH_STORED
3. DUPLICATE_ACCEPTED
4. PARTIAL_ACCEPT
5. CRC_MISMATCH
6. COMMAND_EXPIRED
7. DEVICE_BUSY
8. COMMAND_APPLIED

---
