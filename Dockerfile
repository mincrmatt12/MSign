FROM python:2.7

RUN pip install platformio
RUN apt-get update && apt-get install git

RUN adduser --quiet pio
USER pio
ENV HOME /home/pio
