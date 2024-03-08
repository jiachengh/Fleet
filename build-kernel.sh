#!/bin/bash

# Please modify the ROOT_PATH to the directory containing android-kernel
KERNEL_PATH=/media/jiacheng/DATA3/Pixel3/android-10/android-kernel

cd $KERNEL_PATH/private/msm-google
make mrproper

cd $KERNEL_PATH
./build/build.sh 2>&1 | tee ~/build-kernel.log
