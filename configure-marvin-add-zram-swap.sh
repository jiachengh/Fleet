adb shell swapoff /dev/block/zram0
adb shell "mkfs.ext4 /dev/block/zram0"
adb shell mkdir /sdcard/zram
adb shell mount /dev/block/zram0 /sdcard/zram/
adb shell chmod -R 777 /sdcard/zram/

