include $(MTK_PATH_BUILD)/common.mk
#Fix-me: modified for early porting
#$(call all-subdir-src-or-makefile)
obj-y   += uart/
obj-y   += gpio/
obj-y   += pwm/
obj-y   += dual_ccci/
obj-y   += btcvsd/
ifneq ($(MTK_ALPS_BOX_SUPPORT), yes)
	obj-y   += pmic_wrap/
endif
obj-y   += power/
obj-y   += power_gs/
obj-y   += m4u/
obj-y   += smi/
obj-y   += usb11/
obj-y   += usb20/
#ifeq    ($(MTK_EMMC_SUPPORT),yes)
obj-y   += mmc-host/
#endif
ifneq   ($(MTK_EMMC_SUPPORT),yes)
obj-y   += nand/
endif
obj-y   += jpeg/
obj-y   += i2c/
obj-y   += spi/
obj-y   += keypad/

ifeq ($(MTK_OVERLAY_ENGINE_SUPPORT),yes)
obj-y   += videox/
else
obj-y   += ../../../common/kernel/drivers/video/
endif

obj-y   += hdmitx/
obj-y   += video/
ifeq ($(MTK_INTERNAL_HDMI_SUPPORT), yes)
obj-y   += hdmi/
endif
ifeq ($(MTK_INTERNAL_MHL_SUPPORT), yes)
obj-y   += mhltx/
endif
obj-y   += dispsys/
obj-y   += rtc/
obj-y   += wdt/
obj-y   += sound/
obj-y   += devinfo/
obj-y   += devapc/
obj-$(CONFIG_MTK_BTIF)   += btif/
obj-y   += mrdump/

#ifneq ($(MTK_PROJECT), mt8127fpga_ca7_ldvt)

#endif
#ifneq ($(CONFIG_EARLY_LINUX_PORTING),y)
#obj-y   += nand/
obj-y   += auxadc/
#obj-y   += touchpanel/
#obj-y   += spi/
#obj-y   += ccci/
obj-y   += ldvt/
#obj-y   += mmc-host/
obj-y   += videocodec/
#obj-y   += rtc/
#
#obj-y   += jpeg/
#obj-y   += gdma/
obj-y   += masp/
#endif
#
obj-y   += accdet/
obj-y   += vibrator/
obj-y   += leds/
obj-y   += wmt/
obj-y   += thermal/

include $(MTK_PATH_BUILD)/common.mk
obj-y   += $(call all-subdirs-with-makefile,cpu_stress)
obj-y   += pmt/
obj-y   += gcpu/
ifeq ($(TRUSTONIC_TEE_SUPPORT),yes)
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT)   += gud/
endif

ifeq ($(MTK_ALPS_BOX_SUPPORT),yes)
obj-m += ir/
endif

ifeq ($(MTK_ALPS_BOX_SUPPORT),yes)
obj-y += ethernet/
endif

obj-y	+= $(call all-subdirs-with-makefile,met)

ifneq   ($(MTK_GPU_SUPPORT),no)
obj-y   += gpu/
endif

ifeq ($(MTK_NFC_SUPPORT),yes)
obj-y	+= nfc/
endif
