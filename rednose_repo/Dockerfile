FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y capnproto libcapnp-dev clang wget git autoconf libtool curl make build-essential libssl-dev zlib1g-dev libbz2-dev libreadline-dev libsqlite3-dev llvm libncurses5-dev libncursesw5-dev xz-utils tk-dev libffi-dev liblzma-dev python-openssl libeigen3-dev

RUN curl -L https://github.com/pyenv/pyenv-installer/raw/master/bin/pyenv-installer | bash
ENV PATH="/root/.pyenv/bin:/root/.pyenv/shims:${PATH}"
RUN pyenv install 3.11.4
RUN pyenv global 3.11.4
RUN pyenv rehash

WORKDIR /project

ENV PYTHONPATH=/project

COPY . .
RUN rm -rf .git
RUN pip3 install --no-cache-dir -r requirements.txt
RUN python3 setup.py install
RUN scons -c && scons -j$(nproc)
