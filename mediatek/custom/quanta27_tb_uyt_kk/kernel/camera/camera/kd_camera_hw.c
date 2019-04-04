#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>
#include <mach/upmu_common.h>

#include "kd_camera_hw.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"

#include <mach/upmu_common_mt6397.h>
/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[kd_camera_hw]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...)         xlog_printk(ANDROID_LOG_ERR, PFX , fmt, ##arg)
#define PK_XLOG_INFO(fmt, args...) \
                do {    \
                   xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg); \
                } while(0)
#else
#define PK_DBG(a,...)
#define PK_ERR(a,...)
#define PK_XLOG_INFO(fmt, args...)
#endif

/*  //No use when using PMIC MT6323, this is for PMIC MT6397
void turn_on_blue_led()
{
	//do when on
	upmu_set_isinks_ch2_mode(0x0);
	upmu_set_isinks_ch2_step(0x7);
	upmu_set_isink_dim2_duty(31);
	upmu_set_isink_dim2_fsel(11);
	//on main
	upmu_set_rg_bst_drv_1m_ck_pdn(0x0);
	upmu_set_isink_rsv2_isink2(0x1);
	upmu_set_isinks_ch2_en(0x01);
}
void turn_off_blue_led()
{
	//off main
	upmu_set_isinks_ch2_en(0x00);
	upmu_set_isink_rsv2_isink2(0x00);
}
*/
int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx, char *currSensorName, BOOL On, char* mode_name)
{
#if !defined (MTK_ALPS_BOX_SUPPORT)

u32 pinSetIdx = 0;//default main sensor

#define IDX_PS_CMRST 0
#define IDX_PS_CMPDN 4

#define IDX_PS_MODE 1
#define IDX_PS_ON   2
#define IDX_PS_OFF  3


u32 pinSet[2][8] = {
                    //for main sensor
                    {GPIO_CAMERA_CMRST_PIN,
                        GPIO_CAMERA_CMRST_PIN_M_GPIO,   /* mode */
                        GPIO_OUT_ONE,                   /* ON state */
                        GPIO_OUT_ZERO,                  /* OFF state */
                     GPIO_CAMERA_CMPDN_PIN,
                        GPIO_CAMERA_CMPDN_PIN_M_GPIO,
                        GPIO_OUT_ONE,
                        GPIO_OUT_ZERO,
                    },
                    //for sub sensor
                    {GPIO_CAMERA_CMRST1_PIN,
                     GPIO_CAMERA_CMRST1_PIN_M_GPIO,
                        GPIO_OUT_ONE,
                        GPIO_OUT_ZERO,
                     GPIO_CAMERA_CMPDN1_PIN,
                        GPIO_CAMERA_CMPDN1_PIN_M_GPIO,
                        GPIO_OUT_ONE,
                        GPIO_OUT_ZERO,
                    },
                   };







    if (DUAL_CAMERA_MAIN_SENSOR == SensorIdx){
        pinSetIdx = 0;
    }
    else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx) {
        pinSetIdx = 1;
    }

   
    //power ON
    if (On) {
		
		PK_DBG("kdCISModulePowerOn -on:currSensorName=%s;\n",currSensorName);
		PK_DBG("kdCISModulePowerOn -on:pinSetIdx=%d\n",pinSetIdx);

		 if ((pinSetIdx==0) && currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV5647MIPI_RAW, currSensorName)))
		 {
			 //enable active sensor
			 if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
				 //RST pin,active low
				 if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
				 mdelay(5);
		 
				 //PDN pin,high
				 if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
				 mdelay(5);
			 }
		 
			 //DOVDD
			 PK_DBG("[ON_general 1.8V]sensorIdx:%d \n",SensorIdx);
			 if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
			 {
				 PK_DBG("[CAMERA SENSOR] Fail to enable D2 power\n");
				 //return -EIO;
				 //goto _kdCISModulePowerOn_exit_;
			 }
			 mdelay(5);
		 
			 //AVDD
			 if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
			 {
				 PK_DBG("[CAMERA SENSOR] Fail to enable A power\n");
				 //return -EIO;
				 //goto _kdCISModulePowerOn_exit_;
			 }
			 mdelay(5);
		 
			 //DVDD
			 if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,mode_name))
			 {
				  PK_DBG("[CAMERA SENSOR] Fail to enable D power\n");
				  //return -EIO;
				  //goto _kdCISModulePowerOn_exit_;
			 }
			 mdelay(5);
			 
			 //AF_VCC
			 if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
			 {
				  PK_DBG("[CAMERA SENSOR] Fail to enable A2 power\n");
				  //return -EIO;
				  goto _kdCISModulePowerOn_exit_;
			 }
			 mdelay(5);
		 
			 //enable active sensor
			 if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
				 //RST pin
				 if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
				 mdelay(5);
		 
				 //PDN pin
				 if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
				 mdelay(5);
			 }
#if 0
                         while(1){

				mdelay(2000);
				PK_DBG("im sleep ...\n");
                         }
#endif
#if 1
			 //disable inactive sensor
			 if(pinSetIdx == 0 || pinSetIdx == 2) {//disable sub
				 if (GPIO_CAMERA_INVALID != pinSet[1][IDX_PS_CMRST]) {
					 if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
					 if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
					 if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
					 if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
					 if(mt_set_gpio_out(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
					 if(mt_set_gpio_out(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
				 }
			 }
			 else {
				 if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
					 if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
					 if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
					 if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
					 if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
					 if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
					 if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
				 }
			 }
#endif
		 }
		else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K5E2YA_MIPI_RAW, currSensorName)))
		{
			printk("[Jason] power on s5k5e2ya...\n");
			//AVDD			
			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				//return -EIO;
				//goto _kdCISModulePowerOn_exit_;
			}
			mdelay(2);
			//DVDD
			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1200,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
				//return -EIO;
				//goto _kdCISModulePowerOn_exit_;
			}
			mdelay(2);
			//DOVDD
			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
				//return -EIO;
				//goto _kdCISModulePowerOn_exit_;
			}
			mdelay(2);
			//AF_
			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				//return -EIO;
				//goto _kdCISModulePowerOn_exit_;
			}
			mdelay(2);
			//RST pull high
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			//low reset front-camera (mt9m114) at rear-camera detection period
			if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			printk("[Jason] power on s5k5e2ya finish!!!\n");
			//on led
			//turn_on_blue_led();
		}
		else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_MT9M114_MIPI_YUV, currSensorName)))
		{
			PK_DBG("[Jason]Power-on mt9m114!!!!\n");
			//high reset at beginning, we do not want it do hw reset(low enable) at any time when power is on
			//Please do NOT do low reset after power on mt9m114, to avoid low light flicker bug
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			//no power-down pin, no set
				 if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
			//DVDD
			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
			 {
				  PK_DBG("[CAMERA SENSOR] Fail to enable D power\n");
				  //return -EIO;
				  //goto _kdCISModulePowerOn_exit_;
			 }
			//VCAMA
			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
			 {
				 PK_DBG("[CAMERA SENSOR] Fail to enable A power\n");
				 //return -EIO;
				 //goto _kdCISModulePowerOn_exit_;
			 }
			mDELAY(20);
			//high reset at the operation time
			//fake high pdn
				if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
			PK_DBG("[Jason]Power-on mt9m114 finished!!!!\n");
			//on led
			//turn_on_blue_led();
		}
		else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2355_MIPI_RAW, currSensorName)))
		{
			PK_DBG("[Jason]power on gc2355 ...start!!!\n");
			//DOVDD
            		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
            		{
                		PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                		//return -EIO;
                		//goto _kdCISModulePowerOn_exit_;
            		}
            		//DVDD
            		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name))
            		{
                		PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                		//return -EIO;
                		//goto _kdCISModulePowerOn_exit_;
            		}
			//VCAMA
			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
			 {
				 PK_DBG("[CAMERA SENSOR] Fail to enable A power\n");
				 //return -EIO;
				 //goto _kdCISModulePowerOn_exit_;
			 }
			//pdn H-->L
	if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
			//rst L-->H
	if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			PK_DBG("[Jason]power on gc2355 ...finish!!!\n");
		}
else if (currSensorName && ( 0 == strcmp(SENSOR_DRVNAME_HI258_MIPI_YUV,currSensorName)))
{
        PK_DBG("[Jason]Power-on Hi258!!!!\n");
        //DOVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
        {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
        }
        //AVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
        {
                PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
        }
        //DVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name))
        {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
        }
        //PDN H-->L

        mdelay(5);
        printk("[Jason]Trigger PowerDown: Low\n");
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}

        mdelay(30);
        //RST L-->H
        printk("[Jason]Trigger reset: Low -> High\n");
        if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(1);
        PK_DBG("[Jason]Power-on Hi258 finished!!!!\n");
}
		else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV2659_YUV, currSensorName)))
		{
        //enable active sensor
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            //RST pin,active low
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(5);

            //PDN pin,high
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
            mdelay(5);
        }
		
		/*  Enlarge the VCAMIO voltage to increase driving capability to avoid unexpected lines shown  
			VCAMIO 1.8V + 0.02V ~ 0.16V
			4'b1000: 160 mV
			4'b1001: 140 mV
			4'b1010: 120 mV
			4'b1011: 100 mV
			4'b1100: 80 mV
			4'b1101: 60 mV
			4'b1110: 40 mV
			4'b1111: 20 mV
		*/
		//upmu_set_rg_vcamio_cal(0x08);		// 1.96V

        //DOVDD
        PK_DBG("[ON_general 1.8V]sensorIdx:%d \n",SensorIdx);
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
            //return -EIO;
            //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(5);

        //AVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
            //return -EIO;
            //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(5);

        //DVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,mode_name))
        {
             PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
             //return -EIO;
             //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(5);

		#if 0
       // AF_VCC
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
       {
             PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
             //return -EIO;
             goto _kdCISModulePowerOn_exit_;
        }
        #endif     

		//enable active sensor
		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
			//RST pin
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			mdelay(5);
		
			//PDN pin
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
			mdelay(5);
		}


	 //disable inactive sensor
	 if(pinSetIdx == 0 || pinSetIdx == 2) {//disable sub
		 if (GPIO_CAMERA_INVALID != pinSet[1][IDX_PS_CMRST]) {
			 if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			 if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			 if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			 if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			 if(mt_set_gpio_out(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
			 if(mt_set_gpio_out(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
		 }
	 }
	 else {
		 if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
			 if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			 if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			 if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			 if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			 if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
			 if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
		 }
	 }


	 }
else if (currSensorName && ( 0 == strcmp(SENSOR_DRVNAME_GC2235_MIPI,currSensorName)))
{
            PK_DBG("[Jason]Power-on GC2235!!!!\n");
            // Trigger low reset, ensure reset is low at beginning
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            // PowerDown: Default High
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            //DVDD
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }

            //DOVDD
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }
            mdelay(2);

            //AVDD
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }
            mdelay(2);

            //Power Down set to low 
            printk("[Jason]Trigger PowerDown: Low\n");
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}

            //Trigger low reset again for 2ms
            printk("[Jason]Trigger reset: Low -> High\n");
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(2);

            //Trigger high reset
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            PK_DBG("[Jason]Power-on GC2235 finished!!!!\n");
}

    	}
    else {//power OFF
if (currSensorName && ( 0 == strcmp(SENSOR_DRVNAME_GC2235_MIPI,currSensorName)))
{
        PK_DBG("[Jason]Power-off GC2235!!!!\n");
        // Trigger low to reset sensor
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
        {
                printk("[Jason]Trigger reset: Low\n");
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        }
        //Power Down set to high 
        printk("[Jason]Trigger PowerDown: High\n");
        if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
        if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        //AVDD
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(2);
        //DOVDD
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
        //DVDD
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }

        PK_DBG("[Jason]Power-off GC2235 finished!!!!\n");
}
else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K5E2YA_MIPI_RAW, currSensorName)))
{
	printk("[Jason] power off s5k5e2ya...\n");

	//RST pull low
	if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	mdelay(2);
	//AF_
	if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
	mdelay(2);
	//DVDD
	if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
	mdelay(2);
	//DOVDD
	if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
	mdelay(2);
	//AVDD			
	if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
			
			
	printk("[Jason] power off s5k5e2ya finish!!!\n");
	//off led
	//turn_off_blue_led();
}
else if ((pinSetIdx==0) && currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV5647MIPI_RAW, currSensorName)))
{
		PK_DBG("[Jason]Power-off ov5647!!!!\n");
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
			mdelay(1);
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
			mdelay(2);
		}
		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		}  
		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name)) {
			PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		}

		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name)) {
			PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		}

		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		}  
		PK_DBG("[Jason]Power-off ov5647 finished!!!!\n");
}
else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_MT9M114_MIPI_YUV, currSensorName)))
		{
			PK_DBG("[Jason]Power-off mt9m114!!!!\n");
			//low reset 
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			//no power-down pin, no set
				if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
			//VCAMA
        		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name))
        		{
           			PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
           			//return -EIO;
           			//goto _kdCISModulePowerOn_exit_;
        		}
			//DVDD
			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
        		{
           			PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
           			//return -EIO;
           			//goto _kdCISModulePowerOn_exit_;
        		}
			PK_DBG("[Jason]Power-off mt9m114 finished!!!!\n");
			//off led
			//turn_off_blue_led();
		}
else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2355_MIPI_RAW, currSensorName)))
{
	PK_DBG("[Jason]Power-off gc2355 start\n");
	//pdn L-->H
	if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	//rst H-->L
	if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	//AVDD
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name))
        {
           	PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
           	//return -EIO;
           	//goto _kdCISModulePowerOn_exit_;
        }
	//DVDD
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
        //DOVDD
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
	PK_DBG("[Jason]Power-off gc2355 finished!!!!\n");
}
else if (currSensorName && ( 0 == strcmp(SENSOR_DRVNAME_HI258_MIPI_YUV,currSensorName)))
{
        PK_DBG("[Jason]Power-off Hi258!!!!\n");
        // Trigger low to reset sensor
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
        {
                printk("[Jason]Trigger reset: Low\n");
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        }
        mdelay(1);
        //Power Down set to high 
        printk("[Jason]Trigger PowerDown: High\n");
        if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
        if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        //DVDD
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
        //AVDD
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
        //DOVDD
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
        {
           PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
           //return -EIO;
           //goto _kdCISModulePowerOn_exit_;
        }
        PK_DBG("[Jason]Power-off Hi258 finished!!!!\n");
}
else{
        //PK_DBG("[OFF]sensorIdx:%d \n",SensorIdx);
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
    	    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
        }

    	if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name)) {
            PK_DBG("[CAMERA SENSOR] Fail to OFF a power\n");
        }
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name)){
            PK_DBG("[CAMERA SENSOR] Fail to OFF a2 power\n");
        }
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name)) {
            PK_DBG("[CAMERA SENSOR] Fail to OFF d digital power\n");
        }
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to OFF d2 digital power\n");
        }
}
    }//
    
#endif /* end of defined MTK_ALPS_BOX_SUPPORT */

	return 0;

_kdCISModulePowerOn_exit_:
    return -EIO;
}

EXPORT_SYMBOL(kdCISModulePowerOn);


//!--
//




