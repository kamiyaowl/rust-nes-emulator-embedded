FROM rust:1.47.0-buster

RUN rustup install nightly

RUN apt-get update \
  && apt-get install -y \
  git \
  libgtk-3-dev \
  gcc \
  g++ \
  gcc-arm-none-eabi

RUN mkdir /work
WORKDIR /work

CMD ["/bin/sh"]