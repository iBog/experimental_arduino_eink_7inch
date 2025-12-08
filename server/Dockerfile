# Universal Dockerfile for all architectures (amd64, arm64, etc.)
FROM node:22-bookworm-slim

# Install Chromium (multi-arch) and fonts to support major charsets
RUN apt-get update \
    && apt-get install -y wget gnupg curl \
    && apt-get install -y chromium fonts-ipafont-gothic fonts-wqy-zenhei fonts-thai-tlwg fonts-kacst fonts-freefont-ttf libxss1 \
      python3 make g++ \
      --no-install-recommends \
    && rm -rf /var/lib/apt/lists/*

# Set environment variables
ENV PUPPETEER_SKIP_CHROMIUM_DOWNLOAD=true
ENV PUPPETEER_EXECUTABLE_PATH=/usr/bin/chromium

# Create a non-root user 'appuser'
RUN groupadd -r appuser && useradd -r -g appuser -G audio,video appuser \
    && mkdir -p /home/appuser \
    && chown -R appuser:appuser /home/appuser

# Set the working directory
WORKDIR /app
RUN chown -R appuser:appuser /app

# Copy package files first with correct ownership
COPY --chown=appuser:appuser package*.json ./

# Install dependencies
RUN npm install

# Copy the rest of the application code with correct ownership
COPY --chown=appuser:appuser . .

# Create data directory for persistent config
RUN mkdir -p data && chown -R appuser:appuser data && chmod 755 data

EXPOSE 3123

CMD ["node", "server.js"]

