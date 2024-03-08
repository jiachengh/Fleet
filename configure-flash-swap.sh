adb root
adb shell swapoff /dev/block/zram0

FILE="/data/swapfile"

adb shell "
if ! test -f "$FILE"; then
    echo "$FILE does not exist. Starting creating swapfile."
    dd if=/dev/zero of=/data/swapfile bs=1025 count=2097152
    chmod 777 /data/swapfile
    mkswap /data/swapfile
fi
swapon /data/swapfile
"
