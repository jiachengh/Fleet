
# Instructions for building and evaluating Fleet

Fleet is a co-design of ART and Kernel, enabling more efficient memory management for Android.

**Directory description:**
* `android-10, android-10-0`, `android-10-marvin`: To store the complete source code and complied files of AOSP for Fleet, original Android, and Marvin.
* `android-kernel, android-kernel-0`: To store the complete source code and complied files of the Kernel part for Fleet and original Android, and Marvin.
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
* `build-android.sh, build-android-0.sh`, `build-android-marvin.sh`: The script to build a target AOSP with a given kernel for Fleet, original Android, and Marvin.
* `build-kernel.sh, build-kernel-0.sh`: The script to build a target kernel for Fleet, original Android and Marvin.
* `configure-flash-swap.sh`: The script to configure the swap partition.
* `fastboot-init.sh`: The script to make Pixel device get into the fastboot mode.
* `fastboot-modules.sh`: Install kernel modules into the device.
* `fastboot.sh, fastboot-0.sh`, `fastboot-marvin`: Install a compiled Android image into the device for Fleet, original Android, and Marvin.

This document primarily describes how to install Fleet and the other baselines. 
The main installation workflow follows the [official Android build document](https://source.android.com/docs/setup/build/building), with the exception that we incorporate our modified code in certain steps (such as Step 3).
During the installation procedure, the paths of some files in the script need to be configured as corresponding absolute paths, so user might need to check that the paths in the script are correct before running it.

## 0. Install dependencies
Install required packages:
```bash
sudo apt-get install git-core gnupg flex bison build-essential zip curl zlib1g-dev libc6-dev-i386 libncurses5 x11proto-core-dev libx11-dev lib32z1-dev libgl1-mesa-dev libxml2-utils xsltproc unzip fontconfig
```
Install *repo*:
```
sudo apt-get update
sudo apt-get install repo
```

Install *platform-tools* from https://developer.android.com/tools/releases/platform-tools. Then update all path in `fastboot.sh`, `fastboot-0.sh`, `fastboot-marvin.sh`, `fastboot-init.sh`, `fastboot-module.sh`, and `fastboot-module-0.sh` files to the tools in the download tools (such as adb and fastboot).


## 1. Download Kernel 
Download the kernel for Fleet:
```bash
mkdir android-kernel
cd android-kernel 
repo init -u https://android.googlesource.com/kernel/manifest -b android-msm-crosshatch-4.9-android10 
repo sync  
cd build/ 
git reset --hard 20e4a3abc406aaf9a25903f8c4b7e4c467fbc09e # To use the traditional compilation method
```


Download the kernel original Android and Marvin:
```bash
mkdir android-kernel-0
cd android-kernel-0
repo init -u https://android.googlesource.com/kernel/manifest -b android-msm-crosshatch-4.9-android10 
repo sync  
cd build/ 
git reset --hard 20e4a3abc406aaf9a25903f8c4b7e4c467fbc09e # To use the traditional compilation method
```
 

## 2. Download AOSP 
Download the AOSP for Fleet:
```bash
# From root path
mkdir android-10
cd android-10 
repo init -b main -u https://android.googlesource.com/platform/manifest -b  android-10.0.0_r1 
repo sync # Downloading the entire source code of AOSP would require a significant amount of time 
repo sync # To ensure that all files are complete

# Then, we need to download drivers corresponding to Android 10 and Pixel 3
wget https://dl.google.com/dl/android/aosp/google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
wget https://dl.google.com/dl/android/aosp/qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Decompressing these two driver files 
tar -xvzf ./google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
tar -xvzf ./qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 
# Please select 'I ACCEPT' in the following two commands to proceed with the installation
./extract-google_devices-blueline.sh 
./extract-qcom-blueline.sh 
```

Download the AOSP for original Android:
```bash
# From root path
mkdir android-10-0
cd android-10-0 
repo init -b main -u https://android.googlesource.com/platform/manifest -b  android-10.0.0_r1 
repo sync # Downloading the entire source code of AOSP would require a significant amount of time 
repo sync # To ensure that all files are complete

# Then, we need to download drivers corresponding to Android 10 and Pixel 3
wget https://dl.google.com/dl/android/aosp/google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
wget https://dl.google.com/dl/android/aosp/qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Decompressing these two driver files 
tar -xvzf ./google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
tar -xvzf ./qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 
# Please select 'I ACCEPT' in the following two commands to proceed with the installation
./extract-google_devices-blueline.sh 
./extract-qcom-blueline.sh 
```


Download the AOSP for Marvin:
```bash
# From root path
mkdir android-10-marvin
cd android-10-marvin
repo init -b main -u https://android.googlesource.com/platform/manifest -b  android-10.0.0_r1 
repo sync # Downloading the entire source code of AOSP would require a significant amount of time 
repo sync # To ensure that all files are complete

# Then, we need to download drivers corresponding to Android 10 and Pixel 3
wget https://dl.google.com/dl/android/aosp/google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
wget https://dl.google.com/dl/android/aosp/qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Decompressing these two driver files 
tar -xvzf ./google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
tar -xvzf ./qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 
# Please select 'I ACCEPT' in the following two commands to proceed with the installation
./extract-google_devices-blueline.sh 
./extract-qcom-blueline.sh 
```


## 3. Apply code modification
Apply common modification:
We also need to make common changes to bypass certain security checks. This will help us streamline our experiments.
These changes are also necessary for our `fastboot-modules.sh` script.
This modification also disables the low memory killer daemon for our test Android systems.

To do this, we apply the modification in the `src/common` directory to the AOSP source code:
```bash
# From src folder
./common/apply-modification.sh 
```

Apply modification for Fleet:
```bash
# From src folder
./Fleet-AOSP/apply-modification.sh 
./Fleet-Kernel/apply-modification.sh 
```

Apply modification for Marvin:
```bash
# From src folder
./Marvin-AOSP/apply-modification.sh 
```

## 4. Compile a init AOSP without the downloaded kernel 
Build AOSP for Fleet:
```bash
# Open a new terminal
# From root path 
cd ./android-10
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
make -j`nproc` 
```

Build AOSP for original Android:
```bash
# Open a new terminal
# From root path 
cd ./android-10-0 
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
make -j`nproc` 
```

Build AOSP for Marvin:
```bash
# Open a new terminal
# From root path 
cd ./android-10-marvin
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
make -j`nproc` 
```



## 5. Compile the kernel 
It has to be done after an initial compilation of AOSP, because we need to use the generated tool (android-10/out/soong/host/linux-x86/bin/soong_zip).


Build kernel for Fleet:
```bash
# Open a new terminal 
# From root path 
cd android-10 
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-kernel.sh
```

Build kernel for original Android and Marvin:
```bash
# Open a new terminal 
# From root path 
cd android-10-0
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-kernel-0.sh
```
 
 

## 6. Compile AOSP with the downloaded kernel 
Build AOSP for Fleet:
```bash
# Open a new terminal 
# From root path
cd ./android-10
vim ../build-android.sh # modify the variable TARGET_PREBUILT_KERNEL to the absolute path of "android-kernel/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb"
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-android.sh
```

Build AOSP for original Android:
```bash
# Open a new terminal 
# From root path
cd ./android-10-0
vim ../build-android-0.sh # modify the variable TARGET_PREBUILT_KERNEL to the absolute path of "android-kernel-0/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb"
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-android.sh
```

Build AOSP for Marvin:
```bash
# Open a new terminal 
# From root path
cd ./android-10-marvin
vim ../build-android-marvin.sh # modify the variable TARGET_PREBUILT_KERNEL to the absolute path of "android-kernel-0/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb"
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-android-marvin.sh
```
 

## 7. Flash the image to the devices: 

Please replace the tool path in all used scripts based on the local environment, such as "adb" and "fastboot".


Flash Fleet to the device:
```bash
# From root path
./fastboot-init.sh 
sudo ./fastboot.sh 
./fastboot-modules.sh 
```

Flash original Android to the device:
```bash
# From root path
./fastboot-init.sh 
sudo ./fastboot-0.sh 
./fastboot-modules-0.sh 
```

Flash Marvin to the device:
```bash
# From root path
./fastboot-init.sh 
sudo ./fastboot-marvin.sh 
./fastboot-modules-0.sh 
```
 

## 8. Install pre-built images
User could download the pre-built images from the [onedrive link](https://portland-my.sharepoint.com/:f:/g/personal/jiachuang6-c_my_cityu_edu_hk/EgoDgVPRKxJPuvTVg7LsfKsBSZPqPIuMUzP6-CB2MSqG4g?e=zviP6A).

**Modify `fastboot.sh` and `fastboot-module.sh` for Fleet:**

Modify the `fastboot.sh` to set `ANDROID_PRODUCT_OUT` variable to the absolute path of the folder containing the pre-built AOSP images of Fleet.
For example, user might modify the line in the `fastboot.sh` to:
```
export ANDROID_PRODUCT_OUT=/.../Fleet-AE/images/Fleet/aosp-images
# the absolute path of the pre-built AOSP image of Fleet
```

Modify the `fastboot-module.sh` to set the target of `adb push` (line 12) variable to the path of the folder containing the pre-built kernel images of Fleet, such as:
```
$adb push ./Fleet-AE/images/Fleet/kernel-images/*.ko /vendor/lib/modules
```


**Modify `fastboot-0.sh` and `fastboot-module-0.sh` for original Android:**
```
export ANDROID_PRODUCT_OUT=/.../Fleet-AE/images/Original-Android/aosp-images
# the absolute path of the pre-built AOSP image of the original Android
```
```
$adb push ./Fleet-AE/images/Original-Android/kernel-images/*.ko /vendor/lib/modules
```

**Modify `fastboot-marvin.sh` for Marvin:**
```
export ANDROID_PRODUCT_OUT=/.../Fleet-AE/images/Original-Android/aosp-images
# the absolute path of the pre-built AOSP image of Marvin
```

Finally, user can use the above step 7 to install Fleet, original Android, and Marvin to the device.


## 9. Summary

We build Fleet in `android-10` and `android-kernel`, original Android in `android-10-0` and `android-kernel-0` and Marvin in `android-10-marvin` and `android-kernel-0`.


After completing the build procedure, we can use only step 7 to modify the running Android system during our experiments.

Next, we could do the experiments.
Files related to the experiment are contained in the `evaluation` folder. It contains three experiments: (1) Caching capacity and GC working set; (2) Hot launch performance; and (3) Runtime performance. 
Each experiment has some scripts and associated Jupyter Notebooks to conduct experiments, analyze the data and generate figures of the results. 
To run experiments, please refer to the detailed instructions in [EXPERIMENTS.md](evaluation/EXPERIMENTS.md).

 

 