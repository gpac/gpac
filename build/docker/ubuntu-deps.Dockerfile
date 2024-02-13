FROM ubuntu:latest


# set up the machine
RUN apt-get -yqq update && apt-get install -y --no-install-recommends build-essential pkg-config g++ git cmake yasm fakeroot dpkg-dev devscripts debhelper ccache
RUN apt-get install -y --no-install-recommends zlib1g-dev libfreetype6-dev libjpeg62-dev libpng-dev libmad0-dev libfaad-dev libogg-dev libvorbis-dev libtheora-dev liba52-0.7.4-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev libnghttp2-dev libopenjp2-7-dev libcaca-dev libxv-dev x11proto-video-dev libgl1-mesa-dev libglu1-mesa-dev x11proto-gl-dev libxvidcore-dev libssl-dev libjack-jackd2-dev libasound2-dev libpulse-dev libsdl2-dev dvb-apps mesa-utils
RUN apt-get autoremove -y && apt-get clean -y


WORKDIR /gpac-deps

# get manually built dependencies
RUN git clone --depth=1 https://github.com/gpac/deps_unix
WORKDIR /gpac-deps/deps_unix
RUN git submodule update --init --recursive --force --checkout
RUN ./build_all.sh linux

CMD ["bash"]
