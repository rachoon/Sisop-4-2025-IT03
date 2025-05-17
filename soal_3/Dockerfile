FROM ubuntu:22.04

# Install dependencies termasuk libfuse-dev
RUN apt-get update && \
    apt-get install -y \
    fuse \
    libfuse-dev \
    gcc \
    make \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY antink.c .

# Gunakan -lfuse bukan -lfuse3
RUN gcc antink.c -o antink -D_FILE_OFFSET_BITS=64 -lfuse

HEALTHCHECK --interval=5s --timeout=3s --retries=3 \
  CMD ls /antink_mount && [ -f /var/log/it24.log ]

CMD ["/app/antink", "-f", "/antink_mount"]
