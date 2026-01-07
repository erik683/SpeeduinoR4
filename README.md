# SpeeduinoR4

Firmware that turns an **Arduino Uno R4 WiFi** into a **USB-to-CAN adapter** using the
**Lawicel SLCAN** protocol (compatible with `python-can`'s `slcan` interface and Linux `slcan`/SocketCAN tooling).

## What it does

- Enumerates as a USB CDC serial device (e.g. `/dev/ttyACM0`).
- Implements a practical subset of the SLCAN ASCII command set.
- Forwards received CAN frames to the host while the channel is open.

## Hardware

**Target board:** Arduino Uno R4 WiFi (Renesas RA4M1)  
**Transceiver:** External CAN transceiver required (example used in this project: **SN65HVD230**).

Basic wiring (see `src/main.cpp`):

- `D13 (CANRX0)` -> transceiver `RXD`
- `D10 (CANTX0)` -> transceiver `TXD`
- `3.3V` -> transceiver `VCC` (per SN65HVD230)
- `GND` -> transceiver `GND`
- Transceiver `CANH/CANL` -> CAN bus (with proper termination at the bus ends)

## Build, upload, monitor (PlatformIO)

This is a PlatformIO project (see `platformio.ini`).

- Build: `pio run`
- Upload: `pio run -t upload`
- Serial monitor: `pio device monitor`

Notes:

- Commands are **CR-terminated** (`\r`). `platformio.ini` configures the monitor accordingly.
- The project is configured for a 1,000,000 "baud" monitor speed; on USB CDC this is typically ignored, but some host tooling still wants a value set.

## Using it from a host

### Python (`python-can`)

Example (matches `src/main.cpp`):

```py
import can

bus = can.Bus(interface="slcan", channel="/dev/ttyACM0", bitrate=500000)
bus.send(can.Message(arbitration_id=0x123, data=[1, 2, 3, 4], is_extended_id=False))
print(bus.recv(1.0))
```

### Linux SocketCAN (`can-utils`)

Attach the serial device as `slcan0` (example for 500k = `S6`):

```sh
sudo slcan_attach -o -s6 /dev/ttyACM0
sudo ip link set up slcan0
candump slcan0
```

## SLCAN command reference

All commands are ASCII and must be terminated with **CR** (`\r`). The serial parser also treats **LF** as end-of-line.

**Responses**

- OK: just `\r`
- Error: `BEL` (`0x07`) + `\r`
- TX success (this firmware): `z\r` (standard) or `Z\r` (extended)

### Bitrate / channel control

| Command | Meaning | Notes |
|---|---|---|
| `S<n>` | Set bitrate preset | Only when channel is closed. Supported: `S4`=125k, `S5`=250k, `S6`=500k (default), `S8`=1M. Others return error. |
| `s...` | Set custom bit timing registers | Not supported (always error). |
| `O` | Open channel (normal) | Starts CAN with the configured bitrate. |
| `L` | Open channel (listen-only) | See "Not (yet) implemented / limitations" regarding true listen-only behavior. |
| `C` | Close channel | Safe to call even if already closed. |

### Transmit frames

| Command | Meaning | Format |
|---|---|---|
| `t` | Send standard data frame | `tIII L DD...` (11-bit ID, `III`=3 hex digits) |
| `T` | Send extended data frame | `TIIIIIIII L DD...` (29-bit ID, `IIIIIIII`=8 hex digits) |
| `r` | Send standard RTR | `rIII L` |
| `R` | Send extended RTR | `RIIIIIIII L` |

Where:

- Spaces are shown above only to separate fields; the on-wire SLCAN command has no separators.
- `L` is DLC as a single hex digit (`0-8`)
- `DD...` is 0-16 hex bytes (2 hex chars per byte), omitted for RTR frames

### Receive frames

When the channel is open, received frames are emitted as `t...` / `T...` SLCAN ASCII lines.
If timestamps are enabled (`Z1`), 4 hex timestamp digits are appended.

### Status / identification

| Command | Meaning | Response |
|---|---|---|
| `F` | Read status flags | `Fxx` (software-level error tracking: TX/RX buffer full, data overrun) |
| `V` | Firmware version | `Vxxyy` (major/minor in hex nibbles; e.g. `1.2` -> `V0102`) |
| `N` | Serial number | `Nxxxx` (4 hex digits; configurable in `config.h`) |

### Filters / timestamps

| Command | Meaning | Notes |
|---|---|---|
| `Z0` / `Z1` | Disable/enable RX timestamps | When enabled, RX frames include a 16-bit ms timestamp suffix. |
| `X0` / `X1` | Disable/enable auto-poll/send | When disabled (X0), RX frames are not automatically forwarded to host. |
| `Mxxxxxxxx` | Acceptance mask | 8 hex digits. Filter logic: `(id & mask) == (code & mask)` |
| `mxxxxxxxx` | Acceptance code | 8 hex digits. Setting mask to `00000000` effectively accepts all frames. |

## Libraries used / project structure

**Platform / framework**

- PlatformIO platform: `renesas-ra`
- Framework: Arduino

**External Arduino library**

- `Arduino_CAN` (used by `lib/CANBackend/RA4M1CAN.*`)

**Local libraries (in `lib/`)**

- `Transport`: `ITransport` + `SerialTransport` (USB CDC, line buffering, priority writes)
- `CANBackend`: `ICANBackend` + `RA4M1CAN` (Arduino_CAN wrapper + software TX queue + software acceptance filter)
- `Protocol`: `ProtocolDispatcher` + `IProtocolHandler`
- `SLCAN`: SLCAN parser/formatter + RX ring buffer + command handlers

**Tests**

- PlatformIO is configured for Unity (`test_framework = unity`), but no project-specific tests are included yet.

## Configuration

Most tunables live in `include/config.h` (firmware version, buffer sizes, queue depths, LED behavior, etc.).

## Not (yet) implemented / known limitations

- **Custom bit timing** (`s...`) is not supported.
- **Bitrate presets** are restricted to `S4/S5/S6/S8` (125k/250k/500k/1M) due to the current limitations of the Arduino_CAN library.
- **Common SLCAN extensions** `P` (poll one), `A` (poll all) are defined in `lib/SLCAN/SLCANCommands.h` but not currently handled. (`X0/X1` is now implemented.)
- **True hardware listen-only** is not enabled: the Arduino_CAN API doesn't expose RA4M1 listen-only configuration. Current behavior is "don't transmit".
- **Status flags (`F`)** track software-level errors (TX/RX buffer full, data overrun). Hardware error counters (TEC/REC, bus-off, arbitration lost) would require direct RA4M1 register access (see code comments for details).
- **RTR detection on RX**: Arduino_CAN does not expose an RTR flag, so received RTR frames are not identified as RTR.
- **Acceptance filtering** is implemented as a **software filter** in `RA4M1CAN` (hardware filtering via the Arduino_CAN API is limited).


## SLCAN spec deviations

This firmware aims to be compatible with the Lawicel SLCAN protocol, but the following behaviors are not fully spec-compliant:

- `s...` (custom bit timing) is not supported and always returns error.
- `S<n>` presets are limited to `S4/S5/S6/S8`; other presets return error.
- `P` and `A` (poll commands) are defined but not implemented.
- `L` listen-only is best-effort; the Arduino_CAN API does not expose true hardware listen-only mode.
- `F` status flags track software errors only; hardware error counters require direct register access.
- RX RTR frames cannot be detected, so RTR indication on received frames is lost.
- `M`/`m` require 8 hex digits; 11-bit (4-hex-digit) masks/codes are rejected.
