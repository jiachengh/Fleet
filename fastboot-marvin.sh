# Execute with sudo
/home/jiacheng/tools/platform-tools/adb reboot bootloader

# export ANDROID_PRODUCT_OUT=/media/jiacheng/DATA1/Pixel3/android-10-marvin/out/target/product/blueline
export ANDROID_PRODUCT_OUT=/media/jiacheng/DATA3/Pixel3/Fleet/Fleet-AE/Images/Marvin/blueline # pre-built

cd  $ANDROID_PRODUCT_OUT

/home/jiacheng/tools/platform-tools/fastboot flashall -w
# /home/jiacheng/tools/platform-tools/fastboot flashall

