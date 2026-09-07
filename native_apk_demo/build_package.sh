# 2026.2.4
# fengjx

cd jni
./build_android.sh
cd ..
rm -f build/apk/nativedemo.unsigned.apk
./aapt.exe package -f -M app/AndroidManifest.xml -I android.jar -S app/res/ -F build/apk/nativedemo.unsigned.apk build/apk
ls -l build/apk
rm -f build/nativerender.apk
apksigner.bat sign --ks my-release-key.jks --out build/nativerender.apk build/apk/nativedemo.unsigned.apk

if [[ -f "build/nativerender.apk" ]]; then
	echo "install..."
	adb.exe install -r build/nativerender.apk
	echo "done"
fi

# 定位问题： 电脑终端中执行，过滤应用相关日志
# adb logcat -s "NativeWindowDemo" "*:E"