# Protocol-Flexible Custom NFC Sensor Tag

Hardware design files and firmware for the sensor tag described in:

> R. Maeda and A. Noda, "Protocol-Flexible Custom NFC for Wire-Free Wearable Sensor Networks,"
> *IEEE Sensors Letters*, under review.

An NFC sensor tag whose **communication protocol is defined in software** rather than fixed in a
transponder IC. The tag exposes the baseband modulation and demodulation signals to a
general-purpose microcontroller, so the medium access control layer, the line coding, and the
subcarrier frequency can all be changed by reflashing firmware.

This is a research prototype for protocol exploration. It is not optimized for power
(17 mW, battery-powered) and it does not implement, and is not interoperable with, ISO/IEC 14443.

## Contents

```
hardware/
  schematic/tag-schematic.pdf   Tag schematic (KiCad export)
  gerber/                       Gerber and drill files for the tag PCB
firmware/
  tag_sensor/                   Tag firmware (Arduino sketch, ATmega328P)
    tag_sensor.ino              Polling response loop
    uart_rz.ino                 RZ encode/decode over the UART, CRC-8
    adxl367.ino                 ADXL367 accelerometer driver (I2C)
  reader/                       Reader firmware (Arduino sketch, STM32duino)
    reader.ino                  ST25R3916B setup, polling schedule, logging
    uart_rz.ino                 Same RZ codec, bound to the NFC UART
```

Component values are on the schematic; no separate bill of materials is provided.

## Hardware

| Function | Part |
| --- | --- |
| Microcontroller | ATmega328P |
| Accelerometer | ADXL367, via I2C |
| RF switch (load modulation) | MASWSS0204 |
| Envelope detector, also the backscatter load | LTC5507 |
| Data slicer | BU5265HFV |
| Subcarrier oscillator | LTC6900 |
| Subcarrier gating | 74LVC2G74 flip-flop and TC7SH00 gate |

Board size 30 mm x 30 mm excluding the antenna.

The **LTC5507 serves two roles at once**: its RF input is one of the two states of the RF switch, so
the same node is both the envelope detector input and the modulated load. This is the least obvious
part of the circuit.

## Firmware

### Tag

Arduino sketch for the ATmega328P. Set `TAG_NUM` (0, 1, 2, ...) to give each tag a distinct address
before flashing.

The tag MCU runs from its internal 8 MHz RC oscillator, with no external crystal, so it is not in
the stock Arduino board list. We use the **ATmega328 on a breadboard (8 MHz internal clock)**
definition distributed by Arduino as `breadboard-1-6-x.zip`, available from
<https://www.arduino.cc/en/uploads/Tutorial/breadboard-1-6-x.zip> and described at
<https://docs.arduino.cc/built-in-examples/arduino-isp/ArduinoToBreadboard/>. It is not redistributed
here, since its licence is not stated.

That page describes a procedure for an older IDE, which places the definition in the sketchbook
`hardware/` folder. With IDE 2.x this did not work for us. What did work was extracting the archive
into the Arduino data directory instead, giving `.../packages/breadboard/avr/` containing
`boards.txt`, `bootloaders/` and `variants/`:

| Platform | Directory |
| --- | --- |
| Windows | `%LOCALAPPDATA%\Arduino15\packages\` |
| Linux | `$HOME/.arduino15/packages/` |
| macOS | `~/Library/Arduino15/packages/` |

We tested this on Windows only. The Linux and macOS paths are the standard Arduino data directories
for those platforms, but we have not confirmed that the procedure works there.

Restart the IDE, select the breadboard board, and burn the bootloader once before uploading the
sketch. For reference, the definition sets `f_cpu` to 8 MHz and the fuses to `low = 0xE2`,
`high = 0xDA`, `extended = 0x05`, i.e. internal RC without the divide-by-8 prescaler and brown-out
detection at 2.7 V.

The 8 MHz clock is not arbitrary: it divides exactly into the 100 kbaud UART rate (UBRR = 4 with
U2X0 cleared), so the link runs with no baud-rate error. A clock that does not divide exactly would
introduce a timing offset that the RZ decoder has no way to absorb.

Two points that are easy to miss when reading the code:

- **`Serial.begin(100000)`** sets the UART to 100 kbaud. Each data bit is expanded to two UART bits
  by `uart_rz.ino`, so the effective rate is 50 kbaud.
- **The tag transmits before it reads the sensor.** `loop()` sends the stored reply first and reads
  the accelerometer afterwards, so the reply carries the sample taken during the previous poll.
  This keeps sensor access outside the transmit/receive turnaround, at the cost of one polling
  cycle of sample age.

RZ coding maps each data bit to two UART bits (`0` to `10`, `1` to `11`), so an 8-bit value occupies
two UART bytes. `uart_rz_read()` recovers byte alignment by testing candidate offsets against the
encoding table. The CRC is CRC-8 with polynomial 0x07 and zero initial value, computed over the
three acceleration bytes.

### Reader

`firmware/reader/` is an Arduino sketch for the NUCLEO-L476RG, built with the STM32duino core. It
configures the ST25R3916B over SPI and then hands the link to the MCU UART:

- `write_register(0x28, 0b00100000)` sets the AM modulation index to 10% and the driver resistance
  to 1 ohm
- `write_cmd(0xdc)` puts the reader IC into transparent mode, after which the baseband signals
  appear on the UART pins (PB10/PB11, with TX and RX inverted)
- the polling loop holds each slot to 1500 us with `while (micros() - start_time < 1500)`

`ERR_COUNT` selects the mode. When it is defined, the sketch counts failed polls per tag and prints
the tally instead of the samples; when it is undefined, it prints timestamped acceleration samples.
It is set to 1000 here, the value used for the packet-error-rate measurements reported in the paper,
so each plotted point is the failure count out of 1000 polls of that tag.

Built with Arduino IDE 2.3.10 and STM32duino core 2.12.0, with the following board settings:

| Setting | Value |
| --- | --- |
| Board | Nucleo-64 |
| Board part number | Nucleo L476RG |
| Upload method | Mass Storage |
| Optimize | Smallest (-Os default) |
| C Runtime Library | Newlib Nano (default) |
| USB support | None |
| U(S)ART support | Enabled (no generic 'Serial') |
| USB speed | Low/Full Speed |
| Debug symbols and core logs | None |

The `U(S)ART support: Enabled (no generic 'Serial')` setting matters: the sketch declares its own
`HardwareSerial` instances for the NFC link (PB11/PB10) and the ST-LINK virtual COM port (PA3/PA2),
and does not use a generic `Serial` object.

## Protocol

| | |
| --- | --- |
| Carrier | 13.56 MHz |
| Reader to tag | 10% ASK |
| Tag to reader | Load modulation, on-off keying on a 1 MHz subcarrier |
| Line code | Return-to-zero, generated by expanding UART output |
| UART baud rate | 100 kbaud (50 kbaud effective after RZ) |
| Request frame | `'T'` followed by `'0' + TAG_NUM` (2 bytes) |
| Reply frame | 3 acceleration bytes and 1 CRC byte (4 bytes) |
| Slot time | 1.5 ms per tag |
| Polling rate | 222 Hz per tag with three tags |

The subcarrier is 1 MHz rather than the ISO/IEC 14443 value of 847.5 kHz. The system does not target
interoperability with the standard, and 1 MHz gives an integer ten subcarrier periods per 10 us
symbol at 100 kbaud.

## Reader

The reader is commercial hardware used without modification, so no design files are included here:

- **X-NUCLEO-NFC08A1** (STMicroelectronics), carrying an **ST25R3916B** NFC reader IC and a
  47 mm x 34 mm four-turn 13.56 MHz printed antenna
- **NUCLEO-L476RG** (STMicroelectronics) as the host

The ST25R3916B was selected for its transparent mode, which bypasses the on-chip framing and coding
and routes the raw baseband signals to the host microcontroller. Schematics and BOM for both boards
are published by STMicroelectronics.

The automatic antenna tuning (AAT) of the ST25R3916B is **not** used. Apart from the register writes
in `reader.ino`, the IC runs with its default register settings. This is worth knowing when
reproducing the measurements, since the reader antenna is coupled to a large meander coil and the
impedance it sees differs substantially from the stand-alone case.

## Meander coil

400 mm x 260 mm, copper tape on a fabric substrate, divided into six sections by series capacitors.
Measured with the loop opened at the centre of its longest straight segment: L = 1.7 uH, and with
82.5 pF in series (six sets of 495 pF, each 390 + 75 + 30 pF in parallel) the resonance is at
13.44 MHz with |Z11| = 6.45 ohm, Q = 22.3 and a -3 dB bandwidth of 604 kHz.

The coil follows prior work cited in the paper and is not a contribution of this repository.

## License

MIT License, see `LICENSE`. This covers the whole repository, including the hardware design files
in `hardware/` as well as the firmware.

## Citing

See `CITATION.cff`. Please cite the Letter rather than this repository.
