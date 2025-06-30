# Multi-stage build for SGNL Linux Host
# Stage 1: Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    python3-dev \
    python3-pip \
    libcurl4-openssl-dev \
    libjson-c-dev \
    libpam0g-dev \
    pkg-config \
    make \
    gcc \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
COPY src/ /build/src/
COPY output/ /build/output/
WORKDIR /build

# Build C modules from source in Linux environment
RUN cd src/c && \
    echo "🔨 Building SGNL modules for Linux with $(nproc) parallel jobs..." && \
    make clean && \
    make -j$(nproc) all && \
    echo "✅ C modules built successfully" && \
    ls -la modules/sudo/ && \
    ls -la modules/pam/

# Install Python dependencies and generate protobuf
RUN cd src/python/host-adapter && \
    pip3 install -r requirements.txt && \
    pip3 install grpcio-tools && \
    cd proto && \
    python3 -m grpc_tools.protoc \
        --python_out=.. \
        --grpc_python_out=.. \
        --proto_path=. \
        adapter.proto && \
    echo "✅ Protobuf files generated"

# Stage 2: Runtime stage
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    libcurl4 \
    libjson-c5 \
    libpam-modules \
    libpam-runtime \
    sudo \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy built artifacts from builder stage
COPY --from=builder /build/src/c/modules/sudo/sgnl_policy.so /usr/lib/sudo/sgnl_policy_plugin.so
COPY --from=builder /build/src/c/modules/pam/pam_sgnl.so /lib/security/pam_sgnl.so
COPY --from=builder /build/src/python/host-adapter/ /app/host-adapter/
COPY --from=builder /build/output/ /app/output/

# Set proper permissions for modules
RUN chmod 644 /usr/lib/sudo/sgnl_policy_plugin.so && \
    chmod 644 /lib/security/pam_sgnl.so && \
    echo "✅ SGNL modules installed:" && \
    ls -la /usr/lib/sudo/sgnl_policy_plugin.so && \
    ls -la /lib/security/pam_sgnl.so

# Install Python runtime dependencies
RUN cd /app/host-adapter && pip3 install -r requirements.txt

# Create necessary directories and copy configuration
RUN mkdir -p /etc/sgnl /var/log
COPY output/for-linux-host/etc-sgnl-config.json /etc/sgnl/config.json
COPY output/for-linux-host/etc-sudo.conf /etc/sudo.conf
COPY .adapter_tokens /app/host-adapter/.adapter_tokens
RUN chmod 644 /etc/sgnl/config.json /etc/sudo.conf && \
    echo "✅ SGNL configuration installed"

# Create default test user (can be overridden at runtime)
ARG TEST_USERNAME=testuser
ENV TEST_USERNAME=${TEST_USERNAME}

# Set up environment
ENV PYTHONPATH=/app/host-adapter

WORKDIR /app

# Copy entrypoint script
COPY docker/entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh

# Accept port as build argument (pass with --build-arg GRPC_PORT=9999)
ARG GRPC_PORT=8082
EXPOSE ${GRPC_PORT}

ENTRYPOINT ["/app/entrypoint.sh"]
CMD ["interactive"] 