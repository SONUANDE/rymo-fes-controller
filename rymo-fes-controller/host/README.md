# RTT logger

`rtt_logger.py` opens a live plot of ADC values read from a local TCP socket exposed by SEGGER J-Link RTT. It connects to `127.0.0.1:19021`, parses lines matching the `app:` log format, keeps a rolling ten-second display, and prints each received value to the terminal.

The script does not write CSV or other data files. The project report describes data logging, but that behavior is not present in this supplied script. Preserve terminal output separately if you need a record, or add and validate an explicit file-export feature before relying on it.

## Requirements

- Python
- The packages listed in `requirements.txt`
- SEGGER J-Link running with the RTT TCP service available at `127.0.0.1:19021`

Install the Python packages from this folder:

```text
python -m pip install -r requirements.txt
```

Then start the J-Link RTT service and run:

```text
python rtt_logger.py
```

The plotted title says 100 SPS, but the script does not measure or enforce the incoming sample rate. The display polling interval is 50 ms and the plot keeps approximately ten seconds of received samples.
