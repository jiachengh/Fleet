adb root
adb remount -R
adb root
adb remount -R
adb push "./APK/google/com.google.android.gsf_10-5771379-29_minAPI29(nodpi)_apkmirror.com.apk" /system/priv-app
adb push "./APK/google/com.google.android.gms_21.39.18_(120400-407637301)-213918037_minAPI29(arm64-v8a,armeabi-v7a)(nodpi)_apkmirror.com.apk" /system/priv-app
adb push "./APK/google/com.android.vending_27.9.15-21_0_PR_407877111-82791510_minAPI21(arm64-v8a,armeabi-v7a,x86,x86_64)(nodpi)_apkmirror.com.apk" /system/priv-app
adb reboot

