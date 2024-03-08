#!/bin/bash

rm -rf ../android-10-marvin/art
cp -r ./Marvin-AOSP/art ../android-10-marvin


cp ./Marvin-AOSP/Object.java ../android-10-marvin/libcore/ojluni/src/main/java/java/lang/Object.java

cp ./ActivityThread.java ../android-10-marvin/frameworks/base/core/java/android/app/ActivityThread.java

