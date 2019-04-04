ifeq ($(MTK_PMIC_MT6397),yes)
  ifeq ($(MTK_ALPS_BOX_SUPPORT), yes)
    obj-$(CONFIG_MTK_PMIC) += pmic_mt6397_dummy.o upmu_common.o 
  else
	  obj-$(CONFIG_MTK_PMIC) += pmic_mt6397.o upmu_common.o pmic_chr_type_det.o
	endif
else
	obj-$(CONFIG_MTK_PMIC) += pmic_mt6323.o upmu_common.o  pmic_chr_type_det.o
endif
ifeq ($(MTK_MT6333_SUPPORT),yes)
    obj-$(CONFIG_MTK_PMIC) += mt6333.o
endif

ifeq ($(MTK_INTERNAL_MHL_SUPPORT), yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/mhltx/
endif  

obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_hal.o 

#ifeq ($(MTK_BQ27541_SUPPORT),yes)
#    obj-$(CONFIG_MTK_SMART_BATTERY) += bq27541.o
#endif
ifeq ($(MTK_BQ24196_SUPPORT),yes) 
    obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_bq24196.o bq24196.o tbl_helper_func.o
else
ifeq ($(MTK_NCP1851_SUPPORT),yes) 
    obj-$(CONFIG_MTK_SMART_BATTERY) += ncp1851.o charging_hw_ncp1851.o tbl_helper_func.o
else
ifeq ($(MTK_NCP1854_SUPPORT),yes) 
    obj-$(CONFIG_MTK_SMART_BATTERY) += ncp1854.o charging_hw_ncp1854.o tbl_helper_func.o
else
ifeq ($(MTK_FAN5405_SUPPORT),yes)
    obj-$(CONFIG_MTK_SMART_BATTERY) += fan5405.o charging_hw_fan5405.o tbl_helper_func.o
else
ifeq ($(MTK_BQ24158_SUPPORT),yes)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq24158.o charging_hw_bq24158.o tbl_helper_func.o
else
ifeq ($(MTK_BQ24296_SUPPORT),yes) 
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq24296.o charging_hw_bq24296.o tbl_helper_func.o
else
ifeq ($(MTK_SWCHR_SUPPORT),yes)
    obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_6333.o                
else
    obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_pmic.o 
endif 
endif 
endif 
endif 
endif
endif
endif

ifeq ($(MTK_ALPS_BOX_SUPPORT), yes)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif
