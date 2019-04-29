FROM python:2.7

RUN apt-get update && apt-get install git

RUN pip install platformio

RUN pio platforms install ststm32 --with-package framework-stm32cube && pio platforms install espressif8266@1.8.0 --with-package framework-arduinoespressif8266

ENV HOME /home/jenkins
