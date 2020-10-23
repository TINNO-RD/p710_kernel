Kernel Build  

 0.0) git clone https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9
 0.1) cd aarch64-linux-android-4.9/
 0.2) git checkout android10-mainline-a-release
 1) cross=$PWD/aarch64-linux-android-4.9/bin/aarch64-linux-android-
 2）cd ../msm-4.14
 3）mkdir out
 4）kernel_out_dir=$PWD/out
 5）make ARCH=arm64 CROSS_COMPILE=$cross O=$kernel_out_dir p710-perf_defconfig
 6）make ARCH=arm64 CROSS_COMPILE=$cross O=$kernel_out_dir KCFLAGS=-mno-android headers_install
 7）TARGET_PRODUCT=p710 make ARCH=arm64 CROSS_COMPILE=$cross O=$kernel_out_dir KCFLAGS=-mno-android -j1

* "-j1" : The number, 16, is the number of multiple jobs to be invoked simultaneously. 
- After build, you can find the build image(zImage) at out/arch/arm/
