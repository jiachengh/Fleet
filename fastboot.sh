#!/bin/bash
# Run this script with sudo
/home/jiacheng/tools/platform-tools/adb reboot bootloader

export ANDROID_PRODUCT_OUT=/media/jiacheng/DATA1/Pixel3/android-10/out/target/product/blueline
cd  $ANDROID_PRODUCT_OUT

/home/jiacheng/tools/platform-tools/fastboot flashall -w
# /home/jiacheng/tools/platform-tools/fastboot flashall

