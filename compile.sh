#!/bin/bash
clear
mkdir ./build
cd ./build

#export CC=gcc-8 CXX=g++-8


cmake ../
make -j4
cd ..