# Introduction
This repository hosts the SDL v1.2 source code that ports for both Miyoo A30 and F(x)tec Pro1 devices. For Miyoo A30, it utilizes the OpenGL ES 2.0 for rendering. If the CFW installed on Miyoo A30 doesn't support the OpenGL ES 2.0, then, it means you cannot use this SDL v1.2 on your device. For F(x)tec Pro1, the rendering is based on the Wayland Client and it only tested on Sailfish OS with v4.5.0.19.

&nbsp;

# Building
## Miyoo A30
```
$ cd
$ wget https://github.com/steward-fu/website/releases/download/miyoo-a30/a30_toolchain-v1.0.tar.gz
$ tar xvf a30_toolchain-v1.0.tar.gz
$ sudo mv a30 /opt

$ export PATH=/opt/a30/bin:$PATH
$ export CC=arm-linux-gcc
$ export CXX=arm-linux-g++
$ export LD=arm-linux-ld
$ export AS=arm-linux-as

$ git clone https://github.com/steward-fu/sdl
$ cd sdl
$ ./autogen.sh
$ ./configure --enable-video-a30 --disable-video-x11 --build=arm-linux
$ make -j4
```

&nbsp;

## F(x)tec Pro1
```
$ cd
$ git clone https://github.com/steward-fu/sdl
$ cd sdl
$ ./autogen.sh
$ ./configure --enable-video-qx1000 --disable-video-x11 --build=arm-linux
$ make -j4
```
