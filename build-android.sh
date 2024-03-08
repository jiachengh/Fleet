# cd /media/jiacheng/DATA1/Pixel3/android-10

#unset ART_DEFAULT_GC_TYPE
#export ART_USE_READ_BARRIER=false
#export ART_DEFAULT_GC_TYPE=CMS

# export TARGET_PREBUILT_KERNEL=/media/jiacheng/DATA1/Pixel3/android-kernel/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb
export TARGET_PREBUILT_KERNEL=../android-kernel/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb

make -j`nproc` 2>&1 | tee ~/build-android.log
make -j`nproc` 2>&1 | tee -a ~/build-android.log
make -j`nproc` 2>&1 | tee -a ~/build-android.log
