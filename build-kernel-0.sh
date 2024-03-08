#export SKIP_DEFCONFIG=1

#cd /media/jiacheng/DATA1/Pixel3/android-kernel-0/private/msm-google
#make mrproper

cd ../android-kernel-0 
./build/build.sh 2>&1 | tee ~/build-kernel.log
