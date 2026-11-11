# Hardware and interfaces

This map is transcribed from the supplied C listing and project report. Confirm every connection against the actual board schematic and firmware configuration before hardware work.

## Components and peripherals

| Function | Source reference |
|---|---|
| MCU / development platform | Nordic nRF52832; report names PCA10040 for development |
| Pulse timing | TIMER3 for channel 1; TIMER4 for channel 2 |
| Peripheral links | PPI links TIMER compare events to GPIOTE and SAADC tasks |
| GPIO switching | GPIOTE tasks drive high-side and low-side control pins |
| Analog acquisition | SAADC input AIN0 for channel 1; AIN1 for channel 2 |
| DAC interface | SPI0, configured for mode 0 and 1 MHz; source comments name MCP4921 |
| Wireless control | BLE Nordic UART Service (NUS) |
| Debug/data path in report | SWD/J-Link and RTT for host-side ADC monitoring |

## GPIO assignments in the listing

| Signal | GPIO |
|---|---:|
| Channel 1 high-side control | 12 |
| Channel 1 low-side control | 11 |
| Channel 2 high-side control | 25 |
| Channel 2 low-side control | 26 |
| SPI clock (SCK) | 29 |
| SPI data out (MOSI) | 30 |
| DAC 1 chip select | 28 |
| DAC 2 chip select | 27 |

The listing does not document the complete analog front end, output-stage schematic, electrode interface, isolation design, or protection circuitry. Do not infer these details from the photographs.

## Command fields

The C listing parses four integer fields: `frequency_hz:pulse_width_us:dac_code:channel`. It clamps frequency to 1-150, pulse width to 10-400 microseconds, DAC code to 0-4095, and channel to 1 or 2. These are software clamps in the supplied listing only; they are not validated clinical or electrical safety limits.

The report describes a different three-field command using a voltage value. The voltage conversion and mobile-app command source must be verified against the original project.
