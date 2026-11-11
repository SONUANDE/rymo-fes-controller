# Project overview

## Purpose

The project report describes an nRF52832-based FES controller that accepts output settings over Bluetooth Low Energy (BLE), generates timed output pulses, selects between two output channels, and acquires raw ADC feedback associated with current consumption.

The source material presents this as an internship engineering prototype. It does not establish clinical validation, medical-device certification, or safe human-use limits.

## Firmware path

1. A BLE central sends a command through Nordic UART Service (NUS).
2. The firmware parses frequency, pulse width, DAC code, and channel.
3. Channel selection configures the corresponding SAADC input and timer path.
4. TIMER compare events drive PPI links and GPIOTE tasks for output switching and sampling.
5. SPI writes the DAC command for the selected output channel.
6. The firmware formats a raw ADC sample and sends it over BLE NUS.

The supplied host script, `host/rtt_logger.py`, separately connects to a local J-Link RTT TCP service and plots values parsed from RTT log lines. It prints the received values but does not save them to a data file. The project report describes a broader logging workflow, so do not assume the supplied script includes every feature described in that report.

## Measurement analysis

`analysis/Graphs_All_Loads.xlsx` contains five measurement tables and five chart sheets for the load cases named `0.5k`, `2k`, `3k`, `4k`, and `10k`. The workbook has about 1.49 million measurement rows total. The supplied PDF is a 52-page print export, mostly raw rows, with one chart on its final page. See [analysis notes](../analysis/README.md).

The report says the team used known resistor loads and checked pulse output with an oscilloscope and timing with a logic analyzer. Those are statements from the report and were not independently reproduced while preparing this repository.

The report notes that measurable current was not observed below approximately 14 microseconds in its test setup. This is a reported observation, not a device limit or safety specification.

## Supplied material gaps

The original gallery archive contained a Word document with the C source listing, a project report PDF, photographs, and three videos. The additional supplied files add the analysis workbook, its PDF export, and an RTT plotting script. The package still lacks the original SEGGER project, SDK configuration, complete build files, and raw logger output files. The firmware remains a source reference, not a complete buildable project.
