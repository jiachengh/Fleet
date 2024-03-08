#!/bin/bash

# Please update this path based on the local environment
# It should be the path to the downloaded AOSP
AOSP_PATH=/media/jiacheng/DATA3/Pixel3/android-10/android-10

rm -rf $AOSP_PATH/art
cp -r ./art $AOSP_PATH

cp -r ./Object.java $AOSP_PATH/libcore/ojluni/src/main/java/java/lang/Object.java

cp ./ActivityThread.java $AOSP_PATH/frameworks/base/core/java/android/app/ActivityThread.java

