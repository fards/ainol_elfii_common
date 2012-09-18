#!/bin/bash

set -e

EXACT=1


CPUS=`grep -c processor /proc/cpuinfo`


# build the code
CROSS_COMPILE=/home/ian/toolchain/android-toolchain-eabi/bin/arm-linux-androideabi-
[ -d out ] || mkdir out
make -j${CPUS} O=out ARCH=arm CROSS_COMPILE=$CROSS_COMPILE uImage modules
mkdir out/modules_for_android
make O=out ARCH=arm modules_install INSTALL_MOD_PATH=modules_for_android
