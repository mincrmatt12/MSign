FROM python:2.7-alpine

RUN pip install platformio

RUN pio platforms install ststm32 --with-package framework-stm32cube && pio platforms install espressif8266 --with-package framework-arduinoespressif8266
