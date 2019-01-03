FROM python:2.7-alpine

RUN apk add --no-cache python3
RUN pip2 install platformio
