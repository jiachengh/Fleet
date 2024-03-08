# Execute with sudo

/home/jiacheng/tools/platform-tools/adb reboot bootloader

export ANDROID_PRODUCT_OUT=/media/jiacheng/DATA1/Pixel3/android-10-0/out/target/product/blueline

cd  $ANDROID_PRODUCT_OUT

/home/jiacheng/tools/platform-tools/fastboot flashall -w

