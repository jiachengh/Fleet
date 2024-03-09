#!/bin/bash


rm -rf ../android-10/art
cp -r ./Fleet-AOSP/art ../android-10


cp ./Fleet-AOSP/Object.java ../android-10/libcore/ojluni/src/main/java/java/lang/Object.java

cp ./Fleet-AOSP/ActivityThread.java ../android-10/frameworks/base/core/java/android/app/ActivityThread.java

