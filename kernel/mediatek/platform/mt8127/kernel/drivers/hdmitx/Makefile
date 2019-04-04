# drivers/video/mtk/Makefile
#
# Makefile for MT6516 video driver.
#

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/hdmitx/  \
             -I$(MTK_PATH_SOURCE)/drivers/hdmitx/    \
             -I$(MTK_PATH_PLATFORM)/drivers/video/   \
             -I$(MTK_PATH_SOURCE)/drivers/video/     \
             -I$(MTK_PATH_PLATFORM)/drivers/dispsys/ \
             -I$(MTK_PATH_SOURCE)/drivers/sync/ \
             -I$(MTK_PATH_SOURCE)/drivers/mhl/driver/mhl_tx_drv

ifeq ($(CUSTOM_KERNEL_HDMI),nxp_tda19989)
	ccflags-y += 	-DHDMI_TDA19989
endif

ifeq ($(MTK_INTERNAL_HDMI_SUPPORT), yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/hdmi/
endif  

ifeq ($(MTK_INTERNAL_MHL_SUPPORT), yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/mhltx/
endif  

ifeq ($(CUSTOM_KERNEL_HDMI),Sii8338)
	ccflags-y += 	-DMHL_SII8338
endif
obj-y	:= hdmitx.o debug.o  hdmitx_test.o

ifeq ($(strip $(MTK_SEC_VIDEO_PATH_SUPPORT)),yes)
	ccflags-y += -DMTK_SEC_VIDEO_PATH_SUPPORT
endif

ifeq ($(strip $(MTK_IN_HOUSE_TEE_SUPPORT)),yes)
	ccflags-y += -I$(MTK_PATH_SOURCE)/include/trustzone/
endif

ifeq ($(MTK_ALPS_BOX_SUPPORT), yes)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif
