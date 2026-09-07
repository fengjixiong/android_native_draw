2026.5.26
native_render_demo v1.3 增加全部AFBC压缩参数

2026.5.18
native_render_demo v1.1

2025.12.19 fengjx

// make
mm

// copy to emulator
cp ../../out/target/product/emulator_x86_64/system/bin/NativeRenderDemo /home/pub/fp_database/Android/android14/
cp company_demo_core/prebuild_libs/x86_64/libcompany_demo_core.so /home/pub/fp_database/Android/android14/

adb push NativeRenderDemo /data/test/
adb push libcompany_demo_core.so /data/test/

// addr2line debug
../prebuilts/clang/host/linux-x86/clang-r487747c/bin/llvm-addr2line -e ../out/target/product/emulator_x86_64/system/bin/NativeRenderDemo -f -C -p 0x0000000000004cb7

// 显示颜色
LD_LIBRARY_PATH=. ./NativeRenderDemo -c 1 -F 3 # 1-R,2-G,4-B

// 显示图片
LD_LIBRARY_PATH=. ./NativeRenderDemo -i images -w 480 -h 800 -F 3

// 调试DMA-BUF
insmod dma_buf_copy.ko
cat /proc/devices | grep dmabufcopy
mknod /dev/dma_buf_copy c 502 0
LD_LIBRARY_PATH=. ./NativeRenderDemo -i images -w 480 -h 800 -b /dev/dma_buf_copy -B 1
