
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)

# Include headers.
ifeq ($(CONFIG_ARCH_MT6735M),y)
	ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
else \
ifeq ($(CONFIG_ARCH_MT6755),y)
	ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
  ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
	include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
	subdir-ccflags-y += -I$(srctree)/drivers/spi/trustzone/spi/Tlspi/inc
  endif
else \
ifeq ($(CONFIG_ARCH_MT6795),y)
	include $(srctree)/drivers/misc/mediatek/Makefile.custom
else \
ifeq ($(CONFIG_ARCH_MSM8916),y)
	# TODO:
endif

# SVN revision and build date for driver version.
ccflags-y += -D__FF_SVN_REV=\"$(strip $(shell svn info --show-item last-changed-revision $(PWD) 2>/dev/null))\"
ccflags-y += -D__FF_BUILD_DATE=\"$(shell date +%Y%m%d)\"

# Early-logging level for ff_hal, ff_ta and driver as well. Note that this
# logging level only takes effect before ff_config module is loaded.
#
# Options:
# 0 - FF_LOG_LEVEL_ALL, All
# 1 - FF_LOG_LEVEL_VBS, Verbose
# 2 - FF_LOG_LEVEL_DBG, Debug
# 3 - FF_LOG_LEVEL_INF, Info
# 4 - FF_LOG_LEVEL_WRN, Warning
# 5 - FF_LOG_LEVEL_ERR, Error
ccflags-y += -D__FF_EARLY_LOG_LEVEL=2

# Source files.
obj-$(CONFIG_FINGERPRINT_FOCAL_FT9801_QSEE) := focaltech_fp.o
focaltech_fp-objs := ff_ctl.o ff_spi.o spidev.o
ifeq ($(CONFIG_ARCH_MT6735M),y)
	focaltech_fp-objs += plat-mt6735m.o
else \
ifeq ($(CONFIG_ARCH_MT6755),y)
	focaltech_fp-objs += plat-mt6755.o
else \
ifeq ($(CONFIG_ARCH_MT6795),y)
	focaltech_fp-objs += plat-mt6795.o
else 
	focaltech_fp-objs += plat-msm8916.o
endif

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else

.PHONY: all mt6735m mt6755 mt6795 msm8916 clean

all:
	@echo "Usage: make <mt6735m|mt6755|mt6795|msm8916|clean>"

# If it's NOT under the Android building environment, you MUST assign the desired variables.
define user-defined-env
ifeq ($(1),mt6735m)
	ANDROID_BUILD_TOP   := /opt/mt6737/bootimage
	ANDROID_PRODUCT_OUT := $$(ANDROID_BUILD_TOP)/out/target/product/pixi564g
	ANDROID_TOOLCHAIN   := /opt/mt6795/marshmellow/prebuilts/gcc/linux-x86/arm/cit-arm-eabi-4.8/bin
	ARCH                := arm
	CROSS_COMPILE       := $$(ANDROID_TOOLCHAIN)/arm-eabi-
else \
ifeq ($(1),mt6755)
	ANDROID_BUILD_TOP   := /opt/mt6755/bootimage
	ANDROID_PRODUCT_OUT := $$(ANDROID_BUILD_TOP)/out/target/product/pop564g
	ANDROID_TOOLCHAIN   := /opt/tools/android/android-ndk-r16b/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin
	ARCH                := arm64
	CROSS_COMPILE       := $$(ANDROID_TOOLCHAIN)/aarch64-linux-android-
else \
ifeq ($(1),mt6795)
	ANDROID_BUILD_TOP   := /opt/mt6795/marshmellow
	ANDROID_PRODUCT_OUT := $$(ANDROID_BUILD_TOP)/out/target/product/amt6795_evb_m
	ANDROID_TOOLCHAIN   := $$(ANDROID_BUILD_TOP)/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin
	ARCH                := arm64
	CROSS_COMPILE       := $$(ANDROID_TOOLCHAIN)/aarch64-linux-android-
else \
ifeq ($(1),msm8916)
	ANDROID_BUILD_TOP   := /home/$$(USER)/work/coolpad/Qcom/3600I/mydroid
	ANDROID_PRODUCT_OUT := $$(ANDROID_BUILD_TOP)/out/target/product/CP3600I
	ANDROID_TOOLCHAIN   := $$(ANDROID_BUILD_TOP)/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin
	ARCH                := arm64
	CROSS_COMPILE       := $$(ANDROID_TOOLCHAIN)/aarch64-linux-android-
endif
endef

ARCH          ?= arm64
CROSS_COMPILE ?= $(ANDROID_TOOLCHAIN)/aarch64-linux-android-
KBUILDDIR     ?= $(ANDROID_PRODUCT_OUT)/obj/KERNEL_OBJ
MODULEPWD     := $(shell pwd)

mt6735m mt6755 mt6795 msm8916:
	@echo "Building driver module for '$@'..."
ifeq ($(ANDROID_BUILD_TOP),)
	$(eval $(call user-defined-env,$@))
endif
	make -C $(KBUILDDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(MODULEPWD) CONFIG_FINGERPRINT_FOCAL_FT9801_QSEE=m modules
	@echo "Done."
ifneq ($(findstring succeeded,$(shell adb root 2>/dev/null; adb remount 2>/dev/null)),)
	adb shell 'pkill -KILL fingerprint 2>/dev/null || pkill -l KILL fingerprint'
	MODFILE=$$(ls *.ko); MODNAME=$${MODFILE%.ko}; MODPATH=/vendor/focaltech; \
	adb shell "mkdir -p $${MODPATH}/"; \
	adb shell "chcon -v u:object_r:ff_data_file:s0 $${MODPATH}/"; \
	adb push $${MODFILE} $${MODPATH}/; \
	adb shell sync; \
	adb shell "rmmod $${MODNAME} 2>/dev/null"; \
	adb shell "insmod $${MODPATH}/$${MODFILE} && chown system:system /dev/$${MODNAME}"; \
	adb shell "chcon -v u:object_r:ff_device:s0 /dev/$${MODNAME}"
	adb shell 'chown system:system /dev/spidev?.? 2>/dev/null || lsmod'
endif

clean:
	rm -rf *.o *.mod.c .*.su *.su .*.cmd .*tmp_versions *.symvers *.order *.ko

endif # End of KERNELRELEASE.

