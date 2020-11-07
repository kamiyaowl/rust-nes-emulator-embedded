FROM rust:1.47.0-buster
ENV DEBIAN_FRONTEND noninteractive

RUN rustup install nightly

RUN apt update \
  && apt install -y --no-install-recommends \
  git \
  libgtk-3-dev \
  gcc \
  g++ \
  build-essential \
  libasound2-dev \
  mesa-common-dev \
  libx11-dev \
  libxrandr-dev \
  libxi-dev \
  xorg-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  gcc-arm-none-eabi \
  && apt -y clean

RUN git clone https://github.com/raysan5/raylib.git raylib \
  && cd raylib/src/ \
  && make PLATFORM=PLATFORM_DESKTOP \
  && make install

RUN mkdir /work
WORKDIR /work

CMD ["/bin/sh"]