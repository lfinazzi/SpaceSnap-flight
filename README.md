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
- **Auto-exposure mode** — Standard initialisation via `CAM_Init()`; the sensor's built-in AE/AWB algorithms control exposure and gain.
- **Manual exposure/gain mode** — Advanced initialisation via `CAM_InitAdvanced()`; host sets analog gain, digital gain, coarse and fine exposure directly via ground command.
- **Black-pixel filtering** — Captured frames are inspected for luma content; frames exceeding a configurable black-pixel fraction are discarded and retaken, up to a configurable retry limit.
- **JPEG compression** — On-device compression using the TinyJPEG library at three selectable quality levels (1 = lowest, 3 = highest).
- **Burst photography** — Configurable multi-shot bursts with inter-shot delay and optional automatic FRAM compression, triggered either immediately or after a scheduled delay.
- **Persistent telemetry** — Board status (boot count, uptime, reset causes, operation counters, ADC readings, camera parameters) stored in FRAM and survives power cycles.
- **Delayed picture scheduling** — A burst can be scheduled to execute after N × MIN_INTERVAL minutes, cancelled automatically on the next USS command.
- **FRAM photo archive** — Up to 100 compressed images indexed in FRAM for later downlink.
- **Chunked downlink** — Image data streamed in 117-byte RS-485 chunks to the OBC; headers fit in a single response frame.
- **Firmware backup** — Application image streamed from internal flash to FRAM with CRC32 verification; bootloader can use this copy for recovery.
- **IWDG watchdog** — System health enforced by hardware watchdog, refreshed throughout all long operations.
- **Radiation / SEU fault tolerance** — Layered protection against SRAM and FRAM corruption from single-event upsets, FRAM write faults, and unplanned resets. See [Radiation & SEU Protection](#radiation--seu-protection) for the full architecture and test results.

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
| `protection.c` | RAM shadow CRC, state-machine majority vote, FRAM integrity helpers |
| `telemetry.c` | Reset-cause detection, MCU temperature, VREFINT |

---

## Hardware Architecture

### Camera Interface
Both cameras share a single DCMI bus and I2C bus (I2C2). Only one camera is powered at a time. Power sequencing:

1. Enable level shifter supply rail (`IMG_ENA`, PA12).
2. Start EXTCLK (27 MHz, TIM11 CH1 PWM) — must be running before sensor rails come up.
3. Assert `RESET_BAR` LOW (PC0, shared between both cameras).
4. Enable 2.8 V camera rail (`IMG_ENA_A` / `IMG_ENA_B`). Wait 10 ms for rail stabilisation.
5. Enable I2C level shifter (`IMG_I2C_ENA`, PA11).
6. Release `RESET_BAR` HIGH, starting sensor boot sequence and internal 1.8 V regulator.
7. Wait 150 ms for sensor firmware to complete Host Config Mode boot.
8. Configure sensor via `CAM_Init()` (auto AE/AWB) or `CAM_InitAdvanced()` (manual exposure/gain).

### Memory Layout

**External SRAM (FSMC, ~4 MB)**

| Region | Size | Purpose |
|--------|------|---------|
| RAW_BUFFER 0–4 | 614,424 B each | Raw YCbCr 4:2:2 frames |
| COMPRESSED_BUFFER | 614,432 B | JPEG output staging area |

**FRAM (2 MB, SPI2)**

| Address | Content |
|---------|---------|
| `0x000000` | `board_status_t` struct (104 B) |
| `0x000068` | CRC32 of `board_status_t` |
| `0x00006C` | Compression index table (100 × 9 B entries) |
| `0x0003F0` | CRC32 of compression index table |
| `0x0003F4` | Compressed JPEG archive (~1.75 MB) |
| `0x1C0000` | Firmware backup region (256 KB reserved — see [Radiation & SEU Protection](#radiation--seu-protection)) |

`board_status_t` and the compression index table are each followed immediately by a CRC32, verified on every load from FRAM. See [Radiation & SEU Protection](#radiation--seu-protection) for the full integrity chain.

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
| `opcode[1]` | `[7:4]` advanced mode · `[3:0]` black-filter enable | Advanced: 0 = basic/AE, non-zero = manual exposure/gain · Filter: 0 = disabled, non-zero = enabled |
| `opcode[2]` | Retry count (filter enabled only) | Number of retakes before giving up |
| `opcode[3]` | Black-pixel fraction limit (filter enabled only) | 0–200 (each unit = 0.5 % of total pixels) |
| `opcode[4]` | — | Unused |

> Example — CAM B into buffer 0, no filter, basic AE: `01 00 00 00 00`

---

#### `0x34` — `CMD_TakePictureDelayed`
Schedules a burst of photos to execute after N × MIN_INTERVAL minutes. Returns `CMD_SCHEDULED` immediately and transitions to `STATE_DELAYED_PICTURE`. Burst settings (number of photos, inter-shot delay, compression) are taken from `board_status.delayed_params` (set via `CMD_ChangeBurstParams`). Any subsequent command addressed to USS cancels the scheduled burst.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | `[7:4]` buffer · `[3:0]` camera | Same encoding as `CMD_TakePicture` |
| `opcode[1]` | `[7:4]` advanced mode · `[3:0]` black-filter enable | Same encoding as `CMD_TakePicture` |
| `opcode[2]` | Retry count | Same as `CMD_TakePicture` |
| `opcode[3]` | Black-pixel fraction limit | Same as `CMD_TakePicture` |
| `opcode[4]` | Delay in intervals (N) | 1–255 · max = 255 × MIN_INTERVAL min |

---

#### `0x35` — `CMD_ChangeCamParams`
Updates a configurable camera parameter shared by both cameras and persisted in FRAM. Takes effect on the next camera initialisation.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | Parameter index | See table below |
| `opcode[1]` | New value MSB | — |
| `opcode[2]` | New value LSB | — |
| `opcode[3:4]` | — | Unused |

**Parameter index table:**

| Index | Parameter | Notes |
|-------|-----------|-------|
| 0 | Reset all to defaults | Ignores `opcode[1:2]` |
| 1 | `black_threshold` | Luma threshold for black-pixel filter (0–255) |
| 2 | `sensor_analog_gain` | Analog gain for advanced mode (unity = 32) |
| 3 | `sensor_digital_gain` | Digital gain for advanced mode (unity = 128) |
| 4 | `sensor_coarse_exposure` | Coarse integration time in line periods |
| 5 | `sensor_fine_exposure` | Fine integration time in pixel clocks |

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
Serialises `board_status_t` and `fw_backup_info_t` into the response frame and prints a full human-readable field breakdown over UART4. No opcode.

---

#### `0x3C` — `CMD_ChangeBurstParams`
Updates the burst photography parameters stored in `board_status.delayed_params` and persisted in FRAM. These settings are used by both `CMD_TakePictureDelayed` and `CMD_TakePictureBurst`.

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | Mode | 0x00 = reset all to defaults · non-zero = apply `opcode[1:4]` |
| `opcode[1]` | Number of photos | 1 to RAW_PHOTO_COUNT (5) |
| `opcode[2]` | Time between photos (seconds) | 0–255 |
| `opcode[3]` | Compress and save to FRAM? | 0 = no · non-zero = yes |
| `opcode[4]` | Compression quality | 1 = lowest · 2 = medium · 3 = highest |

---

#### `0x3D` — `CMD_TakePictureBurst`
Captures multiple raw frames immediately using the settings in `board_status.delayed_params` (number of photos, inter-shot delay, optional compression).

| Byte | Field | Values |
|------|-------|--------|
| `opcode[0]` | `[7:4]` starting buffer · `[3:0]` camera select | Buffer: 0–4 · Camera: 0 = CAM A, 1 = CAM B |
| `opcode[1]` | `[7:4]` advanced mode · `[3:0]` black-filter enable | Same encoding as `CMD_TakePicture` |
| `opcode[2]` | Retry count (filter enabled only) | Number of retakes before giving up |
| `opcode[3]` | Black-pixel fraction limit (filter enabled only) | 0–200 (each unit = 0.5 % of total pixels) |
| `opcode[4]` | — | Unused |

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
Erases the full FRAM writable region in 256-byte chunks via burst SPI writes (IWDG refreshed once per chunk). Resets all `board_status` fields and reinitialises `cam_params` and `delayed_params` to defaults. Does not touch the firmware backup region. **Requires an exact 5-byte confirmation opcode** to prevent accidental erasure.

| Byte | Required value |
|------|---------------|
| `opcode[0]` | `0x0A` |
| `opcode[1]` | `0x0F` |
| `opcode[2]` | `0x0A` |
| `opcode[3]` | `0x0F` |
| `opcode[4]` | `0x0A` |

Returns `CMD_CONFIRM_FAILED` if the opcode does not exactly match.

---

#### `0x89` — `CMD_ForceReset`
Persists `board_status` to FRAM, transmits a success response to the OBC, then triggers an immediate IWDG reset (~125 µs). No opcode. Never returns.

---

#### `0x90` — `CMD_EraseCompressions`
Erases only the FRAM photo data region (`PHOTO_DATA_START` → `FIRMWARE_BACKUP_START`) in 256-byte chunks via burst SPI writes (IWDG refreshed once per chunk), and resets the in-RAM compression table and all related `board_status` fields. **Requires an exact 5-byte confirmation opcode** to prevent accidental erasure.

| Byte | Required value |
|------|---------------|
| `opcode[0]` | `0xBA` |
| `opcode[1]` | `0xBF` |
| `opcode[2]` | `0xBA` |
| `opcode[3]` | `0xBF` |
| `opcode[4]` | `0xBA` |

Returns `CMD_CONFIRM_FAILED` if the opcode does not exactly match.

---

#### `0x91` — `CMD_BackupFirmware`
Streams the application image from internal flash to **two independent FRAM backup copies** in 256-byte chunks. Copy A is written to `FIRMWARE_IMAGE_A_START`; copy B is written to `FIRMWARE_IMAGE_B_START`. Each copy is verified by a full-image readback CRC32 (zlib-compatible, poly `0xEDB88320`) before its header (`fw_backup_size`, `fw_backup_crc32`, `fw_backup_version`) is committed — to `FIRMWARE_BACKUP_A_START` for copy A and `FIRMWARE_BACKUP_B_START` for copy B. Both copies should produce identical CRCs; a mismatch between them is logged as a warning. IWDG is refreshed between chunks in all passes. Returns `CMD_ERROR` if either readback CRC does not match. **Requires an exact 5-byte confirmation opcode.**

| Byte | Required value |
|------|---------------|
| `opcode[0]` | `0xB4` |
| `opcode[1]` | `0xC4` |
| `opcode[2]` | `0xB4` |
| `opcode[3]` | `0xC4` |
| `opcode[4]` | `0xB4` |

Returns `CMD_CONFIRM_FAILED` if opcode does not match. Returns `CMD_ERROR` if the application image size is out of range or the readback CRC does not match.

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
| 83 | `COMMAND_INDEX_FULL` | Compression index table full (100-entry max) |
| 84 | `COMMAND_CONFIRM_FAILED` | Confirmation opcode mismatch (danger zone) |
| 85 | `COMMAND_PARAM_INVALID` | Invalid value for a command parameter |

---

## Board Status & Telemetry

The `board_status_t` struct is persisted in FRAM and updated after every significant event. It includes:

- **Boot metrics:** boot count, total uptime, session uptime, requested power-downs.
- **Reset counters:** IWDG, low-power, software, POR, pin reset, unknown; `last_reset_cause` code from the most recent boot.
- **Memory flags:** FRAM and SRAM initialisation health.
- **SEU/corruption counters:** `ram_corruption_recovery` (RAM shadow CRC mismatches corrected at runtime), `fram_corruption_write_recovery` (FRAM write retries that eventually succeeded), `fram_corruption_defaulted` (FRAM CRC failures that forced a reset to safe defaults), `state_vote_fail_count` (majority-vote disagreements).
- **Last command:** instruction ID, status code, opcode bytes.
- **Operation counters:** photos taken, compressions performed, images rejected by black filter.
- **ADC readings:** MCU internal temperature, VREFINT (supply voltage reference).
- **FRAM archive state:** write pointer, compressed image count, bytes remaining.
- **Camera parameters (`cam_params`):** black threshold, analog gain, digital gain, coarse and fine exposure. Persisted so manual settings survive reboots.
- **Burst parameters (`delayed_params`):** number of photos, inter-shot interval, compression flag, compression quality.

The `fw_backup_info_t` struct (also transmitted with `CMD_GetStatus`) tracks the firmware image stored in the FRAM backup region: `fw_backup_size`, `fw_backup_crc32`, and `fw_backup_version`.

---

## Building

1. Open the project in **STM32CubeIDE**.
2. Select the **Debug** build configuration.
3. Build with **Project → Build Project** (or `Ctrl+B`).
4. Flash via ST-LINK using **Run → Debug** or the flash programmer.

The `USS.ioc` file can be opened in **STM32CubeMX** to regenerate HAL drivers or modify peripheral configuration.

---

## Radiation & SEU Protection

UNSAMSat-01 operates in LEO, where Single Event Upsets (SEU) in internal SRAM and corrupted writes to the external FRAM are realistic, if infrequent, failure modes. This section documents the protection architecture implemented against these faults, the specific scenarios it covers, the scenarios it deliberately does not cover, and the fault injection testing performed to validate it.

Ferroelectric memory cells in the FRAM are themselves largely immune to direct SEU — the polarization state of a ferroelectric capacitor is not easily flipped by a single particle strike. The realistic FRAM-side risk is instead corruption in the **peripheral CMOS logic** surrounding the memory array (SPI shift registers, address latches, write control logic) during an active transaction, or data loss from a write interrupted by an unplanned power event. Internal SRAM has no such immunity and is the more exposed memory in this design.

### Protection Layers

**1. RAM shadow CRC (`board_status` and `compression_table`)**

A CRC32 fingerprint of the non-volatile fields of `board_status` and the full `compression_table` is kept in a plain RAM global (`shadow_board_status_crc`, `shadow_compression_table_crc`), deliberately *outside* either protected struct so a corruption of the shadow itself cannot self-cancel against a corruption of the data it protects, and so the shadow is never persisted to FRAM and re-loaded as if it were trustworthy.

- `CommitBoardStatus()` / `CommitCompressionTable()` re-seal the shadow after every legitimate modification.
- `BoardStatusIntact()` / `CompTableIntact()` recompute the CRC and compare against the shadow before any FRAM write.
- The CRC deliberately excludes fields that change every loop iteration regardless of corruption (`uptime_session`, `state`, `last_reset_cause`, `fram_ok`, `sram_ok`, `last_instruction`, `last_cmd_status`, `last_opcode`, `delayed_flag`) — including them would produce constant false positives. `state` is separately protected by majority voting (below); the remaining excluded fields are low-consequence if corrupted and self-correct on the next legitimate update.
- On a mismatch, the affected struct is restored from FRAM via `RecoverBoardStatusFromFRAM()`, a counter (`ram_corruption_recovery`) is incremented, and `fram_ok` is updated to reflect the event — all visible to ground via `CMD_GetStatus`.

**Save architecture and the protection window.** `board_status`/`compression_table` are committed and saved at three distinct points, each chosen to bound how much state can be lost or go undetected if a reset occurs unexpectedly:

| Trigger | Behaviour |
|---------|-----------|
| End of `ExecuteCommand()` | Immediate commit + save. A command's effects (photo taken, compression saved, parameter changed) are never lost to an unplanned reset, even one occurring a fraction of a second later. |
| `SetState(STATE_DELAYED_PICTURE)` | Immediate commit + save, separate from the command-completion save above, since the state transition happens after `ExecuteCommand()` returns. Closes the window where a delayed photo could be silently dropped by a reset landing between scheduling and the next periodic save. |
| Main loop, every `TELEMETRY_LOOPS_TO_SAVE_TELEMETRY` iterations (~1–2 s) | Periodic save for fields that change every loop (uptime, ADC telemetry) but are low-consequence if briefly stale. Batching these writes — rather than writing on every single loop iteration — reduces the number of SPI write transactions by roughly two to three orders of magnitude versus a naive every-loop save, directly reducing exposure to the rare case of a write being interrupted mid-transaction by an unplanned reset. |

`CommitBoardStatus()`/`CommitCompressionTable()` are gated together with the periodic save (not called unconditionally every loop) so the shadow CRC's detection window matches the actual save interval — committing more often than saving would silently re-arm the shadow over corrupted data before the next save ever checked it.

**2. FRAM-resident CRC (`board_status` and `compression_table`)**

Independent of the RAM shadow, each struct's FRAM copy is followed immediately by its own CRC32, written on every save and verified on every load (`RestoreBoardStatusFromFRAM()`, shared by both `LoadBoardStatusFRAM()` at cold boot and `RecoverBoardStatusFromFRAM()` at runtime). This is the layer that catches corruption that has already reached FRAM, regardless of cause — a RAM-side fault that went undetected, a write interrupted by a power event, or peripheral-logic SEU during an SPI transaction.

- On mismatch, the affected struct is reset to safe defaults (`fram_corruption_defaulted` counter incremented, `fram_ok` and `sram_ok` correctly restored to healthy rather than left in a stale failed state).
- The two regions are verified and defaulted **independently** — a corrupt `board_status` does not force-clear a healthy `compression_table`, and vice versa. `compression_count` is recounted from the table's `valid` entries after a load if `board_status` was reset, so the two regions never disagree about how many compressions exist.
- State restore policy: only `STATE_DELAYED_PICTURE` survives a load/recovery from FRAM; every other state resets to `STATE_IDLE`, since any other state represents an in-progress transaction (mid-command, mid-response) that should not be blindly resumed after an unplanned interruption. This policy is applied identically whether the trigger was a cold boot or a runtime RAM-corruption recovery, since both ultimately trust the same FRAM source.

**3. FRAM write verification and retry**

Every write to `board_status`/`compression_table` is followed by a 4-byte readback of the just-written CRC, compared against the value the firmware intended to write. On mismatch, the full write (struct + CRC) is retried up to `FRAM_WRITE_RETRY` (3) times before giving up and setting `fram_ok = 0`. A retry that succeeds on attempt 2 or 3 increments `fram_corruption_write_recovery` rather than `fram_corruption_defaulted`, distinguishing "a write needed a retry" from "a load found FRAM data already wrong" — both are FRAM-relevant events but with different operational implications.

The same write+verify+retry pattern protects `CMD_BackupFirmware`'s image write (full readback CRC over the entire image, not just a 4-byte digest, given the consequence of a corrupted firmware backup).

**4. Brownout Reset (BOR) configuration**

The STM32F217's option bytes were found configured at `BOR_OFF` (lowest threshold, ~1.8–2.1 V) by default, meaning the MCU would continue executing — including attempting in-flight SPI/FRAM transactions — well into a decaying supply rail during a power loss, with no early warning. This was identified as the dominant cause of FRAM write corruption observed during bench testing (PSU power-off mid-write).

BOR level was raised to **Level 3 (2.70–3.60 V threshold)** via option bytes (STM32CubeProgrammer). This forces a clean hardware reset as soon as VDD drops below a safe margin, rather than allowing the chip to continue operating on marginal voltage. Confirmed via repeated bench testing (PSU power-off cycles) to eliminate the great majority of write-corruption events previously observed; residual risk from a write interrupted in the narrow window between BOR threshold and the FRAM chip's own internal power-loss behaviour is mitigated, not eliminated, by this change (see *What Is Not Protected*, below).

**5. State machine majority vote**

`board_status.state` is shadowed by two additional RAM-only copies (`state_shadow_b`, `state_shadow_c`), written together by `SetState()` and read via `GetState()`, which performs a 2-of-3 majority vote. If all three disagree (no majority), the state forces to `STATE_IDLE`, logs the event, and increments `state_vote_fail_count`. This protects the single most consequential field in `board_status` — a corrupted state value could otherwise misroute the program into reprocessing a stale command, re-entering an already-handled response, or other undefined control-flow behaviour.

**6. `compression_ptr_address` bounds check**

The FRAM write pointer for the photo archive is sanity-checked against `[PHOTO_DATA_START, FIRMWARE_BACKUP_START)` before every use in `SaveCompressionToFRAM()`. A corrupted pointer — which could otherwise cause a photo write to land inside `board_status`, the compression table, or the firmware backup region — is detected and the pointer reset to `PHOTO_DATA_START` rather than trusted.

**7. FRAM region write bounds checking**

`SaveBufferFRAM()` (metadata + photo archive region) and `SaveFRAM_Unlocked()` (firmware backup region exclusively) each enforce non-overlapping address ranges at the point of write, independent of any caller-side logic. A corrupted address argument reaching either function — from any cause — is rejected before any SPI transaction occurs, rather than relying solely on every call site getting its own bounds-checking right.

**8. Dual firmware backup (application side)**

`CMD_BackupFirmware` writes the application flash image to **two independent 128 KB regions** in FRAM, each verified by full readback CRC before its header is committed. Both copies should produce identical CRCs since they are written from the same flash source in the same command invocation; a mismatch between the two is logged as a warning. *Bootloader-side fallback logic (attempt copy A, fall back to copy B on CRC failure) is not yet implemented — see Pending Work.*

---

### What Is Protected

| Threat | Mechanism |
|--------|-----------|
| SEU flips a bit in `board_status` (non-volatile fields) while idle | RAM shadow CRC, detected within one periodic save interval (~1–2 s) |
| SEU flips a bit in `board_status` immediately after a command completes | Immediate post-command commit+save closes this window to near-zero |
| SEU flips a bit in `compression_table` | Same RAM shadow CRC mechanism, independent of `board_status` |
| SEU flips a bit in `board_status.state` specifically | Majority vote across 3 RAM copies |
| FRAM already holds corrupted `board_status`/`compression_table` data (any cause) at boot or at the moment of a runtime recovery | FRAM-resident CRC, verified on every load, resets to safe defaults |
| A FRAM write is corrupted in transit (data does not match what was intended) | Write + 4-byte CRC readback + up to 3 retries |
| A FRAM write is interrupted by an unplanned reset/brownout | BOR Level 3 forces early, clean reset before voltage drops far enough to corrupt an in-flight SPI transaction; reduced write frequency (periodic telemetry batching) lowers the number of opportunities for this to occur |
| `compression_ptr_address` corrupted, pointing outside the legal photo archive range | Bounds check before every use in `SaveCompressionToFRAM()` |
| A corrupted FRAM address reaches a write function from any cause | Region-exclusive bounds checking in `SaveBufferFRAM()` / `SaveFRAM_Unlocked()` |
| `boot_count` lost to a reset occurring within seconds of boot | Immediate save after `LoadBoardStatusFRAM()`, not deferred to the periodic interval |
| A scheduled delayed photo lost to a reset occurring within seconds of scheduling | Immediate save on `SetState(STATE_DELAYED_PICTURE)`, validated with worst-case timing (fault injected on the very first loop iteration after the transition — see Test 8) |
| Firmware backup image corrupted at write time | Full-image readback CRC verification, header only committed after verification passes; two independent copies written |

### What Is NOT Protected

This list is deliberately explicit. These are known, accepted gaps — either because the residual risk is judged acceptable given measured probabilities, or because closing them would require disproportionate complexity for the marginal benefit.

- **Corruption occurring strictly inside a command handler's execution, before it returns.** The RAM shadow CRC is only re-sealed at the start and end of a command (and at periodic intervals during idle); a fault landing between two legitimate modifications *within* a single command's body is invisible to the shadow CRC and gets silently sealed in as if it were legitimate, then persisted to FRAM on the immediate post-command save. **Tested and confirmed** (Test 4) — a fault injected mid-`CMD_CompressRawPhoto` produced no detection and a silently wrong `photos_taken` value. Given the small size of the protected structs relative to total SRAM, and that most commands complete in well under a few seconds, the probability of an SEU landing in this specific window is judged negligible for the mission's duration and orbit (see *Probability Estimate*, below) and is accepted as residual risk rather than engineered around.
- **SPI peripheral-level faults during transmission, distinct from data already wrong in storage.** None of `SaveBufferFRAM()`, `ReadBufferFRAM()`, or `SaveFRAM_Unlocked()` check the `HAL_StatusTypeDef` return value of the underlying `HAL_SPI_Transmit`/`HAL_SPI_Receive` calls. A timeout or bus-level fault returned by the HAL is not distinguished from a transaction the firmware assumes succeeded. In practice this is substantially mitigated by the existing write-readback-CRC-retry mechanism, which will independently detect the *downstream symptom* (wrong data in FRAM) regardless of cause — but a HAL-level failure on a *read* (e.g. arbitrary photo data fetched for downlink via `CMD_SendCompFrame`, which has no CRC wrapper) would go entirely unnoticed.
- **Address-byte corruption in transit.** If the 3 address bytes of an SPI WRITE/READ command are corrupted during transmission (rather than the data bytes), a write could land at a completely different FRAM address than intended. The bounds checks in `SaveBufferFRAM()`/`SaveFRAM_Unlocked()` validate the *intended* address before the transaction begins; they cannot validate what was actually clocked onto the SPI bus. Not tested, not specifically mitigated beyond the general write-retry-verify pattern, which would only catch this if the subsequent readback happens to read back from the *intended* (not actual) address and finds it unexpectedly unchanged or wrong.
- **CS (chip select) timing glitches.** An interrupt or fault disrupting `FRAM_CS_LOW()`/`FRAM_CS_HIGH()` timing mid-transaction could truncate or merge SPI operations. Not tested, no specific mitigation beyond the general write-verify-retry pattern.
- **Raw photo and compressed photo header CRCs are computed but not verified.** Both `raw_photo_t` and `compressed_photo_t` carry a `header_crc` field, populated at capture/compression time, but no command currently checks it before transmitting header data to the ground (`CMD_SendRawHeader`, `CMD_DumpRaw`, `CMD_SendCompHeader`, `CMD_DumpCompressed`). A corrupted SRAM header would be downlinked as-is. Deliberately deprioritised: SRAM is significantly larger than the protected FRAM structs, the consequence of a corrupted header (wrong timestamp/designator on one photo) is low relative to `board_status`/`compression_table` corruption, and the JPEG pixel data itself is unaffected either way.
- **Pixel data integrity.** No CRC protects the actual image payload in either the raw SRAM buffers or the compressed JPEG data in FRAM. A corrupted pixel would simply appear as a visual artifact in the downlinked image; no detection or correction is attempted.
- **Multi-bit / multiple-cell upsets (MCU/MBU).** All CRC-based detection in this design assumes single-bit-flip-class corruption is the dominant failure mode, consistent with the FRAM peripheral-logic and SRAM SEU literature for this mission's orbit and shielding. CRC32 cannot distinguish all possible multi-bit corruption patterns from a valid message (a sufficiently adversarial multi-bit pattern could theoretically produce a CRC collision), though this is an extremely low-probability concern for naturally-occurring radiation-induced upsets as opposed to deliberately crafted data.
- **Bootloader-side dual-backup fallback.** The application writes two independent firmware backup copies (Protection 8, above), but the bootloader does not yet attempt copy B if copy A's CRC fails — it currently only knows about a single backup region. This is the last item on the pending work list.

### Probability Estimate (SRAM SEU)

In-flight SEU measurements on comparable LEO missions report on the order of 0.19–0.85 errors/day for a 4 Mbit SRAM array. Scaled to the STM32F217's 1 Mbit internal SRAM, this is roughly one upset every 5–20 days across the *entire* SRAM. `board_status` occupies 104 bytes — approximately 0.006% of total internal SRAM — so a bit flip landing specifically within `board_status` is estimated at roughly once every several hundred years of continuous operation. The unprotected command-execution window (Test 4) is open for at most a few seconds per command; even summed across a realistic mission's total command count, the cumulative exposure remains a small fraction of total mission time. This estimate is the basis for accepting the command-execution-window gap as residual risk rather than engineering further mitigation for it.

---

### Fault Injection Test Matrix

All tests below were performed using temporary, code-level fault injection helpers (`DEBUG_CorruptBoardStatus()`, `DEBUG_CorruptCompressionTable()`, `DEBUG_CorruptFRAMStatus()`), inserted at specific points in the source and removed after validation — **no fault injection code ships in the flight build.**

| # | Scenario | Result |
|---|----------|--------|
| 1 | RAM corruption (`board_status`) injected during `STATE_IDLE` | **PASS** — detected and recovered within one periodic save interval |
| 2 | RAM corruption (`board_status`) injected immediately after a command completes | **PASS** — detected and recovered; confirmed no duplicate command execution (validates the state-restore-to-`STATE_IDLE` fix, see below) |
| 3 | RAM corruption (`compression_table`) injected during `STATE_IDLE` and post-command, cross-checked against real compressed photo data via `CMD_SendCompHeader` | **PASS** — both placements detected and recovered; restored header values matched the pre-fault baseline exactly |
| 4 | RAM corruption (`board_status`) injected *inside* a command body (mid-`CMD_CompressRawPhoto`), before the command returns | **Confirmed undetected** — documents the known, accepted-risk gap (see *What Is NOT Protected*) |
| 5 | FRAM directly corrupted (bypassing RAM entirely), followed by a reset | **PASS** — detected on next boot via FRAM-resident CRC, defaults applied correctly, `fram_corruption_defaulted` incremented, `compression_table` region independently confirmed unaffected |
| 6 | Multiple consecutive clean resets (`CMD_ForceReset` and power-on reset) with no fault injected | **PASS** — zero false-positive CRC failures, `boot_count` incremented correctly every cycle, all persisted data intact |
| 7 | `CMD_TakePictureDelayed` scheduled, then a genuine power-on reset (no intervening command) before the delay elapsed | **PASS** — `STATE_DELAYED_PICTURE` and timing correctly restored from FRAM on cold boot; delayed burst fired at the correct time post-reboot |
| 8 | `CMD_TakePictureDelayed` scheduled, RAM corruption injected on the very first loop iteration after entering `STATE_DELAYED_PICTURE` (worst-case timing, probing the immediate-save fix) | **PASS** — recovery correctly found `STATE_DELAYED_PICTURE` already persisted in FRAM; full burst (4 photos, 4 compressions) fired at the exact scheduled time despite the corruption landing essentially simultaneously with the state transition |

**Bugs found and fixed during this testing process** (documented here since they shaped the final architecture):

- An early version of the periodic-save architecture committed the RAM shadow CRC unconditionally every loop while only *saving* periodically — this silently defeated detection entirely, since the shadow was continuously re-armed over corrupted data before the infrequent save ever checked it. Fixed by gating commit and save together.
- `LoadBoardStatusFRAM()`/`RecoverBoardStatusFromFRAM()` originally restored *any* persisted state value on load, including mid-transaction states like `STATE_EXECUTE_COMMAND` — causing a command to silently re-execute after a RAM-corruption recovery, since the opcode and instruction number were still resident in memory. Fixed by restricting state restoration to `STATE_DELAYED_PICTURE` only; every other state resets to `STATE_IDLE`.
- `boot_count` and the `STATE_DELAYED_PICTURE` transition were initially only persisted by the periodic telemetry save, creating a window (up to the full periodic interval) where a reset occurring shortly after either event would silently lose it. Fixed by giving both their own immediate save, independent of the periodic interval.
- The system's default option byte configuration left BOR effectively disabled (`BOR_OFF`), allowing the MCU to continue executing on a decaying supply rail during PSU power-off testing, producing intermittent FRAM write corruption. Fixed by raising BOR to Level 3 via option bytes.

---

## Pending Work

- **Address-byte and CS-timing fault tolerance** — No specific mitigation beyond the general write-verify-retry pattern protects against SPI address corruption in transit or chip-select timing glitches mid-transaction. Not tested.
- **Photo header CRC verification** — `header_crc` is computed and stored for both raw and compressed photo headers but never checked before downlink. Deliberately deprioritised; see *What Is NOT Protected*.
