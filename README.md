
# Instructions for building and evaluating Fleet

Fleet is a co-design of ART and Kernel, enabling more efficient memory management for Android.

**Directory description:**

* `evaluation`
  * `exp-cache-app`: Scripts and Jupyter notebooks for testing the caching capacity.
  * `exp-hot-launch`: Scripts and Jupyter notebooks for testing the hot launch performance.
  * `exp-runtime-performance`: Scripts and Jupyter notebooks for testing the FPS, janks, and CPU utilization.
  * `APK`: Includes APKs for tested apps and Google Services Framework.
  * `install-apk`: Scripts for the installation of Google Services Framework and tested apps.
  * `tool-scripts`: Other scripts called during the evaluation.
* `src`
  * `common`: Modified code for both Fleet and original Android. It is used to bypass some security checks and disable the Low Memory Killer daemon.
  * `Fleet-AOSP`: Modified code of Fleet in the AOSP side.
  * `Fleet-Kernel`: Modified code of Fleet in the Kernel side.
  * `Marvin-AOSP`: An implementation of Marvin in Android 10.
* `android-10, android-10-0`, `android-10-marvin`: Stores the complete source code and compiled files of AOSP for Fleet, original Android, and Marvin.
* `android-kernel, android-kernel-0`: Stores the complete source code and compiled files of the Kernel part for Fleet, original Android, and Marvin.
* `build-android.sh, build-android-0.sh`, `build-android-marvin.sh`: Scripts to build a target AOSP with a given kernel for Fleet, original Android, and Marvin.
* `build-kernel.sh, build-kernel-0.sh`: Scripts to build a target kernel for Fleet, original Android, and Marvin.
* `configure-flash-swap.sh`: Script to configure the swap partition.
* `fastboot-init.sh`: Script to switch the Pixel device into fastboot mode.
* `fastboot-modules.sh`: Installs kernel modules into the device.
* `fastboot.sh, fastboot-0.sh`, `fastboot-marvin.sh`: Installs a compiled Android image into the device for Fleet, original Android, and Marvin.。

This document primarily describes how to install Fleet and the other baselines. 
The main installation workflow follows the [official Android build document](https://source.android.com/docs/setup/build/building), with the exception that we incorporate our modified code in certain steps (such as Step 3). 
During the installation procedure, the paths of some files in the script need to be configured as corresponding absolute paths, so the user might need to check that the paths in the script are correct before running it.

Note: We also provide the pre-built image for fast evaluation. Users can directly proceed to step 8 after completing step 0.

## 0. Install dependencies
To install the required packages, run the following command in the terminal:
```bash
sudo apt-get install git-core gnupg flex bison build-essential zip curl zlib1g-dev libc6-dev-i386 libncurses5 x11proto-core-dev libx11-dev lib32z1-dev libgl1-mesa-dev libxml2-utils xsltproc unzip fontconfig
```
To install Perfetto:
```bash
pip intall perfetto
```

Next, install *repo* by executing the following commands:
```
sudo apt-get update
sudo apt-get install repo
```

After that, download *platform-tools* from https://developer.android.com/tools/releases/platform-tools. Then, update all the paths in the `fastboot.sh`, `fastboot-0.sh`, `fastboot-marvin.sh` files to the tools in the downloaded tools directory (such as adb and fastboot).


## 1. Download Kernel 
To download the kernel for Fleet, perform the following steps:
```bash
mkdir android-kernel
cd android-kernel 
repo init -u https://android.googlesource.com/kernel/manifest -b android-msm-crosshatch-4.9-android10 
repo sync  
cd build/ 
git reset --hard 20e4a3abc406aaf9a25903f8c4b7e4c467fbc09e # To use the traditional compilation method
```


To download the kernel for the original Android and Marvin, follow these instructions:
```bash
mkdir android-kernel-0
cd android-kernel-0
repo init -u https://android.googlesource.com/kernel/manifest -b android-msm-crosshatch-4.9-android10 
repo sync  
cd build/ 
git reset --hard 20e4a3abc406aaf9a25903f8c4b7e4c467fbc09e # To use the traditional compilation method
```
 

## 2. Download AOSP 
To download the AOSP for Fleet, follow these steps:
```bash
# Navigate to the root path
mkdir android-10
cd android-10 

# Initialize the repository and sync the source code
repo init -b main -u https://android.googlesource.com/platform/manifest -b  android-10.0.0_r1 
repo sync # Note: Downloading the entire source code of AOSP may take a significant amount of time
repo sync # Run this command again to ensure all files are complete

# Download the drivers for Android 10 and Pixel 3
wget https://dl.google.com/dl/android/aosp/google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
wget https://dl.google.com/dl/android/aosp/qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Extract the driver files 
tar -xvzf ./google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
tar -xvzf ./qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Proceed with the installation by selecting 'I ACCEPT' in the following two commands
./extract-google_devices-blueline.sh 
./extract-qcom-blueline.sh 
```

To download the AOSP for the original Android, follow these steps:
```bash
# Navigate to the root path
mkdir android-10-0
cd android-10-0 

# Initialize the repository and sync the source code
repo init -b main -u https://android.googlesource.com/platform/manifest -b  android-10.0.0_r1 
repo sync # Note: Downloading the entire source code of AOSP may take a significant amount of time
repo sync # Run this command again to ensure all files are complete

# Download the drivers for Android 10 and Pixel 3
wget https://dl.google.com/dl/android/aosp/google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
wget https://dl.google.com/dl/android/aosp/qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Extract the driver files 
tar -xvzf ./google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
tar -xvzf ./qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Proceed with the installation by selecting 'I ACCEPT' in the following two commands
./extract-google_devices-blueline.sh 
./extract-qcom-blueline.sh 
```


To download the AOSP for Marvin, follow these steps:
```bash
# Navigate to the root path
mkdir android-10-marvin
cd android-10-marvin

# Initialize the repository and sync the source code
repo init -b main -u https://android.googlesource.com/platform/manifest -b android-10.0.0_r1 
repo sync # Note: Downloading the entire source code of AOSP may take a significant amount of time 
repo sync # Run this command again to ensure all files are complete

# Download the drivers for Android 10 and Pixel 3
wget https://dl.google.com/dl/android/aosp/google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
wget https://dl.google.com/dl/android/aosp/qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Extract the driver files
tar -xvzf ./google_devices-blueline-qp1a.190711.019-0a670fa6.tgz 
tar -xvzf ./qcom-blueline-qp1a.190711.019-f1c7eeea.tgz 

# Proceed with the installation by selecting 'I ACCEPT' in the following two commands
./extract-google_devices-blueline.sh 
./extract-qcom-blueline.sh 
```


## 3. Apply code modification
The common modification is applied to all Android systems in the experiment. It bypasses certain security checks and helps streamline our experiments. 
Additionally, this modification disables the low memory killer daemon for our test Android systems
```bash
# From the src folder
./common/apply-modification.sh 
```

Apply modification for Fleet:
```bash
# From the src folder
./Fleet-AOSP/apply-modification.sh 
./Fleet-Kernel/apply-modification.sh 
```

Apply modification for Marvin:
```bash
# From the src folder
./Marvin-AOSP/apply-modification.sh 
```

## 4. Compile an AOSP without the downloaded kernel 
To build AOSP for Fleet, follow these steps:
```bash
# Open a new terminal
# From the root path 
cd ./android-10
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
make -j`nproc` 
```

To build AOSP for the original Android, use the following commands:
```bash
# Open a new terminal
# From the root path 
cd ./android-10-0 
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
make -j`nproc` 
```

To build AOSP for Marvin, execute the following commands:
```bash
# Open a new terminal
# From the root path 
cd ./android-10-marvin
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
make -j`nproc` 
```



## 5. Compile the kernel 
This step needs to be performed after an initial compilation of AOSP because we require the generated tool (android-10/out/soong/host/linux-x86/bin/soong_zip).


To build the kernel for Fleet, follow these steps:
```bash
# Open a new terminal 
# From the root path 
cd android-10 
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-kernel.sh
```

To build the kernel for original Android and Marvin, follow these steps:
```bash
# Open a new terminal 
# From the root path 
cd android-10-0
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-kernel-0.sh
```
 
 

## 6. Compile AOSP with the downloaded kernel
To build AOSP for Fleet, follow these steps:
```bash
# Open a new terminal 
# From the root path
cd ./android-10

# Edit the ../build-android.sh file using vim and modify the TARGET_PREBUILT_KERNEL variable to the absolute path of "android-kernel/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb".
vim ../build-android.sh
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-android.sh
```

To build AOSP for the original Android, follow these steps:
```bash
# Open a new terminal 
# From the root path
cd ./android-10-0

# Edit the ../build-android-0.sh file using vim and modify the TARGET_PREBUILT_KERNEL variable to the absolute path of "android-kernel-0/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb".
vim ../build-android-0.sh 
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-android.sh
```

To build AOSP for Marvin, follow these steps:
```bash
# Open a new terminal 
# From the root path
cd ./android-10-marvin

# Edit the ../build-android-marvin.sh file using vim and modify the TARGET_PREBUILT_KERNEL variable to the absolute path of "android-kernel-0/out/android-msm-pixel-4.9/private/msm-google/arch/arm64/boot/Image.lz4-dtb".
vim ../build-android-marvin.sh 
source build/envsetup.sh 
lunch aosp_blueline-userdebug 
../build-android-marvin.sh
```
 

## 7. Flashing the image to the devices

Please update the paths in the  `fastboot.sh`, `fastboot-0.sh`, `fastboot-marvin.sh` scripts to match your local environment. This includes updating the paths for "adb", "fastboot", and "ANDROID_PRODUCT_OUT" before executing the aforementioned scripts.


Flash Fleet to the device:
```bash
# From the root path
./fastboot-init.sh 
sudo ./fastboot.sh 
./fastboot-modules.sh 
```

Flash the original Android to the device:
```bash
# From the root path
./fastboot-init.sh 
sudo ./fastboot-0.sh 
./fastboot-modules-0.sh 
```

Flash Marvin to the device:
```bash
# From the root path
./fastboot-init.sh 
sudo ./fastboot-marvin.sh 
./fastboot-modules-0.sh 
```
 

## 8. Install pre-built images
Users can download the pre-built images from the [onedrive link](https://portland-my.sharepoint.com/:f:/g/personal/jiachuang6-c_my_cityu_edu_hk/EgoDgVPRKxJPuvTVg7LsfKsBSZPqPIuMUzP6-CB2MSqG4g?e=zviP6A).

There are 5 compressed folders in the shared folder `Fleet-AE`:
- `Fleet-AE/Images/Fleet/dist.zip`: Linux kernel images of Fleet.
- `Fleet-AE/Images/Fleet/blueline.zip`: AOSP images of Fleet.
- `Fleet-AE/Images/Original-Android/dist.zip`: Linux kernel images of the original Android.
- `Fleet-AE/Images/Original-Android/blueline.zip`: AOSP images of the original Android.
- `Fleet-AE/Images/Marvin/blueline.zip`: AOSP images of Marvin.

Users can download the shared folder and decompress these 5 zip files.



**Modify image paths of `fastboot.sh` and `fastboot-module.sh` for Fleet:**

To modify the `fastboot.sh` script, set the `ANDROID_PRODUCT_OUT` variable to the absolute path of the folder containing the pre-built AOSP images of Fleet. For example, users can modify the line in `fastboot.sh` as follows:
```
export ANDROID_PRODUCT_OUT=<The absolute path>/Fleet-AE/Images/Fleet/blueline
# the absolute path of the pre-built AOSP image of Fleet
```

To modify the `fastboot-module.sh` script, set the target of the `adb push` command (line 12) to the path of the folder containing the pre-built kernel images of Fleet, like this:
```
adb push ./Fleet-AE/Images/Fleet/dist/*.ko /vendor/lib/modules
```


**Modify image paths of `fastboot-0.sh` and `fastboot-module-0.sh` for original Android:**
```
export ANDROID_PRODUCT_OUT=<The absolute path>/Fleet-AE/Images/Original-Android/blueline
# the absolute path of the pre-built AOSP image of the original Android
```
```
adb push ./Fleet-AE/Images/Original-Android/dist/*.ko /vendor/lib/modules
```

**Modify image paths of `fastboot-marvin.sh` for Marvin:**
```
export ANDROID_PRODUCT_OUT=<The absolute path>/Fleet-AE/Images/Marvin/blueline
# the absolute path of the pre-built AOSP image of Marvin
```

Finally, users can follow the steps mentioned in step 7 to install Fleet, original Android, and Marvin on the mobile device.


## 9. Summary

We have built Fleet in `android-10` and `android-kernel`, the original Android in `android-10-0` and `android-kernel-0`, and Marvin in `android-10-marvin` and `android-kernel-0`.


After completing the build procedure, we can use only step 7 to modify the running Android system during our experiments.

Next, we can proceed with the experiments. 
The files related to the experiments are contained in the `evaluation` folder, which includes three experiments: (1) Caching capacity and GC working set; (2) Hot launch performance; and (3) Runtime performance.
Each experiment has scripts and associated Jupyter Notebooks to conduct the experiments, analyze the data, and generate figures of the results. 
To run the experiments, please refer to the detailed instructions in [EXPERIMENTS.md](evaluation/EXPERIMENTS.md).

 

 