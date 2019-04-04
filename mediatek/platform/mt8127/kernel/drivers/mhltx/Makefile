ifeq ($(MTK_INTERNAL_MHL_SUPPORT), yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/mhltx/

ifeq ($(MTK_IN_HOUSE_TEE_SUPPORT), yes)
ccflags-y += -I$(srctree)/../mediatek/kernel/include/trustzone/ -Werror
endif

obj-y	:=mhl_drv.o 
obj-y	+=mhl_avd.o 
obj-y	+=mhl_ctrl.o 
obj-y	+=mhl_cbus.o 
obj-y	+=mhl_cbus_ctrl.o 
obj-y	+=mhl_edid.o 
obj-y	+=mhl_hdcp.o 
obj-y	+=mhl_keycode.o 

ifeq ($(MTK_IN_HOUSE_TEE_SUPPORT), yes)
obj-y += hdmi_ca.o
endif

endif



