obj-$(CONFIG_MTK_FB) := dsi_drv.o dpi_drv.o dpi1_drv.o lcd_drv.o disp_drv_dbi.o disp_drv_dpi.o disp_drv_dsi.o disp_hal.o disp_intr.o

ccflags-y += -I$(MTK_PATH_SOURCE)/drivers/video/ \
			 -I$(MTK_PATH_PLATFORM)/drivers/dispsys/ 
			 
ifeq ($(MTK_OVERLAY_ENGINE_SUPPORT),yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/videox/
ccflags-y += -DMTK_OVERLAY_ENGINE_SUPPORT
else
ccflags-y += -I$(MTK_PATH_PLATFORM)/../../common/kernel/drivers/video/
endif			 

ifeq ($(MTK_GPU_SUPPORT), yes)
ccflags-y += -DHWGPU_SUPPORT
endif

ifeq ($(MTK_INTERNAL_MHL_SUPPORT), yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/mhltx/
endif

ifeq ($(MTK_INTERNAL_HDMI_SUPPORT), yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/hdmi/
endif

ifeq ($(MTK_ALPS_BOX_SUPPORT), yes)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif

