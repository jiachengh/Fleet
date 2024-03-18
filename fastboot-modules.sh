#!/bin/bash
# adb=/home/jiacheng/tools/platform-tools/adb
adb root
adb remount -R

adb root > /dev/null 2>&1
while [ $? -ne 0 ]; do
    sleep 1
    adb root > /dev/null 2>&1
done
adb remount -R
# adb push ./android-kernel/out/android-msm-pixel-4.9/dist/*.ko /vendor/lib/modules
adb push /media/jiacheng/DATA3/Pixel3/Fleet/Fleet-AE/Images/Fleet/dist/*.ko /vendor/lib/modules # pre-built
adb disable-verity
adb reboot

