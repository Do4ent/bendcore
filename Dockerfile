# BendCore Multi-stage Docker build
# Build: docker build -t bendcore:latest .
# Run: docker run -p 8080:8080 -p 3001:3001 bendcore:latest

FROM ubuntu:24.04 as builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    ca-certificates \
    libeigen3-dev \
    libtbb-dev \
    libocct-data-exchange-dev \
    libocct-modeling-algorithms-dev \
    nodejs \
    npm \
    && rm -rf /var/lib/apt/lists/*

# Copy source
WORKDIR /src
COPY . .

# Build C++ with OCCT
RUN cmake --preset host -DBENDCORE_WITH_OCCT=ON && \
    cmake --build --preset host --parallel 4

# Build Node.js backend
WORKDIR /src/backend
RUN npm ci && npm run build

# Build React UI (static)
WORKDIR /src/ui
RUN npm ci && npm run build

# Runtime stage
FROM ubuntu:24.04

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libeigen3-dev \
    libtbb-dev \
    libocct-data-exchange-7.6 \
    libocct-modeling-algorithms-7.6 \
    nodejs \
    && rm -rf /var/lib/apt/lists/*

# Copy built artifacts
COPY --from=builder /src/build/bendcore_server /app/
COPY --from=builder /src/backend/dist /app/backend/dist
COPY --from=builder /src/backend/node_modules /app/backend/node_modules
COPY --from=builder /src/backend/package.json /app/backend/
COPY --from=builder /src/ui/build /app/ui/build

# Copy support files
COPY --from=builder /src/docs /app/docs
COPY --from=builder /src/USER_GUIDE.md /app/

WORKDIR /app

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/v1/health || exit 1

# Expose ports
EXPOSE 8080 3001

# Start script
RUN echo '#!/bin/bash' > /app/start.sh && \
    echo './bendcore_server 8080 &' >> /app/start.sh && \
    echo 'cd backend && node dist/index.js &' >> /app/start.sh && \
    echo 'wait' >> /app/start.sh && \
    chmod +x /app/start.sh

CMD ["/app/start.sh"]
