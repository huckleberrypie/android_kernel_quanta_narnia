/* Quanta Projects Version Control
 *
 * Copyright (C) 2011 Quanta Computer Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*-----------------------------------------------------------------------------
 * Global Include files
 *---------------------------------------------------------------------------*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/autoconf.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/vermagic.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/mtgpio.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>

#include <mach/mt_boot.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio.h>
#include <mach/mt_hardware_info.h>

/*-----------------------------------------------------------------------------
 * Local Include files
 *---------------------------------------------------------------------------*/
#if 0
#include "tpd_custom_gt9xx.h"
#endif
/*-----------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/
#define KERNEL_VERSION 		UTS_RELEASE
#define PROJECTS_PROC_NAME	"img_version"
#define BSP_VERSION 		"KK1.MP10.V1.52"
#define IMAGE_VERSION 		"0.0.1"

/*-----------------------------------------------------------------------------
 * Marcos
 *---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Global variables
 *---------------------------------------------------------------------------*/
#if 0
extern s32 cam_module_id;
extern s32 sub_cam_module_id;
#endif
/*-----------------------------------------------------------------------------
 * Global Functions
 *---------------------------------------------------------------------------*/
int qci_mainboard_version()
{
	#define GPIO_REV_ID_0 GPIO85
	#define GPIO_REV_ID_1 GPIO86
	#define GPIO_REV_ID_2 GPIO87

	static int rev_id	= -1;

	if (rev_id == -1) {
		/* Change Mulit-Function PINS to GPIO Setting */
		mt_set_gpio_mode (GPIO_REV_ID_0, GPIO_MODE_00);
		mt_set_gpio_mode (GPIO_REV_ID_1, GPIO_MODE_00);
		mt_set_gpio_mode (GPIO_REV_ID_2, GPIO_MODE_00);

		/* Direction Setting */
		mt_set_gpio_dir (GPIO_REV_ID_0, GPIO_DIR_IN);
		mt_set_gpio_dir (GPIO_REV_ID_1, GPIO_DIR_IN);
		mt_set_gpio_dir (GPIO_REV_ID_2, GPIO_DIR_IN);

		/* Get GPIO Data */
		rev_id	= mt_get_gpio_in (GPIO_REV_ID_2);
		rev_id <<= 1;
		rev_id	+= mt_get_gpio_in (GPIO_REV_ID_1);
		rev_id <<= 1;
		rev_id	+= mt_get_gpio_in (GPIO_REV_ID_0);
	}
	
	return rev_id;
}

int qci_mem_vendor()
{
	return MEM_SAMSUNG;
}

int qci_maincam_vendor()
{
#if 0
	return MAINCAM_MCNEX;
#endif
}

int qci_subcam_vendor()
{
#if 0
	return SUBCAM_LITEON;
#endif
}

/*-----------------------------------------------------------------------------
 * Local Functions
 *---------------------------------------------------------------------------*/

static int version_proc_show(struct seq_file *m, void *v)
{
	int	ret = -1;
	char	*hwver_ptr = NULL;
	char	*hwmemtype_ptr = NULL;
	char	*hwtptype_ptr = NULL;
	char	*hwmaincamtype_ptr = NULL;
	char	*hwsubcamtype_ptr = NULL;

	ret = qci_mainboard_version();

	if (ret == HW_REV_A)
		hwver_ptr = "A";
	else if (ret == HW_REV_B)
		hwver_ptr = "B";
	else if (ret == HW_REV_C)
		hwver_ptr = "C";
	else if (ret == HW_REV_D)
		hwver_ptr = "D";
	else if (ret == HW_REV_E)
		hwver_ptr = "E";
	else if (ret == HW_REV_F)
		hwver_ptr = "F";
	else if (ret >= HW_REV_RESERVE)
		hwver_ptr = "?";

	ret = -1;
	ret = qci_mem_vendor();
	if (ret == MEM_ELPIDA)
		hwmemtype_ptr = "ELPIDA";
	else if (ret == MEM_SAMSUNG)
		hwmemtype_ptr = "SAMSUNG";
	else if (ret == MEM_HYNIX)
		hwmemtype_ptr = "HYNIX";
	else if (ret == MEM_KINGSTON)
		hwmemtype_ptr = "KINGSTON";
	else if (ret >= MEM_RESERVE)
		hwmemtype_ptr = "Unknown";

	ret = -1;
	ret = qci_maincam_vendor();
	if (ret == MAINCAM_MCNEX)
		hwmaincamtype_ptr = "McNex";
	else if (ret == MAINCAM_LITEON)
		hwmaincamtype_ptr = "LiteOn";
	else if (ret == MAINCAM_BISON)
		hwmaincamtype_ptr = "Bison";
	else
		hwmaincamtype_ptr = "Unknown";

	ret = -1;
	ret = qci_subcam_vendor();
	if (ret == SUBCAM_MCNEX)
		hwsubcamtype_ptr = "McNex";
	else if (ret == SUBCAM_LITEON)
		hwsubcamtype_ptr = "LiteOn";
	else if (ret == SUBCAM_BISON)
		hwsubcamtype_ptr = "Bison";
	else
		hwsubcamtype_ptr = "Unknown";


	seq_printf(m, "BSP version : %s\n", BSP_VERSION);
	seq_printf(m, "Kernel version : %s\n", KERNEL_VERSION);
	seq_printf(m, "Image version : %s\n", IMAGE_VERSION);
	seq_printf(m, "HW version : %s\n", hwver_ptr);
	seq_printf(m, "Memory vendor : %s\n", hwmemtype_ptr);

#if 0
	seq_printf(m, "Touchpanel module vendor : %s\n", gFocus_module_msg.name);
#endif

	seq_printf(m, "MainCam module vendor : %s\n", hwmaincamtype_ptr);
	seq_printf(m, "SubCam module vendor : %s\n", hwsubcamtype_ptr);

	seq_printf(m, "Chip code: %04X\n", get_chip_code());
	seq_printf(m, "Chip HW subcode: %04X\n", get_chip_hw_subcode());
	seq_printf(m, "Chip HW version code: %04X\n", get_chip_hw_ver_code());
	seq_printf(m, "Chip SW version code: %04X\n", get_chip_sw_ver_code());
	seq_printf(m, "Chip SW version: %x\n", mt_get_chip_sw_ver());

	return 0;
}

static int version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, version_proc_show, NULL);
}

static const struct file_operations version_proc_fops = {
	.open		= version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_version_init(void)
{
	proc_create(PROJECTS_PROC_NAME, 0, NULL, &version_proc_fops);

	return 0;
}

static void __exit proc_version_exit(void)
{
	remove_proc_entry(PROJECTS_PROC_NAME, NULL);
}

module_init(proc_version_init);
module_exit(proc_version_exit);

MODULE_AUTHOR("Quanta Computer Inc.");
MODULE_DESCRIPTION("Quanta Projects Version Control");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL v2");
