#!/bin/bash
./configure --enable-qx1050 --disable-fbcon --enable-dummyaudio --enable-video-dummy --build=arm-linux
make -j4
