import socket
import re
import sys
import time

from collections import deque

from PyQt5 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg


# ============================================================
# SETTINGS
# ============================================================

RTT_HOST = "127.0.0.1"
RTT_PORT = 19021

SAMPLE_RATE_HZ = 100

DISPLAY_SECONDS = 10

MAX_SAMPLES = SAMPLE_RATE_HZ * DISPLAY_SECONDS

PLOT_UPDATE_MS = 50


# ============================================================
# ADC DATA
# ============================================================

adc_values = deque(maxlen=MAX_SAMPLES)
time_values = deque(maxlen=MAX_SAMPLES)

start_time = time.perf_counter()

rx_buffer = b""


# ============================================================
# REGEX
# ============================================================

# Your firmware:
#
# NRF_LOG_INFO("%d", value);
#
# RTT output:
#
# <info> app: 1832
# <info> app: -5
#
ADC_PATTERN = re.compile(
    rb"app:\s*(-?\d+)"
)


# ============================================================
# PYQT APPLICATION
# ============================================================

app = QtWidgets.QApplication(sys.argv)

# Use Fusion style
app.setStyle("Fusion")


# ============================================================
# DARK QT PALETTE
# ============================================================

dark_palette = QtGui.QPalette()

dark_palette.setColor(
    QtGui.QPalette.Window,
    QtGui.QColor(25, 25, 25)
)

dark_palette.setColor(
    QtGui.QPalette.WindowText,
    QtGui.QColor(230, 230, 230)
)

dark_palette.setColor(
    QtGui.QPalette.Base,
    QtGui.QColor(15, 15, 15)
)

dark_palette.setColor(
    QtGui.QPalette.AlternateBase,
    QtGui.QColor(35, 35, 35)
)

dark_palette.setColor(
    QtGui.QPalette.ToolTipBase,
    QtGui.QColor(255, 255, 255)
)

dark_palette.setColor(
    QtGui.QPalette.ToolTipText,
    QtGui.QColor(0, 0, 0)
)

dark_palette.setColor(
    QtGui.QPalette.Text,
    QtGui.QColor(230, 230, 230)
)

dark_palette.setColor(
    QtGui.QPalette.Button,
    QtGui.QColor(45, 45, 45)
)

dark_palette.setColor(
    QtGui.QPalette.ButtonText,
    QtGui.QColor(230, 230, 230)
)

dark_palette.setColor(
    QtGui.QPalette.BrightText,
    QtGui.QColor(255, 255, 255)
)

dark_palette.setColor(
    QtGui.QPalette.Highlight,
    QtGui.QColor(70, 70, 70)
)

dark_palette.setColor(
    QtGui.QPalette.HighlightedText,
    QtGui.QColor(255, 255, 255)
)

app.setPalette(dark_palette)


# ============================================================
# PYQTGRAPH DARK THEME
# ============================================================

pg.setConfigOption(
    "background",
    (15, 15, 15)
)

pg.setConfigOption(
    "foreground",
    "w"
)


# ============================================================
# CONNECT TO J-LINK RTT
# ============================================================

print("Connecting to J-Link RTT...")

sock = socket.socket(
    socket.AF_INET,
    socket.SOCK_STREAM
)

sock.settimeout(5)

try:

    sock.connect(
        (
            RTT_HOST,
            RTT_PORT
        )
    )

except Exception as e:

    print()
    print("======================================")
    print("Could not connect to J-Link RTT")
    print("======================================")
    print()
    print("Make sure JLink.exe is running.")
    print()
    print("Expected:")
    print("127.0.0.1:19021")
    print()
    print("Error:")
    print(e)

    sys.exit(1)


print("Connected to J-Link RTT.")
print("Waiting for ADC data...")

sock.settimeout(0.01)


# ============================================================
# MAIN WINDOW
# ============================================================

window = QtWidgets.QMainWindow()

window.setWindowTitle(
    "nRF52832 SAADC - RTT / SWD"
)

window.resize(
    1100,
    650
)


# ============================================================
# PLOT WIDGET
# ============================================================

plot_widget = pg.PlotWidget()

plot_widget.setTitle(
    "nRF52832 ADC - 100 SPS",
    color="w",
    size="14pt"
)

plot_widget.setLabel(
    "left",
    "ADC Value",
    color="w"
)

plot_widget.setLabel(
    "bottom",
    "Time",
    units="s",
    color="w"
)

plot_widget.showGrid(
    x=True,
    y=True,
    alpha=0.25
)


# ============================================================
# FIXED Y RANGE
# ============================================================

plot_widget.setYRange(
    -100,
    5000,
    padding=0
)


# ============================================================
# PLOT CURVE
# ============================================================

curve = plot_widget.plot(
    pen=pg.mkPen(
        color=(0, 255, 255),
        width=2
    )
)


# ============================================================
# SET WINDOW
# ============================================================

window.setCentralWidget(
    plot_widget
)

window.show()


# ============================================================
# READ RTT
# ============================================================

def read_rtt():

    global rx_buffer

    try:

        while True:

            try:

                data = sock.recv(4096)

            except socket.timeout:

                break

            if not data:

                print("RTT connection closed.")

                app.quit()

                return


            rx_buffer += data


            # ------------------------------------------------
            # FIND ADC VALUES
            # ------------------------------------------------

            matches = list(
                ADC_PATTERN.finditer(
                    rx_buffer
                )
            )


            if not matches:

                # Keep buffer from growing indefinitely.
                if len(rx_buffer) > 4096:
                    rx_buffer = rx_buffer[-1024:]

                break


            # ------------------------------------------------
            # EXTRACT VALUES
            # ------------------------------------------------

            for match in matches:

                try:

                    value = int(
                        match.group(1)
                    )

                except ValueError:

                    continue


                timestamp = (
                    time.perf_counter()
                    - start_time
                )


                adc_values.append(
                    value
                )

                time_values.append(
                    timestamp
                )


                # Print received value
                print(
                    f"{timestamp:8.3f} s    "
                    f"ADC = {value}"
                )


            # ------------------------------------------------
            # REMOVE PROCESSED DATA
            # ------------------------------------------------

            last_end = matches[-1].end()

            rx_buffer = rx_buffer[last_end:]


            # Stop this read cycle if packet was small
            if len(data) < 4096:

                break


    except Exception as e:

        print(
            "RTT read error:",
            e
        )


# ============================================================
# UPDATE GRAPH
# ============================================================

def update_plot():

    # Read incoming RTT data
    read_rtt()


    if len(adc_values) == 0:

        return


    # Copy deque data
    x = list(time_values)

    y = list(adc_values)


    # --------------------------------------------------------
    # UPDATE CURVE
    # --------------------------------------------------------

    curve.setData(
        x,
        y
    )


    # --------------------------------------------------------
    # ROLLING X AXIS
    # --------------------------------------------------------

    latest_time = x[-1]

    left_time = max(
        0,
        latest_time - DISPLAY_SECONDS
    )

    right_time = max(
        DISPLAY_SECONDS,
        latest_time
    )


    plot_widget.setXRange(
        left_time,
        right_time,
        padding=0
    )


# ============================================================
# TIMER
# ============================================================

timer = QtCore.QTimer()

timer.timeout.connect(
    update_plot
)

timer.start(
    PLOT_UPDATE_MS
)


# ============================================================
# CLEAN EXIT
# ============================================================

def cleanup():

    try:

        sock.close()

    except Exception:

        pass


app.aboutToQuit.connect(
    cleanup
)


# ============================================================
# START APPLICATION
# ============================================================

sys.exit(
    app.exec_()
)