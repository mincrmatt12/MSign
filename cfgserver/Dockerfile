FROM debian:trixie-slim AS py-build

RUN apt-get update \
 && apt-get install -y --no-install-recommends --no-install-suggests \
            ca-certificates \
 && update-ca-certificates \
 && apt-get install -y python3 python3-venv python3-pip python3-dev build-essential gcc

RUN python3 -m venv /opt/venv
ENV PATH "/opt/venv/bin:$PATH"

WORKDIR /opt/build

# install deps
COPY requirements.txt .
RUN pip install -r requirements.txt gunicorn

COPY setup.py .
COPY cfgserver/ ./cfgserver

RUN pip install .

FROM debian:trixie-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends --no-install-suggests \
            ca-certificates \
 && update-ca-certificates \
 && apt-get install -y python3

COPY --from=py-build /opt/venv /opt/venv
ENV PATH "/opt/venv/bin:$PATH"

CMD ["gunicorn", "-w", "1", "cfgserver.app:app", "-b", "0.0.0.0:8080"]
