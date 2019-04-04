ifeq ($(MTK_INTERNAL_HDMI_SUPPORT), yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/hdmi/
             
ifeq ($(MTK_IN_HOUSE_TEE_SUPPORT), yes)
ccflags-y += -I$(srctree)/../mediatek/kernel/include/trustzone/ -Werror
endif

obj-y := hdmi_drv.o hdmi_ctrl.o hdmiavd.o hdmicec.o hdmiddc.o hdmiedid.o hdmihdcp.o hdmicmd.o hdmictrl.o hdmi_debug.o

ifeq ($(MTK_IN_HOUSE_TEE_SUPPORT), yes)
obj-y += hdmi_ca.o
endif

endif