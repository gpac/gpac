# Common Dockerfile for building GPAC WASM
FROM debian:bullseye-slim as base

ARG EMSDK_VERSION=3.1.56
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get -yqq update && apt-get install -y --no-install-recommends \
  build-essential \
  ca-certificates \
  pkg-config \
  python3 \
  g++ \
  git \
  cmake \
  wget autotools-dev automake libtool && \
  apt-get autoremove -y && apt-get clean -y && rm -rf /var/lib/apt/lists/*

# Install latest version of autoconf
RUN wget https://ftp.gnu.org/gnu/autoconf/autoconf-2.71.tar.xz && \
  tar -xf autoconf-2.71.tar.xz && cd autoconf-2.71/ && \
  ./configure && make && make install && \
  cd .. && rm -rf autoconf-2.71 autoconf-2.71.tar.xz

# Get the emsdk repo
RUN git clone --depth 1 https://github.com/emscripten-core/emsdk.git

RUN cd emsdk && ./emsdk install $EMSDK_VERSION && ./emsdk activate $EMSDK_VERSION

# Update environment variables
ENV PATH="/emsdk:/emsdk/upstream/emscripten:${PATH}"
ENV EMSDK="/emsdk"
ENV EMSDK_NODE="/emsdk/node/16.20.0_64bit/bin/node"

# Build GPAC WASM dependencies
FROM base as deps

# Install GPAC WASM dependencies
RUN git clone --recurse-submodules --shallow-submodules --depth 1 https://github.com/gpac/deps_wasm

# Build GPAC WASM dependencies
WORKDIR /deps_wasm
RUN sed -i "s/^compile_x265$//g" wasm_extra_libs.sh
RUN ./wasm_extra_libs.sh --enable-threading

# Build GPAC
FROM base as gpac

# Copy GPAC source code
COPY . /gpac_public
WORKDIR /gpac_public

# Copy GPAC WASM dependencies
COPY --from=deps /deps_wasm/wasm_thread /deps_wasm/wasm_thread
ENV PKG_CONFIG_PATH=/deps_wasm/wasm_thread/lib/pkgconfig

# Configure GPAC
RUN make distclean; ./configure --emscripten --extra-cflags="-Wno-pointer-sign -Wno-implicit-const-int-float-conversion"

# Build GPAC
RUN sed -i "s/-sMODULARIZE=1/-O2 -sMODULARIZE=1/g" applications/gpac/Makefile
RUN make -j$(nproc)
RUN cp -a --remove-destination /gpac_public/share/emscripten/gpac.html  /gpac_public/bin/gcc/
