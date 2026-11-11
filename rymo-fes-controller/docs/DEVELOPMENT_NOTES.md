# Development notes

## Source provenance

`firmware/main.c` was extracted from the supplied `Fes__code_documentatin(readable).docx`. The source document did not include the SEGGER project or the accompanying Nordic SDK configuration files.

The prepared listing makes three source-level changes:

- Added `<stdio.h>` for the existing `sscanf` and `sprintf` calls.
- Removed a duplicate global declaration of `m_tx_buffer` that appeared twice in the document.
- Added an input-length check before copying a BLE NUS command into a fixed 50-byte buffer.

These edits do not supply the missing SDK project setup and do not establish that the firmware builds or is safe to operate. No build or hardware test was performed for this package.

## Protocol discrepancy

The project report describes a three-field command: `frequency:pulse_width:voltage`. The C listing parses four integer fields: `frequency:pulse_width:dac_code:channel`. It does not document the voltage conversion. Verify the mobile sender and DAC scaling before using the code.

## RTT logger behavior

`host/rtt_logger.py` connects to `127.0.0.1:19021`, parses RTT lines containing `app: <integer>`, plots a rolling time window, and prints samples to the terminal. It does not write a CSV or other file, despite the project report describing data storage. The script labels the plot `100 SPS`, but it does not measure or enforce a 100-sample-per-second rate.

## Workbook and PDF

`analysis/Graphs_All_Loads.xlsx` contains five measurement tables (`0.5k`, `2k`, `3k`, `4k`, and `10k`) and five chart sheets. The measurement fields are frequency, pulse width, DAC code, and raw ADC value. The graph sheets use `SORT` and `UNIQUE` dynamic-array formulas. `analysis/Graphs_All_Loads.pdf` is a 52-page print export with raw table pages and one chart on the last page. The workbook is the more useful analysis source. Three embedded chart caches contain fewer stored point values than the row counts referenced by their series, so recalculate and visually review all five charts in a current Excel version before relying on them.

## Missing build inputs

To turn the reference listing into a reproducible firmware project, obtain and document at least:

- The original SEGGER Embedded Studio project and target configuration.
- The exact nRF5 SDK and SoftDevice versions.
- `sdk_config.h` and any SDK configuration headers.
- Startup, linker, board, and build-system files.
- Pin mappings and the schematic for the final PCB.
- The expected BLE command sender and DAC voltage mapping.

## Reported validation

The supplied report says the team checked pulse frequency, width, and voltage with an oscilloscope; used a logic analyzer to inspect timing; monitored ADC data through RTT/J-Link; and characterized known resistor loads. These are reported procedures, not independently repeated results.

## Safety scope

This repository documents an engineering prototype only. It contains no clinical protocol, patient-use procedure, or evidence of medical-device certification. Do not connect the prototype to a person.

