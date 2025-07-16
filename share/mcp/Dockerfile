FROM node:22.12-alpine AS builder

# Must be entire project because `prepare` script is run during `npm install` and requires all files.
COPY . /app

WORKDIR /app

RUN --mount=type=cache,target=/root/.npm npm install

FROM ubuntu:22.04 AS runtime

# Install gpac
RUN apt-get update && \
    apt-get install -y curl && \
    curl -L https://download.tsi.telecom-paristech.fr/gpac/new_builds/gpac_latest_head_linux64.deb -o /tmp/gpac_latest_head_linux64.deb && \
    apt-get install -y /tmp/gpac_latest_head_linux64.deb && \
    apt-get autoremove -y && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/gpac_latest_head_linux64.deb

# Install nodejs
RUN curl -fsSL https://deb.nodesource.com/setup_22.x -o nodesource_setup.sh && \
    bash nodesource_setup.sh && \
    apt-get install -y nodejs && \
    rm nodesource_setup.sh

COPY --from=builder /app/dist /app/dist
COPY --from=builder /app/package.json /app/package.json
COPY --from=builder /app/package-lock.json /app/package-lock.json

ENV NODE_ENV=production

WORKDIR /app

RUN npm ci --ignore-scripts --omit-dev

# Initialize gpac
RUN gpac && MP4Box

ENTRYPOINT ["node", "dist/index.js"]