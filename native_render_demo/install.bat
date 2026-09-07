adb remount
adb shell "mkdir -p /data/test"
adb shell "mkdir -p /data/test/dump"
adb shell "mkdir -p /data/test/data"
::adb push images /data/test/
adb push company_demo_core/prebuild_libs/arm64-v8a/libcompany_demo_core.so /system/lib64/
adb push company_framework/prebuild_libs/arm64-v8a/libcompany_framework.so /system/lib64/
adb push prebuilt/arm64-v8a/native_render_demo /data/test/
adb push test_main.sh /data/test/
adb shell "chmod +x /data/test/native_render_demo"
adb shell "chmod +x /data/test/test_main.sh"

