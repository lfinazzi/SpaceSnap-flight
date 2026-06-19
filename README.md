# UNSAM SpaceSnap (USS)

Firmware for the **UNSAM SpaceSnap** imaging payload — a PC/104 form-factor board designed to capture Earth photographs aboard **UNSAMSat-01** in Low Earth Orbit (LEO). The payload is integrated into an **EnduroSat 1U CubeSat bus**.

---

## Overview

| Item | Details |
|------|---------|
| **MCU** | STM32F217ZGTX (Arm Cortex-M3, 120 MHz) |
| **Cameras** | 2× ASX340AT (640×480, YCbCr 4:2:2) |
| **Non-volatile storage** | 2 MB Ferroelectric RAM — CY15B108QSN (SPI) |
| **Volatile frame buffer** | ~4 MB external SRAM (FSMC) |
| **Host interface** | RS-485 via EnduroSat AirMAC protocol |
| **Debug interface** | UART4 @ 115200 baud |
| **Build toolchain** | STM32CubeIDE (Eclipse CDT / GCC ARM) |

---

## Features

- **Dual camera system** — Camera A (I2C 0xBA) and Camera B (I2C 0x90), each with independent 2.8 V power rail and 27 MHz external clock.
- **DCMI capture** — DMA-driven parallel camera interface; single-shot snapshot mode.
- **JPEG compression** — On-device compression using the TinyJPEG library at three selectable quality levels (1 = lowest, 3 = highest).
- **Persistent telemetry** — Board status (boot count, uptime, reset causes, operation counters, ADC readings) stored in FRAM and survives power cycles.
- **Delayed picture scheduling** — A photo can be scheduled to execute after N RS-485 polling intervals.
- **FRAM photo archive** — Up to 40 compressed images indexed in FRAM for later downlink.
- **Chunked downlink** — Image data and headers streamed in 111-byte RS-485 chunks to the OBC.
- **IWDG watchdog** — System health enforced by hardware watchdog.

---

## Repository Structure

```
USS/
├── Core/
│   ├── Inc/          # Application header files
│   ├── Src/          # Application source files
│   └── Startup/      # Cortex-M3 assembly startup
├── Drivers/          # STM32F2xx HAL + CMSIS (generated)
├── Debug/            # Build artefacts (not tracked by git)
├── USS.ioc           # STM32CubeMX peripheral configuration
├── STM32F217ZGTX_FLASH.ld
├── STM32F217ZGTX_RAM.ld
└── README.md
```

### Key source modules

| File | Responsibility |
|------|---------------|
| `main.c` | Entry point, HAL init, main polling loop |
| `command.c` | Command dispatch table and handlers |
| `comms.c` | RS-485 / UART framing, AirMAC state machine |
| `photo.c` | Camera init, DCMI capture, TinyJPEG compression |
| `fram.c` | SPI FRAM read / write / erase |
| `sram.c` | External SRAM buffer management and self-test |
| `status.c` | Board status struct, FRAM persistence, logging |
| `telemetry.c` | Reset-cause detection, MCU temperature, VREFINT |

---

## Hardware Architecture

### Camera Interface
Both cameras share a single DCMI bus and I2C bus (I2C2). Only one camera is powered at a time. Power sequencing:

1. Assert 27 MHz EXTCLK output (TIM11 CH1 PWM).
2. Enable 2.8 V camera rail (`IMG_ENA_A` / `IMG_ENA_B`).
3. Enable I2C level shifter (`IMG_I2C_ENA`).
4. Release camera reset (`CAM_RESET`).
5. Wait 150 ms for sensor firmware boot.
6. Configure sensor registers over I2C2 (resolution, frame rate, exposure).

### Memory Layout

**External SRAM (FSMC, ~4 MB)**

| Region | Size | Purpose |
|--------|------|---------|
| RAW_BUFFER 0–4 | 614,420 B each | Raw YCbCr 4:2:2 frames |
| COMPRESSED_BUFFER | 614,428 B | JPEG output staging area |

**FRAM (2 MB, SPI2)**

| Address | Content |
|---------|---------|
| `0x000000` | `board_status_t` struct |
| `0x000100` | Compression index table (40 × 12 B entries) |
| `0x000600` | Compressed JPEG archive (~1.8 MB) |
| `0x1C0000` | Firmware backup region (256 KB reserved) |

### Communication — RS-485 AirMAC

The EnduroSat OBC polls the payload over RS-485 using 119-byte frames:

```
[ Init Byte ] [ Status ] [ Instruction ] [ Opcode (0 or 5 B) ]
```

The application state machine has five states:

| State | Description |
|-------|-------------|
| `STATE_IDLE` | Listening for incoming command frames |
| `STATE_IGNORE` | LS-02 module holds the bus |
| `STATE_EXECUTE_COMMAND` | Processing a received command |
| `STATE_TRANSMIT_RESPONSE` | Sending the response frame |
| `STATE_DELAYED_PICTURE` | Waiting for scheduled photo trigger |

---

## Command Reference

All commands arrive in a 119-byte AirMAC frame. The 5-byte opcode field is always present; unused bytes are ignored. See [Return Codes](#return-codes) for response status values.

---

### Mission Commands

#### `0x33` — `CMD_TakePicture`
Captures a single raw YCbCr frame and stores it in the selected SRAM buffer.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | `[7:4]` destination buffer · `[3:0]` camera select | Buffer: 0–4 · Camera: 0 = CAM A, 1 = CAM B |
| `opcode[1]` | Black-filter enable | 0 = disabled, 1 = enabled |
| `opcode[2]` | Retry count (filter enabled only) | Number of retakes before giving up |
| `opcode[3]` | Black-pixel threshold (filter enabled only) | Pixel count limit |
| `opcode[4]` | — | Unused |

> Example — CAM B into buffer 0: `01 00 00 00 00`

---

#### `0x34` — `CMD_TakePictureDelayed`
Schedules a photo to execute after N × MIN_INTERVAL minutes. Returns `CMD_SCHEDULED` immediately and transitions the state machine to `STATE_DELAYED_PICTURE`.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | `[7:4]` buffer · `[3:0]` camera | Same encoding as `CMD_TakePicture` |
| `opcode[1]` | Black-filter enable | Same as `CMD_TakePicture` |
| `opcode[2]` | Retry count | Same as `CMD_TakePicture` |
| `opcode[3]` | Black-pixel threshold | Same as `CMD_TakePicture` |
| `opcode[4]` | Delay in intervals (N) | 1–255 · max = 255 × MIN_INTERVAL min |

---

#### `0x35` — `CMD_ChangeCamParams` *(untested)*
Updates a configurable camera register shared by both cameras.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | Parameter index | 0 = `ae_rule_algo_val` (only index implemented) |
| `opcode[1]` | New value MSB | — |
| `opcode[2]` | New value LSB | — |
| `opcode[3:4]` | — | Unused |

---

#### `0x36` — `CMD_CompressRawPhoto`
JPEG-compresses a raw SRAM buffer and saves the result to both the SRAM compressed buffer and FRAM.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | Source raw buffer | 0–4 |
| `opcode[1]` | JPEG quality | 1 = lowest · 2 = medium · 3 = highest |
| `opcode[2:4]` | — | Unused |

---

#### `0x37` — `CMD_SendRawFrame`
Sends a 117-byte chunk of raw pixel data from SRAM to the OBC. Repeat with incrementing offsets to downlink the full frame (~5,250 chunks for a 614,400-byte frame).

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | Source raw buffer | 0–4 |
| `opcode[1:4]` | Byte offset into `data[]` (big-endian uint32) | 0, 117, 234, … |

---

#### `0x38` — `CMD_SendCompFrame`
Sends a 117-byte chunk of JPEG data from FRAM to the OBC. Repeat with incrementing offsets to downlink the full compressed image.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | Compression table index | 0 to `compression_count − 1` |
| `opcode[1:4]` | Byte offset into JPEG data region (big-endian uint32) | 0, 117, 234, … |

---

#### `0x39` — `CMD_SendRawHeader`
Returns the fixed-size metadata header of a raw buffer (designator, opcode, timestamp, black-pixel count) in a single response frame.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | Source raw buffer | 0–4 |
| `opcode[1:4]` | — | Unused |

---

#### `0x3A` — `CMD_SendCompHeader`
Returns the fixed-size metadata header of a FRAM-stored compressed image (index, designator, opcode, quality, JPEG size, timestamp, black-pixel count) in a single response frame.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | Compression table index | 0 to `compression_count − 1` |
| `opcode[1:4]` | — | Unused |

---

#### `0x3B` — `CMD_GetStatus`
Serialises `board_status_t` into the response frame and prints a full human-readable field breakdown over UART4. No opcode.

---

### Debug Commands

#### `0x11` — `CMD_DumpRaw`
Hex-dumps a raw buffer's header and pixel data over UART4. Takes approximately 1 m 20 s for a full 614,400-byte frame.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | Raw buffer to dump | 0–4 |
| `opcode[1:4]` | — | Unused |

---

#### `0x12` — `CMD_DumpCompressed`
Hex-dumps the SRAM compressed buffer (header + JPEG data) over UART4. No opcode. Fails if the SRAM buffer holds no valid compression.

#### `0x13` — `CMD_DumpAllSRAM`
Streams the entire raw SRAM buffer region as raw binary over UART4 (256-byte chunks, IWDG refreshed per chunk). Takes approximately 1 m 20 s. No opcode.

#### `0x14` — `CMD_DumpAllFRAM`
Streams the entire FRAM address space as raw binary over UART4 (256-byte burst SPI reads). Takes approximately 50 s. No opcode.

---

### Danger Zone

#### `0x88` — `CMD_EraseFRAM`
Erases the full 2 MB FRAM byte-by-byte (~40 s, IWDG refreshed throughout). **Requires an exact 5-byte confirmation opcode** to prevent accidental erasure.

| Byte | Required value |
|------|---------------|
| `opcode[0]` | `0x0A` |
| `opcode[1]` | `0x0F` |
| `opcode[2]` | `0x0A` |
| `opcode[3]` | `0x0F` |
| `opcode[4]` | `0x0A` |

Returns `CMD_CONFIRM_FAILED` if the opcode does not exactly match `0A 0F 0A 0F 0A`.

---

#### `0x89` — `CMD_ForceReset`
Persists `board_status` to FRAM, transmits a success response to the OBC, then triggers an immediate IWDG reset (~125 µs). No opcode. Never returns.

---

#### `0x90` — `CMD_EraseCompressions`
Erases only the FRAM photo data region (`PHOTO_DATA_START` → `FIRMWARE_BACKUP_START`, ~40 s) and resets the in-RAM compression table and all related `board_status` fields. No opcode.

---

### Return Codes

| Value | Name | Meaning |
|-------|------|---------|
| 44 | `COMMAND_SUCCESS` | Executed successfully |
| 45 | `COMMAND_SCHEDULED` | Accepted, execution deferred |
| 73 | `COMMAND_ERROR` | Generic failure |
| 74 | `COMMAND_NOT_FOUND_FAILURE` | Instruction ID not in command table |
| 75 | `COMMAND_INCORRECT_PARAMETER_FAILURE` | Wrong number of opcode bytes |
| 76 | `COMMAND_BUFFER_UNOCCUPIED` | Requested buffer contains no photo |
| 77 | `COMMAND_BUFFER_OUT_OF_BOUNDS` | Offset exceeds buffer size |
| 78 | `COMMAND_CAM_BOOT_ERROR` | Camera did not respond on I2C init |
| 79 | `COMMAND_CAM_DCMI_ERROR` | DCMI capture failed |
| 80 | `COMMAND_COMPRESS_ERROR` | TinyJPEG compression failed |
| 81 | `COMMAND_FRAM_FULL` | Insufficient FRAM space for new compression |
| 82 | `COMMAND_BUFFER_INVALID` | Buffer index out of range or entry not valid |
| 83 | `COMMAND_INDEX_FULL` | Compression index table full (40-entry max) |
| 84 | `COMMAND_CONFIRM_FAILED_ID` | Confirmation opcode mismatch (danger zone) |

---

## Board Status & Telemetry

The `board_status_t` struct is persisted in FRAM and updated after every significant event. It includes:

- **Boot metrics:** boot count, total uptime, session uptime.
- **Reset counters:** IWDG, low-power, software, POR, pin reset, unknown.
- **Memory flags:** FRAM and SRAM initialisation health.
- **Last command:** instruction ID, status code, opcode bytes.
- **Operation counters:** photos taken, compressions performed, images rejected.
- **ADC readings:** MCU internal temperature, VREFINT (supply voltage reference).
- **FRAM archive state:** write pointer, compressed image count, bytes remaining.

---

## Building

1. Open the project in **STM32CubeIDE**.
2. Select the **Debug** build configuration.
3. Build with **Project → Build Project** (or `Ctrl+B`).
4. Flash via ST-LINK using **Run → Debug** or the flash programmer.

The `USS.ioc` file can be opened in **STM32CubeMX** to regenerate HAL drivers or modify peripheral configuration.

---

## Known Limitations / Pending Work

- **Black pixel filtering** — Infrastructure (counters, opcode fields) is in place but the detection algorithm is not yet implemented.
- **Camera parameter tuning** — `CMD_ChangeCamParams` only writes the AE algorithm register; other AE/AWB stages are commented out and marked untested.
- **USS reset GPIO** — `PollUSSReset()` cannot read PA8 as HIGH; pending hardware investigation.
- **Firmware backup CRC** — Field reserved in `board_status_t`; CRC algorithm and linker support not yet implemented.
