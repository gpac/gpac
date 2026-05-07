FROM node:22.12-alpine AS builder

# Must be entire project because `prepare` script is run during `npm install` and requires all files.
COPY . /app
WORKDIR /app
RUN --mount=type=cache,target=/root/.npm npm install

FROM alpine:3.19 AS gpac-builder

RUN apk add --no-cache --virtual .build-deps \
    build-base git cmake yasm wget zlib-dev zlib-static ccache \
    linux-headers musl-dev

RUN git clone --depth 1 https://github.com/gpac/gpac.git /tmp/gpac-master

WORKDIR /tmp/gpac-master
RUN make distclean && ./configure --static-bin && \
    make -j$(nproc) && cp bin/gcc/MP4Box /usr/local/bin/MP4Box

FROM node:22.12-alpine

# Copy built application
COPY --from=builder /app/dist /app/dist
COPY --from=builder /app/package.json /app/package.json
COPY --from=builder /app/package-lock.json /app/package-lock.json

# Copy gpac binaries from builder
COPY --from=gpac-builder /usr/local/bin/MP4Box /usr/local/bin/MP4Box

ENV NODE_ENV=production
WORKDIR /app

# Install production dependencies and initialize gpac in one layer
RUN npm ci --ignore-scripts --omit=dev && \
    npm cache clean --force && \
    MP4Box -h > /dev/null

ENTRYPOINT ["node", "/app/dist/index.js"]