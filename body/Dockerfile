FROM ubuntu:20.04
ENV PYTHONUNBUFFERED 1
ENV PYTHONPATH /tmp/openpilot:$PYTHONPATH

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    automake \
    ca-certificates \
    clang \
    curl \
    g++ \
    gcc-arm-none-eabi libnewlib-arm-none-eabi \
    git \
    libtool \
    libssl-dev \
    libsqlite3-dev \
    libffi-dev \
    locales \
    make \
    patch \
    pkg-config \
    zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && locale-gen
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

RUN curl -L https://github.com/pyenv/pyenv-installer/raw/master/bin/pyenv-installer | bash
ENV PATH="/root/.pyenv/bin:/root/.pyenv/shims:${PATH}"

COPY requirements.txt /tmp/
RUN pyenv install 3.11.4 && \
    pyenv global 3.11.4 && \
    pyenv rehash && \
    pip install --no-cache-dir -r /tmp/requirements.txt

COPY . /tmp/openpilot/body
RUN rm -rf /tmp/openpilot/body/.git
