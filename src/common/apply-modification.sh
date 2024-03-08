#!/bin/bash

# Please update this path based on the local environment
# It should be the path to the downloaded AOSP

cp ./common/device.mk ../android-10/device/google/crosshatch/device.mk
cp ./common/privapp-permissions-platform.xml ../android-10/frameworks/base/data/etc/privapp-permissions-platform.xml 
cp ./common/lmkd.c ../android-10/system/core/lmkd/lmkd.c


cp ./common/device.mk ../android-10-0/device/google/crosshatch/device.mk
cp ./common/privapp-permissions-platform.xml ../android-10-0/frameworks/base/data/etc/privapp-permissions-platform.xml 
cp ./common/lmkd.c ../android-10-0/system/core/lmkd/lmkd.c

cp ./common/device.mk ../android-10-marvin/device/google/crosshatch/device.mk
cp ./common/privapp-permissions-platform.xml ../android-10-marvin/frameworks/base/data/etc/privapp-permissions-platform.xml 
cp ./common/lmkd.c ../android-10-marvin/system/core/lmkd/lmkd.c