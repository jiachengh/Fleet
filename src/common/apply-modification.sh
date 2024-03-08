#!/bin/bash

# Please update this path based on the local environment
# It should be the path to the downloaded AOSP
AOSP_PATH=/media/jiacheng/DATA3/Pixel3/android-10/android-10

cp ./device.mk $AOSP_PATH/device/google/crosshatch/device.mk
cp ./privapp-permissions-platform.xml $AOSP_PATH/frameworks/base/data/etc/privapp-permissions-platform.xml 
cp ./lmkd.c $AOSP_PATH/system/core/lmkd/lmkd.c
