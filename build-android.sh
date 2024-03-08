#!/bin/bash

cd /media/jiacheng/DATA3/Pixel3/android-10/android-10

# Please change this path based on the location of kernel
export TARGET_PREBUILT_KERNEL=/media/jiacheng/DATA3/Pixel3/android-10/android-kernel/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb

#make generate-asm-support
#make core-platform-api-stubs-update-current-api
make -j`nproc` 2>&1 | tee ~/build-android.log
make -j`nproc` 2>&1 | tee -a ~/build-android.log
make -j`nproc` 2>&1 | tee -a ~/build-android.log
