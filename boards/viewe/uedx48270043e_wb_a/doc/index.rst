.. zephyr:board:: uedx48270043e_wb_a

Overview
********

The VIEWE UEDX48270043E-WB-A is a 4.3" 480x272 parallel RGB TFT display module with an
ESP32-S3-WROOM-1-N16R8 MCU and Goodix GT911 capacitive touch panel.

Key features include:

- ESP32-S3 dual-core Xtensa LX7 @ 240 MHz (``ESP32-S3-WROOM-1-N16R8``)
- 16 MB Quad SPI Flash, 8 MB Octal SPI PSRAM
- 4.3" 480x272 RGB565 16-bit parallel TFT display (ILI6485A / ST7283, 12 MHz PCLK)
- Goodix GT911 5-point capacitive touch controller (I2C0 on GPIO19/GPIO20, RST on GPIO38)
- MicroSD card slot over SPI2 (CS=GPIO10, MOSI=GPIO11, SCLK=GPIO12, MISO=GPIO13)
- WS2812B addressable RGB LED (GPIO0, shared with BOOT button)
- USB Type-C with CH340C UART0 bridge (TX=GPIO43, RX=GPIO44) and auto-download circuit
- Boot and Reset buttons, plus dual expansion headers

Hardware
********

.. include:: ../../../espressif/common/soc-esp32s3-features.rst
   :start-after: espressif-soc-esp32s3-features

Supported Features
==================

.. zephyr:board-supported-hw::

System Requirements
*******************

.. include:: ../../../espressif/common/system-requirements.rst
   :start-after: espressif-system-requirements

Programming and Debugging
*************************

.. zephyr:board-supported-runners::

.. include:: ../../../espressif/common/building-flashing.rst
   :start-after: espressif-building-flashing
