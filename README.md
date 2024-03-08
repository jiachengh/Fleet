
# Instructions for building and evaluating Fleet

Fleet is a co-design of ART and Kernel, enabling more efficient memory management for Android.

### **Directory description:**
* `android-10, android-10-0`: To store the complete source code and complied files of AOSP for Fleet, original Android.
* `android-kernel, android-kernel-0`: To store the complete source code and complied files of the Kernel part for Fleet and original Android.
* `evaluation`
  * `APK`: Include APKs for tested apps and google services framework.
  * `exp-cache-app`: Stripts and jupyter notebooks for testing the caching capacity.
  * `exp-hot-launch`: Scripts and jupyter notebooks fro testing the hot launch performance.
  * `exp-runtime-performance`: Scripts and jupyter notebooks for testing the FPS, janks, and CPU utilization.
  * `install-apk`: Scripts about installation of google services frameworks and tested apps.
  * `tool-scripts`: Other scripts that are called during the evaluation.
* `src`
  * `common`: Modified code for both Fleet, original Android. It is used to bypass some security check and disable the low memory killer daemon.
  * `Fleet-AOSP`: Modified code of Fleet in the AOSP side.
  * `Fleet-Kernel`: Modified code of Fleet in the Kernel side.
  * `Marvin-AOSP`: An implementation of Marvin in Android 10.
* `build-android.sh, build-android-0.sh`: The script to build a target AOSP with a given kernel for Fleet and original Android.
* `build-kernel.sh, build-kernel-0.sh`: The script to build a target kernel for Fleet and original Android.
* `fastboot-flash-swap.sh`: The script to configure the swap partition.
* `fastboot-init.sh`: The script to make Pixel device get into the fastboot mode.
* `fastboot-modules.sh`: Install kernel modules into the device.
* `fastboot.sh, fastboot-0.sh`: Install a compiled Android image into the device.

This document primarily describes how to install Fleet and the original Android. 
The main installation workflow follows the [official Android build document](https://source.android.com/docs/setup/build/building), with the exception that we incorporate our modified code in certain steps (Step 3, 7).
During the installation procedure, the paths of some files in the script need to be configured as corresponding absolute paths, so user might need to check that the paths in the script are correct before running it.

### 0. Install dependencies
Install required packages:
```bash
sudo apt-get install git-core gnupg flex bison build-essential zip curl zlib1g-dev libc6-dev-i386 libncurses5 x11proto-core-dev libx11-dev lib32z1-dev libgl1-mesa-dev libxml2-utils xsltproc unzip fontconfig
```
Install repo:
```
sudo apt-get update
sudo apt-get install repo
```

### 1. Download Kernel 
Download the kernel from Google's source:
```bash
cd android-kernel # or android-kernel-0 for original Android
repo init -u https://android.googlesource.com/kernel/manifest -b android-msm-crosshatch-4.9-android10 
repo sync  
cd build/ 
git reset --hard 20e4a3abc406aaf9a25903f8c4b7e4c467fbc09e # To use the traditional compilation method
```
 

### 2. Download AOSP 
Download the AOSP from Google's source:
```bash
cd android-10 # or android-10-0 for original Android
repo init -b main -u https://android.googlesource.com/platform/manifest -b  android-10.0.0_r1 
repo sync # Downloading the entire source code of AOSP would require a significant amount of time 
repo sync # To ensure that all files are complete
```

Then, we need to download drivers corresponding to Android 10 and the Pixel 3:
```bash
cd android-10 # or android-10-0 for original Android
wget https://dl.google.com/dl/android/aosp/google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
wget https://dl.google.com/dl/android/aosp/qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Decompressing these two driver files 
tar -xvzf ./google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
tar -xvzf ./qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 
# Please select 'I ACCEPT' in the following two commands to proceed with the installation
./extract-google_devices-blueline.sh 
./extract-qcom-blueline.sh 
```


We also need to make common changes to bypass certain security checks. This will help us streamline our experiments.
These changes are also necessary for our `fastboot-modules.sh` script.

This modification also disables the low memory killer daemon for our test Android systems.

To do this, we apply the modification in the `src/common` directory to the AOSP source code:
```bash
cd ../modify-files-common 
./apply-modification.sh 
```


### 3. Compile AOSP without the downloaded kernel 
```bash
# Open a new terminal 
cd ./android-10 # android-10-0 for original Android
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
make -j`nproc` 
```
 
### 4. Compile the kernel 

It has to be done after an initial compilation of AOSP, because we need to use the generated tool (android-10/out/soong/host/linux-x86/bin/soong_zip).

```bash
# Open a new terminal 
cd android-10 # Or android-10-0 for original Android
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-kernel.sh # Or build-kernel.sh for original Android
```
 

### 5. Compile AOSP with the downloaded kernel 
```bash
# Open a new terminal 
cd ./android-10 # Or android-10-0 for original Android
vim ../build-android.sh # modify the variable TARGET_PREBUILT_KERNEL to the absolute path of ‘android-kernel(-0)/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb’ 
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-android.sh # Or build-android-0.sh for original Android
```
 

### 6. Flash the image to the devices: 

Please replace the tool path in all used scripts based on the local environment, such as adb 
```bash
./fastboot-init.sh 
sudo ./fastboot.sh # Or fastboot-0.sh for original Android
./fastboot-modules.sh # Or fastboot-modules-0.sh for original Android
```
After that we can successfully run the original Android in our Pixel 3.
 

### 7. Apply code modification and install Fleet
```bash
cd src
./Fleet-AOSP/apply-modification.sh 
./Fleet-Kernel/apply-modification.sh 
```
 
After that, execute the above **4, 5, 6 steps** to install Fleet in the device  


<!-- ### 8. Apply code modification and install Marvin (Optional)
```bash
cd src
./Marvin-AOSP/apply-modification.sh 
```
 
After that, execute the above **4, 5, 6 steps** to install Marvin in the device.
 -->

### 8. Summary

We build Fleet in `android-10` and `android-kernel` and original Android in `android-10-0` and `android-kernel-0`.

To install Fleet, we need to follow the steps 1, 2, 3, 7, 4, 5, and 6.

To install the original Android, we need to execute the steps 1, 2, 3, 4, 5, and 6.

After completing the build procedure, we can use only step 6 to modify the running Android system during our experiments.


Files related to the experiment are contained in the `evaluation` folder. It contains three experiments: (1) Caching capacity and GC working set; (2) Hot launch performance; and (3) Runtime performance. 
Each experiment has some scripts and associated Jupyter Notebooks to conduct experiments, analyze the data and generate figures of the results. 
To run experiments, please refer to the detailed instructions in [EXPERIMENTS.md](evaluation/EXPERIMENTS.md).

 

 