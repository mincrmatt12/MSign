FROM python:2.7

RUN apt-get update && apt-get install git

RUN pip install platformio

RUN pio platforms install ststm32 --with-package framework-stm32cube && pio platforms install espressif8266@2.2.2 --with-package framework-arduinoespressif8266 

ENV HOME /home/jenkins
