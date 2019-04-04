/*
 * 
 * Copyright (C) 2014-2015 QUANTA COMPUTER INC.
 * MediaTek Inc. (C) 2010. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */


#ifndef BUILD_LK
#include <linux/string.h>
#else
#include <string.h>
#endif
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <platform/upmu_common.h>
#include <debug.h>
#elif (defined BUILD_UBOOT)
#include <asm/arch/mt6577_gpio.h>
#else
#include <mach/mt_gpio.h>
#include <linux/xlog.h>
#include <mach/mt_pm_ldo.h>
#include <mach/upmu_common.h>
#if defined (MTK_KERNEL_POWER_OFF_CHARGING)
#include <linux/xlog.h>
#include <mach/mt_boot.h>
#endif
#endif
#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (1024)
#define FRAME_HEIGHT (600)

#define GPIO_LCD_3V3_EN				GPIO83

#define HSYNC_PULSE_WIDTH 140
#define HSYNC_BACK_PORCH  20
#define HSYNC_FRONT_PORCH 160
#define VSYNC_PULSE_WIDTH 20
#define VSYNC_BACK_PORCH  3 
#define VSYNC_FRONT_PORCH 12

#define LCD_DATA_FORMAT LCD_DATA_FORMAT_VESA8BIT

#define V_TOTAL (FRAME_HEIGHT + VSYNC_PULSE_WIDTH + VSYNC_BACK_PORCH + VSYNC_FRONT_PORCH)
#define H_TOTAL (FRAME_WIDTH + HSYNC_PULSE_WIDTH + HSYNC_BACK_PORCH + HSYNC_FRONT_PORCH)

#define H_START (HSYNC_PULSE_WIDTH + HSYNC_BACK_PORCH)
#define H_END (H_START + FRAME_WIDTH - 1)

#define V_START (VSYNC_PULSE_WIDTH + VSYNC_BACK_PORCH)
#define V_END (V_START + FRAME_HEIGHT - 1)

#define V_DELAY  0x0002  //Fixed Value
#define H_DELAY  (FRAME_WIDTH + HSYNC_PULSE_WIDTH + HSYNC_BACK_PORCH + HSYNC_FRONT_PORCH - V_DELAY)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (mt_set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

static __inline void send_ctrl_cmd(unsigned int cmd)
{

}

static __inline void send_data_cmd(unsigned int data)
{

}

static __inline void set_lcm_register(unsigned int regIndex,
		unsigned int regData)
{

}


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DPI;
	params->ctrl   = LCM_CTRL_SERIAL_DBI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->io_select_mode = 0;

	params->dpi.PLL_CLOCK         = 51;
	params->dpi.clk_pol           = LCM_POLARITY_FALLING;
	params->dpi.de_pol            = LCM_POLARITY_RISING;
	params->dpi.vsync_pol         = LCM_POLARITY_RISING;
	params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

	params->dpi.hsync_pulse_width = HSYNC_PULSE_WIDTH;
	params->dpi.hsync_back_porch  = HSYNC_BACK_PORCH;
	params->dpi.hsync_front_porch = HSYNC_FRONT_PORCH;
	params->dpi.vsync_pulse_width = VSYNC_PULSE_WIDTH;
	params->dpi.vsync_back_porch  = VSYNC_BACK_PORCH;
	params->dpi.vsync_front_porch = VSYNC_FRONT_PORCH;

	//params->dpi.i2x_en = 1;
	params->dpi.lvds_tx_en = 1;

#if defined(QCI_PROJECT_UYTEVB)
	params->dpi.format            = LCM_DPI_FORMAT_RGB666;
#else
	params->dpi.format            = LCM_DPI_FORMAT_RGB888;
#endif
	params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
	params->dpi.is_serial_output  = 0;

	params->dpi.intermediat_buffer_num = 0;

	//params->dpi.io_driving_current = LCM_DRIVING_CURRENT_2MA;
}

void lcd_power_en(unsigned char enabled)
{
	if(enabled)
	{
#ifdef BUILD_LK
	printf("[LCM] Power On\n");
#else
	printk("[LCM] Power On\n");
#endif
		mt_set_gpio_mode(GPIO_LCD_3V3_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_3V3_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_3V3_EN, GPIO_OUT_ONE);
		MDELAY(10);
	}
	else
	{
#ifdef BUILD_LK
	printf("[LCM] Power Off\n");
#else
	printk("[LCM] Power Off\n");
#endif
		MDELAY(10);
		mt_set_gpio_mode(GPIO_LCD_3V3_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_3V3_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_3V3_EN, GPIO_OUT_ZERO);
	}
}

void lcd_bl_en(unsigned char enabled)
{
}


static void lcm_init(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_init()  \n");

	//lcd_power_en(1);
	
#elif (defined BUILD_UBOOT)
	// do nothing in uboot
#else
	printk("[LCM] lcm_init() enter\n");
#endif
}


static void lcm_suspend(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_suspend() enter\n");
#else
	printk("[Kernel/LCM] lcm_suspend() enter\n");
#endif

#ifndef BUILD_LK
#if defined (MTK_KERNEL_POWER_OFF_CHARGING)
    if(g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT || g_boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
    {
        lcd_power_en(0);
        return 0;
    }
#endif
#endif
	return;
}


static void lcm_resume(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_resume() enter\n");
#else
	printk("[Kernel/LCM] lcm_resume() enter\n");
#endif

#ifndef BUILD_LK
#if defined (MTK_KERNEL_POWER_OFF_CHARGING)
    if(g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT || g_boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
    {
        lcd_power_en(1);
        return 0;
    }
#endif
#endif
	return;
}

LCM_DRIVER hx8282_wsvga_lvds_kingdisplay_lcm_drv = 
{
	.name           = "HX8282_WSVGA_LVDS_KINGDISPLAY",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};


