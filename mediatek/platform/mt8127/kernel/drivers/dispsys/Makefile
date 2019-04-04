# drivers/dispsys

ccflags-y += -I$(MTK_PATH_SOURCE)/drivers/video/ \
             -I$(MTK_PATH_PLATFORM)/drivers/video/
             
ifeq ($(MTK_OVERLAY_ENGINE_SUPPORT),yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/videox/
ccflags-y += -DMTK_OVERLAY_ENGINE_SUPPORT

  ifeq ($(MTK_SEC_VIDEO_PATH_SUPPORT),yes)
  ccflags-y += -I$(MTK_PATH_SOURCE)/include/trustzone/ \
             -I$(MTK_PATH_SOURCE)/../kernel/include
  ccflags-y += -DMTK_SEC_VIDEO_PATH_SUPPORT
  endif
else
ccflags-y += -I$(MTK_PATH_PLATFORM)/../../common/kernel/drivers/video/ 
endif



#TODO: the header file under "protect-bsp" folder does not exist for customer release codebase.

obj-y += ddp_drv.o
obj-y += ddp_ovl.o
obj-y += ddp_bls.o
obj-y += ddp_color.o
# obj-y += ddp_tdshp.o
obj-y += ddp_rdma.o
obj-y += ddp_wdma.o
# obj-y += ddp_scl.o
# obj-y += ddp_rot.o
obj-y += ddp_path.o
obj-y += ddp_debug.o
obj-y += ddp_cmdq.o
obj-y += ddp_cmdq_debug.o
obj-y += ddp_cmdq_sec.o
#obj-y += ddp_cmdq_record.o
#obj-y += ddp_cmdq_test.o
# obj-y += ddp_dpfd.o
obj-y += ddp_aal.o
obj-y += data_rgb888_64x64.o
obj-y += data_rgb888_64x64_golden.o

# EOF

ifeq ($(MTK_ALPS_BOX_SUPPORT), yes)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/hdmitx \
			 -I$(MTK_PATH_PLATFORM)/drivers/hdmi
endif


ccflags-y += -I$(MTK_PATH_SOURCE)/../kernel/include/trustzone/tz_cross/
ccflags-y += -I$(MTK_PATH_SOURCE)/../kernel/include/trustzone/
