#!/bin/bash

# Please update this path based on the local environment
# It should be the path to the downloaded Kernel
KERNEL_PATH=/media/jiacheng/DATA3/Pixel3/android-10/android-kernel

rm -rf $KERNEL_PATH/private/msm-google
cp -r ./msm-google $KERNEL_PATH/private/msm-google
