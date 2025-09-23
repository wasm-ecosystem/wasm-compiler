FROM ubuntu:latest

ENV http_proxy=$proxy
ENV https_proxy=$proxy
ENV HTTP_PROXY=$proxy
ENV HTTPS_PROXY=$proxy

RUN export PATH="/host/usr/local/bin/:$PATH"

RUN apt-get update && apt-get install -y libglib2.0-dev pkg-config ninja-build libpixman-1-dev p7zip-full texinfo bison flex python2.7 libpython2.7 libfdt-dev
