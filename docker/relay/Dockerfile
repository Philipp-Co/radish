FROM python:3.12-alpine AS builder

RUN apk add --no-cache \
    gcc \
    musl-dev \
    python3-dev \
    linux-headers \
    pkgconfig \
    libffi-dev \
    openssl-dev \
    cargo \
    rust \
    ffmpeg-dev \
    opus-dev \
    libvpx-dev \
    libsrtp-dev

COPY relay/requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir --prefix=/install -r /tmp/requirements.txt

FROM python:3.12-alpine

RUN apk add --no-cache \
    libffi \
    openssl \
    ffmpeg-libs \
    opus \
    libvpx \
    libsrtp

COPY --from=builder /install /usr/local

WORKDIR /app
COPY relay/server.py /app/server.py

EXPOSE 8765/tcp
ENTRYPOINT ["python", "server.py"]
