/*
	
	Copyright (C) 2006-2014 ILITEK TECHNOLOGY CORP.
	Copyright (C) 2014-2015 QUANTA COMPUTER INC.

	Description:	ILITEK based touchscreen  driver .
	
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, see the file COPYING, or write
	to the Free Software Foundation, Inc.,
	51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


	ilitek I2C touch screen driver for Android platform

	Author:	 Steward Fu
	Maintain:Luca Hsu 
	Version: 1
	History:
		2010/10/26 Firstly released
		2010/10/28 Combine both i2c and hid function together
		2010/11/02 Support interrupt trigger for I2C interface
		2010/11/10 Rearrange code and add new IOCTL command
		2010/11/23 Support dynamic to change I2C address
		2010/12/21 Support resume and suspend functions
		2010/12/23 Fix synchronous problem when application and driver work at the same time
		2010/12/28 Add erasing background before calibrating touch panel
		2011/01/13 Rearrange code and add interrupt with polling method
		2011/01/14 Add retry mechanism
		2011/01/17 Support multi-point touch
		2011/01/21 Support early suspend function
		2011/02/14 Support key button function
		2011/02/18 Rearrange code
		2011/03/21 Fix counld not report first point
		2011/03/25 Support linux 2.36.x 
		2011/05/31 Added "echo dbg > /dev/ilitek_ctrl" to enable debug message
				   Added "echo info > /dev/ilitek_ctrl" to show tp informaiton
				   Added VIRTUAL_KEY_PAD to enable virtual key pad
				   Added CLOCK_INTERRUPT to change interrupt from Level to Edge
				   Changed report behavior from Interrupt to Interrupt with Polling
				   Added disable irq when doing firmware upgrade via APK, it needs to use APK_1.4.9
		2011/06/21 Avoid button is pressed when press AA
		2011/08/03 Added ilitek_i2c_calibration function
		2011/08/18 Fixed multi-point tracking id
				   Added ROTATE_FLAG to change x-->y, y-->x
				   Fixed when draw line from non-AA to AA, the line will not be appeared on screen.
		2011/09/29 Added Stop Polling in Interrupt mode
				   Fixed Multi-Touch return value
				   Added release last point
		2011/10/26 Fixed ROTATE bug
				   Added release key button when finger up.
				   Added ilitek_i2c_calibration_status for read calibration status
		2011/11/09 Fixed release last point issue
				   enable irq when i2c error.
		2011/11/28 implement protocol 2.1.
		2012/02/10 Added muti_touch key.
				   Added interrupt flag
		2012/04/02 Added input_report_key , Support Android 4.0
		2013/01/04 remove release event ABS_MT_TOUCH_MAJOR.
		2013/01/17 Added protocol 1.6 upgrade flow.(for APK 1.4.16.1)
				   Added to stop the reported point function.
		2013/04/11 added report point protocol 3.0
				   Fixed protocol 1.6 upgrade flow support 4,8,16,32 byte update(for APK 1.4.17.0)
		2013/04/23 added report key protocol 3.0
		2013/05/28 added ilitek_i2c_reset function
				   Fixed versions show the way	
		2013/08/29 Fixed protocol 2.0 report ,  remove release event ABS_MT_TOUCH_MAJOR.
				   Added set input device 	
		
*/

/*
   Sakia Lien add 2014-12-01
   - Modify gpio and interrupt control for MTK platform
 */

#include "ilitek.h"
#ifdef MTK_PLATFORM
static struct i2c_client *i2c_client = NULL;
static const struct i2c_device_id ilitek_tpd_i2c_id[] = {{"ilitek2116",0},{}};
static unsigned short ilitek_force[] = {TPD_I2C_NUMBER,TPD_I2C_ADDR,I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const ilitek_forces[] = { ilitek_force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces,};
static struct i2c_board_info __initdata ilitek_i2c_tpd = { I2C_BOARD_INFO("ilitek2116", TPD_I2C_ADDR)};
static int ilitek_tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int ilitek_tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int ilitek_tpd_i2c_remove(struct i2c_client *client);

struct i2c_driver ilitek_tpd_i2c_driver = {                       
    .probe = ilitek_tpd_i2c_probe,                                   
    .remove = ilitek_tpd_i2c_remove,                           
    .detect = ilitek_tpd_i2c_detect,                           
    .driver.name = "ilitek2116", 
    .id_table = ilitek_tpd_i2c_id,                             
    .address_list = (const unsigned short*) ilitek_forces,
}; 

static int ilitek_tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) {
    strcpy(info->type, "mtk-tpd");
    return 0;
}
#endif /* MTK_PLATFORM */

unsigned long *CTPM_FW;
char *pbt_buf;

int touch_key_hold_press = 0;
int touch_key_code[] = {KEY_MENU,KEY_HOME,KEY_BACK,KEY_VOLUMEDOWN,KEY_VOLUMEUP};
int touch_key_press[] = {0, 0, 0, 0, 0};
unsigned long touch_time=0;

int driver_information[] = {DERVER_VERSION_MAJOR,DERVER_VERSION_MINOR,RELEASE_VERSION,CUSTOMER_ID,MODULE_ID,PLATFORM_ID,PLATFORM_MODULE,ENGINEER_ID};

// module information
MODULE_AUTHOR("Steward_Fu");
MODULE_DESCRIPTION("ILITEK I2C touch driver for Android platform");
MODULE_LICENSE("GPL");

// global variables
static struct i2c_data i2c;
static struct dev_data dev;
static char DBG_FLAG;
static char Report_Flag;
volatile static char int_Flag;
volatile static char update_Flag;
static int update_timeout;
#ifdef MTK_I2C_DMA
static u8 *gpDMABuf_va = NULL;
static u32 gpDMABuf_pa = 0;
#endif

#ifndef MTK_PLATFORM
// i2c id table
static const struct i2c_device_id ilitek_i2c_id[] ={
	{ILITEK_I2C_DRIVER_NAME, 0}, {}
};
MODULE_DEVICE_TABLE(i2c, ilitek_i2c_id);

// declare i2c function table
static struct i2c_driver ilitek_i2c_driver = {
	.id_table = ilitek_i2c_id,
	.driver = {.name = ILITEK_I2C_DRIVER_NAME},
	.resume = ilitek_i2c_resume,
	.suspend  = ilitek_i2c_suspend,
	.shutdown = ilitek_i2c_shutdown,
	.probe = ilitek_i2c_probe,
	.remove = ilitek_i2c_remove,
};
#endif /* ! MTK_PLATFORM */

// declare file operations
struct file_operations ilitek_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	.unlocked_ioctl = ilitek_file_ioctl,
#else
	.ioctl = ilitek_file_ioctl,
#endif
	.read = ilitek_file_read,
	.write = ilitek_file_write,
	.open = ilitek_file_open,
	.release = ilitek_file_close,
};

/*
description
	open function for character device driver
prarmeters
	inode
	    inode
	filp
	    file pointer
return
	status
*/
static int 
ilitek_file_open(struct inode *inode, struct file *filp)
{
	DBG("%s\n",__func__);
	return 0; 
}
/*
description
	calibration function
prarmeters
	count
	    buffer length
return
	status
*/
static int ilitek_i2c_calibration(size_t count)
{

	int ret;
	unsigned char buffer[128]={0};
	struct i2c_msg msgs[] = {
		{.addr = i2c.client->addr, .flags = 0, .len = count, .buf = buffer,}
	};
	
	buffer[0] = ILITEK_TP_CMD_ERASE_BACKGROUND;
	msgs[0].len = 1;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	if(ret < 0){
		printk(ILITEK_DEBUG_LEVEL "%s, i2c erase background, failed\n", __func__);
	}
	else{
		printk(ILITEK_DEBUG_LEVEL "%s, i2c erase background, success\n", __func__);
	}

	buffer[0] = ILITEK_TP_CMD_CALIBRATION;
	msgs[0].len = 1;
	msleep(2000);
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	msleep(1000);
	return ret;
}
/*
description
	read calibration status
prarmeters
	count
	    buffer length
return
	status
*/
static int ilitek_i2c_calibration_status(size_t count)
{
	int ret;
	unsigned char buffer[128]={0};
	struct i2c_msg msgs[] = {
		{.addr = i2c.client->addr, .flags = 0, .len = count, .buf = buffer,}
	};
	buffer[0] = ILITEK_TP_CMD_CALIBRATION_STATUS;
	ilitek_i2c_transfer(i2c.client, msgs, 1);
	msleep(500);
	ilitek_i2c_read(i2c.client, ILITEK_TP_CMD_CALIBRATION_STATUS, buffer, 1);
	printk("%s, i2c calibration status:0x%X\n",__func__,buffer[0]);
	ret=buffer[0];
	return ret;
}
/*
description
	write function for character device driver
prarmeters
	filp
	    file pointer
	buf
	    buffer
	count
	    buffer length
	f_pos
	    offset
return
	status
*/
static ssize_t 
ilitek_file_write(
	struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	int ret;
	unsigned char buffer[128]={0};
        
	// before sending data to touch device, we need to check whether the device is working or not
	if(i2c.valid_i2c_register == 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c device driver doesn't be registered\n", __func__);
		return -1;
	}

	// check the buffer size whether it exceeds the local buffer size or not
	if(count > 128){
		printk(ILITEK_ERROR_LEVEL "%s, buffer exceed 128 bytes\n", __func__);
		return -1;
	}

	// copy data from user space
	ret = copy_from_user(buffer, buf, count-1);
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, copy data from user space, failed", __func__);
		return -1;
	}

	// parsing command
	if(strcmp(buffer, "calibrate") == 0){
		ret=ilitek_i2c_calibration(count);
		if(ret < 0){
			printk(ILITEK_DEBUG_LEVEL "%s, i2c send calibration command, failed\n", __func__);
		}
		else{
			printk(ILITEK_DEBUG_LEVEL "%s, i2c send calibration command, success\n", __func__);
		}
		ret=ilitek_i2c_calibration_status(count);
		if(ret == 0x5A){
			printk(ILITEK_DEBUG_LEVEL "%s, i2c calibration, success\n", __func__);
		}
		else if (ret == 0xA5){
			printk(ILITEK_DEBUG_LEVEL "%s, i2c calibration, failed\n", __func__);
		}
		else{
			printk(ILITEK_DEBUG_LEVEL "%s, i2c calibration, i2c protocol failed\n", __func__);
		}
		return count;
	}else if(strcmp(buffer, "dbg") == 0){
		DBG_FLAG=!DBG_FLAG;
		printk("%s, %s message(%X).\n",__func__,DBG_FLAG?"Enabled":"Disabled",DBG_FLAG);
	}else if(strcmp(buffer, "info") == 0){
		ilitek_i2c_read_tp_info();
	}else if(strcmp(buffer, "report") == 0){
		Report_Flag=!Report_Flag;
	}else if(strcmp(buffer, "stop_report") == 0){
		i2c.report_status = 0;
		printk("The report point function is disable.\n");
	}else if(strcmp(buffer, "start_report") == 0){
		i2c.report_status = 1;
		printk("The report point function is enable.\n");
	}else if(strcmp(buffer, "update_flag") == 0){
		printk("update_Flag=%d\n",update_Flag);
	}else if(strcmp(buffer, "irq_status") == 0){
		printk("i2c.irq_status=%d\n",i2c.irq_status);
	}else if(strcmp(buffer, "disable_irq") == 0){
		ilitek_i2c_irq_disable();
		printk("i2c.irq_status=%d\n",i2c.irq_status);
	}else if(strcmp(buffer, "enable_irq") == 0){
		ilitek_i2c_irq_enable();
		printk("i2c.irq_status=%d\n",i2c.irq_status);
	}else if(strcmp(buffer, "reset") == 0){
		printk("start reset\n");
		ilitek_i2c_reset();
		printk("end reset\n");
	}
	return -1;
}

/*
description
        ioctl function for character device driver
prarmeters
	inode
		file node
        filp
            file pointer
        cmd
            command
        arg
            arguments
return
        status
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	static unsigned char buffer[64]={0};
	static int len = 0, i;
	int ret;
	struct i2c_msg msgs[] = {
		{.addr = i2c.client->addr, .flags = 0, .len = len, .buf = buffer,}
        };

	// parsing ioctl command
	switch(cmd){
		case ILITEK_IOCTL_I2C_WRITE_DATA:
			ret = copy_from_user(buffer, (unsigned char*)arg, len);
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, copy data from user space, failed\n", __func__);
				return -1;
			}
#ifdef	SET_RESET
			if(buffer[0] == 0x60){
				ilitek_i2c_reset();
			}
#endif
			ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, i2c write, failed\n", __func__);
				return -1;
			}
			break;
		case ILITEK_IOCTL_I2C_READ_DATA:
			msgs[0].flags = I2C_M_RD;
	
			ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, i2c read, failed\n", __func__);
				return -1;
			}
			ret = copy_to_user((unsigned char*)arg, buffer, len);
			
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, copy data to user space, failed\n", __func__);
				return -1;
			}
			break;
		case ILITEK_IOCTL_I2C_WRITE_LENGTH:
		case ILITEK_IOCTL_I2C_READ_LENGTH:
			len = arg;
			break;
		case ILITEK_IOCTL_DRIVER_INFORMATION:
			for(i = 0; i < 8; i++){
				buffer[i] = driver_information[i];
			}
			ret = copy_to_user((unsigned char*)arg, buffer, 7);
			break;
		case ILITEK_IOCTL_I2C_UPDATE:
			break;
		case ILITEK_IOCTL_I2C_INT_FLAG:
			if(update_timeout == 1){
				buffer[0] = int_Flag;
				ret = copy_to_user((unsigned char*)arg, buffer, 1);
				if(ret < 0){
					printk(ILITEK_ERROR_LEVEL "%s, copy data to user space, failed\n", __func__);
					return -1;
				}
			}
			else
				update_timeout = 1;

			break;
		case ILITEK_IOCTL_START_READ_DATA:
			i2c.stop_polling = 0;
			if(i2c.client->irq != 0 )
				ilitek_i2c_irq_enable();
			i2c.report_status = 1;
			printk("The report point function is enable.\n");
			break;
		case ILITEK_IOCTL_STOP_READ_DATA:
			i2c.stop_polling = 1;
			if(i2c.client->irq != 0 )
				ilitek_i2c_irq_disable();
			i2c.report_status = 0;
			printk("The report point function is disable.\n");
			break;
		case ILITEK_IOCTL_I2C_SWITCH_IRQ:
			ret = copy_from_user(buffer, (unsigned char*)arg, 1);
			if (buffer[0] == 0)
			{
				if(i2c.client->irq != 0 ){
					ilitek_i2c_irq_disable();
				}
			}
			else
			{
				if(i2c.client->irq != 0 ){
					ilitek_i2c_irq_enable();				
				}
			}
			break;	
		case ILITEK_IOCTL_UPDATE_FLAG:
			update_timeout = 1;
			update_Flag = arg;
			DBG("%s,update_Flag=%d\n",__func__,update_Flag);
			break;
		case ILITEK_IOCTL_I2C_UPDATE_FW:
			ret = copy_from_user(buffer, (unsigned char*)arg, 35);
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, copy data from user space, failed\n", __func__);
				return -1;
			}
			int_Flag = 0;
			update_timeout = 0;
			msgs[0].len = buffer[34];
			ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
			#ifndef CLOCK_INTERRUPT
			ilitek_i2c_irq_enable();
			#endif
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, i2c write, failed\n", __func__);
				return -1;
			}
			break;
		default:
			return -1;
	}
    	return 0;
}

/*
description
	read function for character device driver
prarmeters
	filp
	    file pointer
	buf
	    buffer
	count
	    buffer length
	f_pos
	    offset
return
	status
*/
static ssize_t
ilitek_file_read(
        struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

/*
description
	close function
prarmeters
	inode
	    inode
	filp
	    file pointer
return
	status
*/
static int 
ilitek_file_close(
	struct inode *inode, struct file *filp)
{
	DBG("%s\n",__func__);
        return 0;
}

/*
description
	set input device's parameter
prarmeters
	input
		input device data
	max_tp
		single touch or multi touch
	max_x
		maximum	x value
	max_y
		maximum y value
return
	nothing
*/
static void 
ilitek_set_input_param(
	struct input_dev *input, 
	int max_tp, 
	int max_x, 
	int max_y)
{
	int key;
	input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	#ifndef ROTATE_180 //ROTATE_FLAG
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, max_x+2, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, max_y+2, 0, 0);
	#else
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, max_x+2, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, max_y+2, 0, 0);
	#endif
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, max_tp, 0, 0);
	set_bit(INPUT_PROP_DIRECT, input->propbit);
	for(key=0; key<sizeof(touch_key_code); key++){
		if(touch_key_code[key] <= 0){
			continue;
		}
		set_bit(touch_key_code[key] & KEY_MAX, input->keybit);
	}
	input->name = ILITEK_I2C_DRIVER_NAME;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &(i2c.client)->dev;
}

/*
description
	send message to i2c adaptor
parameter
	client
		i2c client
	msgs
		i2c message
	cnt
		i2c message count
return
	>= 0 if success
	others if error
*/
static int ilitek_i2c_transfer(struct i2c_client *client, struct i2c_msg *msgs, int cnt)
{
	int ret, count=ILITEK_I2C_RETRY_COUNT;
	while(count >= 0){
		count-= 1;
		ret = down_interruptible(&i2c.wr_sem);
		ret = i2c_transfer(client->adapter, msgs, cnt);
		up(&i2c.wr_sem);
		if(ret < 0){
			msleep(500);
			continue;
		}
		break;
	}
	return ret;
}

/*
description
	read data from i2c device
parameter
	client
		i2c client data
	addr
		i2c address
	data
		data for transmission
	length
		data length
return
	status
*/
static int 
ilitek_i2c_read(
	struct i2c_client *client,
	uint8_t cmd, 
	uint8_t *data, 
	int length)
{
	int ret;
    struct i2c_msg msgs[] = {
#ifdef MTK_I2C_DMA
		{
			.addr = client->addr & I2C_MASK_FLAG,
			.flags = 0,
			.len = 1,
			.buf = &cmd,
		},
		{
			.addr = client->addr & I2C_MASK_FLAG,
			.ext_flag = (client->addr | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
			.flags = I2C_M_RD,
			.len = length,
			.buf = gpDMABuf_pa,
		},
#else
		{.addr = client->addr, .flags = 0, .len = 1, .buf = &cmd,},
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,}
#endif
    };

    ret = ilitek_i2c_transfer(client, msgs, 2);
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret %d\n", __func__, ret);
	}

#ifdef MTK_I2C_DMA
	memcpy(data, gpDMABuf_va, length);
#endif
	return ret;
}

/*
description
	read data from i2c device
parameter
	client
		i2c client data
	addr
		i2c address
	data
		data for transmission
	length
		data length
return
	status
*/
static int 
ilitek_i2c_write(
	struct i2c_client *client,
	uint8_t cmd, 
	uint8_t *data, 
	int length)
{
	int ret;
    struct i2c_msg msgs[] = {
		{.addr = client->addr, .flags = 0, .len = 1, .buf = &cmd,},
    };

    ret = ilitek_i2c_transfer(client, msgs, 1);
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c write error, ret %d\n", __func__, ret);
	}
	return ret;
}
/*
description
	read data from i2c device
parameter
	client
		i2c client data
	addr
		i2c address
	data
		data for transmission
	length
		data length
return
	status
*/
static int 
ilitek_i2c_only_read(
	struct i2c_client *client,
	uint8_t *data, 
	int length)
{
	int ret;
    struct i2c_msg msgs[] = {
#ifdef MTK_I2C_DMA
		{
			.addr = client->addr & I2C_MASK_FLAG,
			.ext_flag = (client->addr | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
			.flags = I2C_M_RD,
			.len = length,
			.buf = gpDMABuf_pa,
		},
#else
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,}
#endif
    };

    ret = ilitek_i2c_transfer(client, msgs, 1);
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret %d\n", __func__, ret);
	}

#ifdef MTK_I2C_DMA
	memcpy(data, gpDMABuf_va, length);
#endif

	return ret;
}

/*
description
	process i2c data and then report to kernel
parameters
	none
return
	status
*/
static int ilitek_i2c_process_and_report(void)
{
#ifdef ROTATE_180 //ROTATE_FLAG
	int org_x = 0, org_y = 0;
#endif
	int i, len = 0, ret, x = 0, y = 0,key,mult_tp_id,packet = 0,tp_status = 0, j,buffer_flag[10] = {0}, release_flag[10]={0},cmd_flag=0;
#ifdef VIRTUAL_KEY_PAD
	unsigned char key_id = 0,key_flag= 1;
#endif
	static unsigned char last_id = 0;
	struct input_dev *input = i2c.input_dev;
    unsigned char buf[64]={0},touch_status[10]={0};
	unsigned char tp_id,max_point=6;
	unsigned char release_counter = 0;
	if(i2c.report_status == 0){
		return 1;
	} 
	
	//mutli-touch for protocol 3.1
	if((i2c.protocol_ver & 0x300) == 0x300){
		#ifdef TRANSFER_LIMIT
		ret = ilitek_i2c_read(i2c.client, ILITEK_TP_CMD_TOUCH_STATUS, buf, 2);
		cmd_flag = 1;
		//read touch information 
		for(i = 0; i < 8; i++){
			release_flag[i] = (buf[0] & (0x1 << i))>>i;
			}
		release_flag[8] = buf[1] & 0x1;	
		release_flag[9] = (buf[1] & 0x2) >> 1;	
		for(i = 0; i < 10; i++)
			DBG("release_flag[%d]=%d,buffer[%d]=%d",i,release_flag[i],i,buffer_flag[i]);
		DBG("\n");

		for(i = 0; i < 10; i++){
			if(release_flag[i] == 1 ){
				if(i<6)
					j = (1+i*5)/8;
				else
					j = ((1+i*5)+1)/8;
				DBG("i=%d,j=%d,cmd_flag=%d,command[%d]=%d\n",i,j,cmd_flag,i,buffer_flag[j]);
				if((((j+1)*8 > 2+i*5)||i==6) && buffer_flag[j] == 0){
					
					if(buffer_flag[j-1] == 1 || j==0){//
						cmd_flag = 1;
					}
					if(cmd_flag == 1){
						ret = ilitek_i2c_only_read(i2c.client, buf+(j*8), 8);
						buffer_flag[j] = 1;
					}
					else{
						ret = ilitek_i2c_write(i2c.client, ILITEK_TP_CMD_READ_DATA+j, buf+(j*8), 8);
						udelay(10);
						ret = ilitek_i2c_only_read(i2c.client, buf+(j*8), 8);
						buffer_flag[j] = 1;
						cmd_flag = 1;
					}
				}
				if(buffer_flag[j] == 0)
					cmd_flag = 0;
				j++;
				//msleep(1);
				if( (j*8 < 6+i*5) && buffer_flag[j] == 0){
					ret = ilitek_i2c_only_read(i2c.client, buf+(j*8), 8);
					buffer_flag[j] = 1;
					cmd_flag = 1;
				}
				max_point = i+1;
			}else
				 cmd_flag = 0;
		}
		//buf[31] is reserved so the data is moved forward.
		for(i = 31; i < 53; i++){
			buf[i] = buf[i+1];
			//printk("buf[%d]=0x%x\n",i,buf[i]);
		}
		packet = buf[0]+buf[1];
		#else
		ret = ilitek_i2c_read(i2c.client, ILITEK_TP_CMD_READ_DATA, buf, 31);
		if(ret < 0){
			return ret;
		}
		packet = buf[0];
		ret = 1;
		if (packet == 2){
			ret = ilitek_i2c_only_read(i2c.client, buf+31, 20);
			if(ret < 0){
				return ret;
			}
			max_point = 10;
		}
		#endif
		DBG("max_point=%d\n",max_point);
		// read touch point
		for(i = 0; i < max_point; i++){
			tp_status = buf[i*5+1] >> 7;	
			#ifndef ROTATE_180 //ROTATE_FLAG
			x = (((buf[i*5+1] & 0x3F) << 8) + buf[i*5+2]);
			y = (buf[i*5+3] << 8) + buf[i*5+4];
			#else
			org_x = (((buf[i*5+1] & 0x3F) << 8) + buf[i*5+2]);
			org_y = (buf[i*5+3] << 8) + buf[i*5+4];
			x = i2c.max_x - org_x + 2;
			y = i2c.max_y - org_y + 2;
			#endif
			if(tp_status){
				if(i2c.keyflag == 0){
					for(j = 0; j <= i2c.keycount; j++){
						if((x >= i2c.keyinfo[j].x && x <= i2c.keyinfo[j].x + i2c.key_xlen) && (y >= i2c.keyinfo[j].y && y <= i2c.keyinfo[j].y + i2c.key_ylen)){
							input_report_key(input,  i2c.keyinfo[j].id, 1);
							i2c.keyinfo[j].status = 1;
							touch_key_hold_press = 1;
							release_flag[0] = 1;
							DBG("Key, Keydown ID=%d, X=%d, Y=%d, key_status=%d,keyflag=%d\n", i2c.keyinfo[j].id ,x ,y , i2c.keyinfo[j].status,i2c.keyflag);
							break;
						}
					}
				}
				if(touch_key_hold_press == 0){
					input_report_key(i2c.input_dev, BTN_TOUCH,  1);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TRACKING_ID, i);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_X, x);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_Y, y);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
					input_mt_sync(i2c.input_dev);
					release_flag[i] = 1;
					i2c.keyflag = 1;
					DBG("Point, ID=%02X, X=%04d, Y=%04d,release_flag[%d]=%d,tp_status=%d,keyflag=%d\n",i, x,y,i,release_flag[i],tp_status,i2c.keyflag);	
				}
				if(touch_key_hold_press == 1){
					for(j = 0; j <= i2c.keycount; j++){
						if((i2c.keyinfo[j].status == 1) && (x < i2c.keyinfo[j].x || x > i2c.keyinfo[j].x + i2c.key_xlen || y < i2c.keyinfo[j].y || y > i2c.keyinfo[j].y + i2c.key_ylen)){
							input_report_key(input,  i2c.keyinfo[j].id, 0);
							i2c.keyinfo[j].status = 0;
							touch_key_hold_press = 0;
							DBG("Key, Keyout ID=%d, X=%d, Y=%d, key_status=%d\n", i2c.keyinfo[j].id ,x ,y , i2c.keyinfo[j].status);
							break;
						}
					}
				}
					
				ret = 0;
			}
			else{
				release_flag[i] = 0;
				DBG("Point, ID=%02X, X=%04d, Y=%04d,release_flag[%d]=%d,tp_status=%d\n",i, x,y,i,release_flag[i],tp_status);	
				input_mt_sync(i2c.input_dev);
			} 
				
		}
		if(packet == 0 ){
			i2c.keyflag = 0;
			input_report_key(i2c.input_dev, BTN_TOUCH,  0);
			input_mt_sync(i2c.input_dev);
		}
		else{
			for(i = 0; i < max_point; i++){
				if(release_flag[i] == 0)
					release_counter++;
			}
			if(release_counter == max_point ){
				input_report_key(i2c.input_dev, BTN_TOUCH,  0);
				input_mt_sync(i2c.input_dev);
				i2c.keyflag = 0;
				if (touch_key_hold_press == 1){
					for(i = 0; i < i2c.keycount; i++){
						if(i2c.keyinfo[i].status){
							input_report_key(input, i2c.keyinfo[i].id, 0);
							i2c.keyinfo[i].status = 0;
							touch_key_hold_press = 0;
							DBG("Key, Keyup ID=%d, X=%d, Y=%d, key_status=%d, touch_key_hold_press=%d\n", i2c.keyinfo[i].id ,x ,y , i2c.keyinfo[i].status, touch_key_hold_press);
						}
					}
				}
			}
			DBG("release_counter=%d,packet=%d\n",release_counter,packet);
		}
	}
	// multipoint process
	else if((i2c.protocol_ver & 0x200) == 0x200){
	    // read i2c data from device
		ret = ilitek_i2c_read(i2c.client, ILITEK_TP_CMD_READ_DATA, buf, 1);
		if(ret < 0){
			return ret;
		}
		len = buf[0];
		ret = 1;
		if(len>20)
			return ret;
		// read touch point
		for(i=0; i<len; i++){
			// parse point
			if(ilitek_i2c_write(i2c.client, ILITEK_TP_CMD_READ_SUB_DATA, buf, 5)){
				udelay(100);
				ilitek_i2c_only_read(i2c.client,buf,5);
			#ifndef ROTATE_180 //ROTATE_FLAG
				x = (((int)buf[1]) << 8) + buf[2];
				y = (((int)buf[3]) << 8) + buf[4];
			#else
				org_x = (((int)buf[1]) << 8) + buf[2];
				org_y = (((int)buf[3]) << 8) + buf[4];
				x = i2c.max_x - org_x + 2;
				y = i2c.max_y - org_y + 2;
			#endif
				mult_tp_id = buf[0];
				switch ((mult_tp_id & 0xC0)){
#ifdef VIRTUAL_KEY_PAD	
					case RELEASE_KEY:
						//release key
						DBG("Key: Release\n");
						for(key=0; key<sizeof(touch_key_code)/sizeof(touch_key_code[0]); key++){
							if(touch_key_press[key]){
								input_report_key(input, touch_key_code[key], 0);
								touch_key_press[key] = 0;
								DBG("Key:%d ID:%d release\n", touch_key_code[key], key);
								DBG(ILITEK_DEBUG_LEVEL "%s key release, %X, %d, %d\n", __func__, buf[0], x, y);
							}
							touch_key_hold_press=0;
							//ret = 1;// stop timer interrupt	
						}		

						break;
					
					case TOUCH_KEY:
						//touch key
						#if VIRTUAL_FUN==VIRTUAL_FUN_1
						key_id = buf[1] - 1;
						#endif	
						#if VIRTUAL_FUN==VIRTUAL_FUN_2
						if (abs(jiffies-touch_time) < msecs_to_jiffies(BTN_DELAY_TIME))
							break;
						//DBG("Key: Enter\n");
						x = (((int)buf[4]) << 8) + buf[3];
						
						//printk("%s,x=%d\n",__func__,x);
						if (x > KEYPAD01_X1 && x<KEYPAD01_X2)		// btn 1
							key_id=0;
						else if (x > KEYPAD02_X1 && x<KEYPAD02_X2)	// btn 2
							key_id=1;
						else if (x > KEYPAD03_X1 && x<KEYPAD03_X2)	// btn 3
							key_id=2;
						else if (x > KEYPAD04_X1 && x<KEYPAD04_X2)	// btn 4
							key_id=3;
						else 
							key_flag=0;
						#endif
						if((touch_key_press[key_id] == 0) && (touch_key_hold_press == 0 && key_flag)){
							input_report_key(input, touch_key_code[key_id], 1);
							touch_key_press[key_id] = 1;
							touch_key_hold_press = 1;
							DBG("Key:%d ID:%d press x=%d,touch_key_hold_press=%d,key_flag=%d\n", touch_key_code[key_id], key_id,x,touch_key_hold_press,key_flag);
						}
						break;					
#endif	
					case TOUCH_POINT:
	
#ifdef VIRTUAL_KEY_PAD		
						#if VIRTUAL_FUN==VIRTUAL_FUN_3
						if((buf[0] & 0x80) != 0 && ( y > KEYPAD_Y) && i==0){
							DBG("%s,touch key\n",__func__);
							if((x > KEYPAD01_X1) && (x < KEYPAD01_X2)){
								input_report_key(input,  touch_key_code[0], 1);
								touch_key_press[0] = 1;
								touch_key_hold_press = 1;
								DBG("%s,touch key=0 ,touch_key_hold_press=%d\n",__func__,touch_key_hold_press);
							}
							else if((x > KEYPAD02_X1) && (x < KEYPAD02_X2)){
								input_report_key(input, touch_key_code[1], 1);
								touch_key_press[1] = 1;
								touch_key_hold_press = 1;
								DBG("%s,touch key=1 ,touch_key_hold_press=%d\n",__func__,touch_key_hold_press);
							}
							else if((x > KEYPAD03_X1) && (x < KEYPAD03_X2)){
								input_report_key(input, touch_key_code[2], 1);
								touch_key_press[2] = 1;
								touch_key_hold_press = 1;
								DBG("%s,touch key=2 ,touch_key_hold_press=%d\n",__func__,touch_key_hold_press);
							}
							else {
								input_report_key(input, touch_key_code[3], 1);
								touch_key_press[3] = 1;
								touch_key_hold_press = 1;
								DBG("%s,touch key=3 ,touch_key_hold_press=%d\n",__func__,touch_key_hold_press);
							}
							
						}
						if((buf[0] & 0x80) != 0 && y <= KEYPAD_Y)
							touch_key_hold_press=0;
						if((buf[0] & 0x80) != 0 && y <= KEYPAD_Y)
						#endif
#endif
						{				
						// report to android system
						DBG("Point, ID=%02X, X=%04d, Y=%04d,touch_key_hold_press=%d\n",buf[0]  & 0x3F, x,y,touch_key_hold_press);	
						input_report_key(input, BTN_TOUCH,  1);
						input_event(input, EV_ABS, ABS_MT_TRACKING_ID, (buf[0] & 0x3F)-1);
						input_event(input, EV_ABS, ABS_MT_POSITION_X, x+1);
						input_event(input, EV_ABS, ABS_MT_POSITION_Y, y+1);
						input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
						input_mt_sync(input);
						ret=0;
						}
						break;
						
					case RELEASE_POINT:
						if (touch_key_hold_press !=0 && i==0){
							for(key=0; key<sizeof(touch_key_code)/sizeof(touch_key_code[0]); key++){
								if(touch_key_press[key]){
									input_report_key(input, touch_key_code[key], 0);
									touch_key_press[key] = 0;
									DBG("Key:%d ID:%d release\n", touch_key_code[key], key);
									DBG(ILITEK_DEBUG_LEVEL "%s key release, %X, %d, %d,touch_key_hold_press=%d\n", __func__, buf[0], x, y,touch_key_hold_press);
								}
								touch_key_hold_press=0;
								//ret = 1;// stop timer interrupt	
							}		
						}
						// release point
						#ifdef CLOCK_INTERRUPT
						release_counter++;
						if (release_counter == len){
							input_report_key(input, BTN_TOUCH,  0);
							input_mt_sync(input);
						}
						#endif
						//ret=1;				
						break;
						
					default:
						break;
				}
			}
		}
		// release point
		if(len == 0){
			DBG("Release3, ID=%02X, X=%04d, Y=%04d\n",buf[0]  & 0x3F, x,y);
			input_report_key(input, BTN_TOUCH,  0);
			//input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, 0);
			input_mt_sync(input);
			//ret = 1;
			if (touch_key_hold_press !=0){
				for(key=0; key<sizeof(touch_key_code)/sizeof(touch_key_code[0]); key++){
					if(touch_key_press[key]){
						input_report_key(input, touch_key_code[key], 0);
						touch_key_press[key] = 0;
						DBG("Key:%d ID:%d release\n", touch_key_code[key], key);
						DBG(ILITEK_DEBUG_LEVEL "%s key release, %X, %d, %d\n", __func__, buf[0], x, y);
					}
					touch_key_hold_press=0;
					//ret = 1;// stop timer interrupt	
				}		
			}
		}
		DBG("%s,ret=%d\n",__func__,ret);
	}
	
	else{
	    // read i2c data from device
		ret = ilitek_i2c_read(i2c.client, ILITEK_TP_CMD_READ_DATA, buf, 9);
		if(ret < 0){
			return ret;
		}
		if(buf[0] > 20){
			ret = 1;
			return ret ;
		}
		// parse point
		ret = 0;
		
		
		tp_id = buf[0];
		if (Report_Flag!=0){
			printk("%s(%d):",__func__,__LINE__);
			for (i=0;i<9;i++)
				DBG("%02X,",buf[i]);
			DBG("\n");
		}
		switch (tp_id)
		{
			case 0://release point
#ifdef VIRTUAL_KEY_PAD				
				if (touch_key_hold_press !=0)
				{
					for(key=0; key<sizeof(touch_key_code)/sizeof(touch_key_code[0]); key++){
						if(touch_key_press[key]){
							//input_report_key(input, touch_key_code[key], 0);
							touch_key_press[key] = 0;
							DBG("Key:%d ID:%d release\n", touch_key_code[key], key);
						}
					}
					touch_key_hold_press = 0;
				}
				else
#endif
				{
					for(i=0; i<i2c.max_tp; i++){
						// check 
						if (!(last_id & (1<<i)))
							continue;	
							
						#ifndef ROTATE_180 //ROTATE_FLAG
						x = (int)buf[1 + (i * 4)] + ((int)buf[2 + (i * 4)] * 256);
						y = (int)buf[3 + (i * 4)] + ((int)buf[4 + (i * 4)] * 256);
						#else
						org_x = (int)buf[1 + (i * 4)] + ((int)buf[2 + (i * 4)] * 256);
						org_y = (int)buf[3 + (i * 4)] + ((int)buf[4 + (i * 4)] * 256);
						x = i2c.max_x - org_x + 2;
						y = i2c.max_y - org_y + 2;
						#endif
						touch_key_hold_press=2; //2: into available area
						input_report_key(input, BTN_TOUCH,  1);
						input_event(i2c.input_dev, EV_ABS, ABS_MT_TRACKING_ID, i);
						input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_X, x+1);
						input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_Y, y+1);
						input_event(i2c.input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
						input_mt_sync(i2c.input_dev);
						DBG("Last Point[%d]= %d, %d\n", buf[0]&0x3F, x, y);
						last_id=0;
					}
					input_sync(i2c.input_dev);
					input_report_key(input, BTN_TOUCH,  0);		
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 0);
					input_mt_sync(i2c.input_dev);
					ret = 1; // stop timer interrupt
				}
				break;
#ifdef VIRTUAL_KEY_PAD				
			case 0x81:
				if (abs(jiffies-touch_time) < msecs_to_jiffies(BTN_DELAY_TIME))
					break;
				DBG("Key: Enter\n");
	
				#if VIRTUAL_FUN==VIRTUAL_FUN_1
				key_id = buf[1] - 1;
				#endif
				
				#if VIRTUAL_FUN==VIRTUAL_FUN_2
				x = (int)buf[1] + ((int)buf[2] * 256);
				if (x > KEYPAD01_X1 && x<KEYPAD01_X2)		// btn 1
					key_id=0;
				else if (x > KEYPAD02_X1 && x<KEYPAD02_X2)	// btn 2
					key_id=1;
				else if (x > KEYPAD03_X1 && x<KEYPAD03_X2)	// btn 3
					key_id=2;
				else if (x > KEYPAD04_X1 && x<KEYPAD04_X2)	// btn 4
					key_id=3;
				else 
					key_flag=0;			
				#endif
				input_report_abs(input, ABS_MT_TOUCH_MAJOR, 0);
    				input_mt_sync(input);
				if((touch_key_press[key_id] == 0) && (touch_key_hold_press == 0 && key_flag)){
					input_report_key(input, touch_key_code[key_id], 1);
					touch_key_press[key_id] = 1;
					touch_key_hold_press = 1;
					DBG("Key:%d ID:%d press\n", touch_key_code[key_id], key_id);
				}			
				break;
			case 0x80:
				DBG("Key: Release\n");
				for(key=0; key<sizeof(touch_key_code)/sizeof(touch_key_code[0]); key++){
					if(touch_key_press[key]){
						input_report_key(input, touch_key_code[key], 0);
						touch_key_press[key] = 0;
						DBG("Key:%d ID:%d release\n", touch_key_code[key], key);
                   	}
				}		
				touch_key_hold_press=0;
				ret = 1;// stop timer interrupt	
				break;
#endif
			default:
				last_id=buf[0];
				for(i=0; i<i2c.max_tp; i++){
					// check 
					if (!(buf[0] & (1<<i)))
						continue;	
						
					#ifndef ROTATE_180 //ROTATE_FLAG
					x = (int)buf[1 + (i * 4)] + ((int)buf[2 + (i * 4)] * 256);
					y = (int)buf[3 + (i * 4)] + ((int)buf[4 + (i * 4)] * 256);
					#else
					org_x = (int)buf[1 + (i * 4)] + ((int)buf[2 + (i * 4)] * 256);
					org_y = (int)buf[3 + (i * 4)] + ((int)buf[4 + (i * 4)] * 256);
					x = i2c.max_x - org_x + 2;
					y = i2c.max_y - org_y + 2;
					#endif
#ifdef VIRTUAL_KEY_PAD						
					#if VIRTUAL_FUN==VIRTUAL_FUN_3
					if (y > KEYPAD_Y){
						if (abs(jiffies-touch_time) < msecs_to_jiffies(BTN_DELAY_TIME))
							break;									
						x = (int)buf[1] + ((int)buf[2] * 256);
						if (x > KEYPAD01_X1 && x<KEYPAD01_X2)		// btn 1
							key_id=0;
						else if (x > KEYPAD02_X1 && x<KEYPAD02_X2)	// btn 2
							key_id=1;
						else if (x > KEYPAD03_X1 && x<KEYPAD03_X2)	// btn 3
							key_id=2;
						else if (x > KEYPAD04_X1 && x < KEYPAD04_X2)	// btn 4
							key_id=3;
						else 
							key_flag=0;			
						if (touch_key_hold_press==2){
							input_report_key(input, BTN_TOUCH,  0);
							input_event(i2c.input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 0);
							input_mt_sync(i2c.input_dev);
							touch_key_hold_press=0;
						}
						if((touch_key_press[key_id] == 0) && (touch_key_hold_press == 0 && key_flag)){
							//input_report_key(input, touch_key_code[key_id], 1);
							touch_key_press[key_id] = 1;
							touch_key_hold_press = 1;
							DBG("Key:%d ID:%d press\n", touch_key_code[key_id], key_id);					
						}
					}
					else if (touch_key_hold_press){
						for(key=0; key<sizeof(touch_key_code)/sizeof(touch_key_code[0]) ; key++){
							if(touch_key_press[key]){
								//input_report_key(input, touch_key_code[key], 0);
								touch_key_press[key] = 0;
								DBG("Key:%d ID:%d release\n", touch_key_code[key], key);
							}
						}
						touch_key_hold_press = 0;
					}
					else
					#endif
					touch_time=jiffies + msecs_to_jiffies(BTN_DELAY_TIME);
#endif					
					{
						touch_key_hold_press=2; //2: into available area
						input_report_key(input, BTN_TOUCH,  1);
						input_event(i2c.input_dev, EV_ABS, ABS_MT_TRACKING_ID, i);
						input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_X, x+1);
						input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_Y, y+1);
						input_event(i2c.input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
						input_mt_sync(i2c.input_dev);
						DBG("Point[%d]= %d, %d\n", buf[0]&0x3F, x, y);
					}
					
				}
				break;
		}
	}
	input_sync(i2c.input_dev);
    return ret;
}

static void ilitek_i2c_timer(unsigned long handle)
{
    struct i2c_data *priv = (void *)handle;

    schedule_work(&priv->irq_work);
}
/*
description
	work queue function for irq use
parameter
	work
		work queue
return
	nothing
*/
static void 
ilitek_i2c_irq_work_queue_func(
	struct work_struct *work)
{
	int ret;
	struct i2c_data *priv =  
		container_of(work, struct i2c_data, irq_work);
	ret = ilitek_i2c_process_and_report();
	DBG("%s,enter\n",__func__);
#ifdef CLOCK_INTERRUPT
	ilitek_i2c_irq_enable();
#else
    if (ret == 0){
		if (!i2c.stop_polling)
			mod_timer(&priv->timer, jiffies + msecs_to_jiffies(0));
	}
    else if (ret == 1){
		if (!i2c.stop_polling){
			ilitek_i2c_irq_enable();
		}
		DBG("stop_polling\n");
	}
	else if(ret < 0){
		msleep(100);
		DBG(ILITEK_ERROR_LEVEL "%s, process error\n", __func__);
		ilitek_i2c_irq_enable();
    }	
#endif
}

/*
description
	i2c interrupt service routine
parameters
	irq
		interrupt number
	dev_id
		device parameter
return
	return status
*/

#ifdef MTK_PLATFORM
static void 
ilitek_i2c_isr(
	void)
#else
static irqreturn_t 
	int irq, void *dev_id)
#endif /* MTK_PLATFORM */
{
	#ifndef CLOCK_INTERRUPT
		if(i2c.irq_status == 1 ){
#ifdef MTK_PLATFORM
			mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#else
			disable_irq_nosync(i2c.client->irq);
#endif /* MTK_PLATFORM */
			DBG("disable nosync\n");
			i2c.irq_status = 0;
		}
	#endif
	if(update_Flag == 1){
		int_Flag = 1;
	}
	else{
		queue_work(i2c.irq_work_queue, &i2c.irq_work);
	}
	return IRQ_HANDLED;
}

/*
description
        i2c polling thread
parameters
        arg
			arguments
return
        return status
*/
static int 
ilitek_i2c_polling_thread(
	void *arg)
{

	int ret=0;
	// check input parameter
	DBG(ILITEK_DEBUG_LEVEL "%s, enter\n", __func__);

	// mainloop
	while(1){
		// check whether we should exit or not
		if(kthread_should_stop()){
			printk(ILITEK_DEBUG_LEVEL "%s, stop\n", __func__);
			break;
		}

		// this delay will influence the CPU usage and response latency
		msleep(10);
		
		// when i2c is in suspend or shutdown mode, we do nothing
		if(i2c.stop_polling){
			msleep(1000);
			continue;
		}

		// read i2c data
		if(ilitek_i2c_process_and_report() < 0){
			msleep(3000);
			printk(ILITEK_ERROR_LEVEL "%s, process error\n", __func__);
		}
	}
	return ret;
}

/*
description
	i2c early suspend function
parameters
	h
	early suspend pointer
return
	nothing
*/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ilitek_i2c_early_suspend(struct early_suspend *h)
{
	ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND);
	printk("%s\n", __func__);
}
#endif

/*
description
	i2c later resume function
parameters
	h
	early suspend pointer
return
	nothing
*/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ilitek_i2c_late_resume(struct early_suspend *h)
{
	ilitek_i2c_resume(i2c.client);
	printk("%s\n", __func__);
}
#endif
/*
description
        i2c irq enable function
*/
static void ilitek_i2c_irq_enable(void)
{
	if (i2c.irq_status == 0){
		i2c.irq_status = 1;
#ifdef MTK_PLATFORM
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#else
		enable_irq(i2c.client->irq);
#endif /* MTK_PLATFORM */
		DBG("enable\n");
		
	}
	else
		DBG("no enable\n");
}
/*
description
        i2c irq disable function
*/
static void ilitek_i2c_irq_disable(void)
{
	if (i2c.irq_status == 1){
		i2c.irq_status = 0;
#ifdef MTK_PLATFORM
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#else
		disable_irq(i2c.client->irq);
#endif /* MTK_PLATFORM */
		DBG("disable\n");
	}
	else
		DBG("no disable\n");
}

/*
description
        i2c suspend function
parameters
        client
		i2c client data
	mesg
		suspend data
return
        return status
*/

static int 
ilitek_i2c_suspend(
	struct i2c_client *client, pm_message_t mesg)
{
	if(i2c.valid_irq_request != 0){
		ilitek_i2c_irq_disable();
	}
	else{
		i2c.stop_polling = 1;
		printk(ILITEK_DEBUG_LEVEL "%s, stop i2c thread polling\n", __func__);
  	}

	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	udelay(50);
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
#endif

	return 0;
}

/*
description
        i2c resume function
parameters
        client
		i2c client data
return
        return status
*/
static int ilitek_i2c_resume(struct i2c_client *client)
{
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, TPD_POWER_SOURCE_VOL, "TP");
#endif
	udelay(50);
	ilitek_i2c_reset();

        if(i2c.valid_irq_request != 0){
			ilitek_i2c_irq_enable();
        }
	else{
		i2c.stop_polling = 0;
        	printk(ILITEK_DEBUG_LEVEL "%s, start i2c thread polling\n", __func__);
	}
	return 0;
}

/*
description
	reset touch ic 
prarmeters
	reset_pin
	    reset pin
return
	status
*/
static int ilitek_i2c_reset(void)
{
	int ret = 0;
	#ifndef SET_RESET
	static unsigned char buffer[64]={0};
	struct i2c_msg msgs[] = {
		{.addr = i2c.client->addr, .flags = 0, .len = 1, .buf = buffer,}
    };
	buffer[0] = 0x60;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	#else
	/*
	
	____         ___________
		|_______|
		   1ms      100ms
	*/
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	mdelay(10);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	mdelay(1);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	#endif
	msleep(100);
	return ret; 
}

/*
description
        i2c shutdown function
parameters
        client
                i2c client data
return
        nothing
*/
static void
ilitek_i2c_shutdown(
        struct i2c_client *client)
{
	printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
	i2c.stop_polling = 1;
}

static int inwrite(unsigned int address) {
	char outbuff[64];
	int data, ret;
	outbuff[0] = 0x25;
	outbuff[1] = (char)((address & 0x000000FF) >> 0);
	outbuff[2] = (char)((address & 0x0000FF00) >> 8);
	outbuff[3] = (char)((address & 0x00FF0000) >> 16);
	ret = ilitek_i2c_only_write(i2c.client, outbuff, 4);
	ret = ilitek_i2c_only_read(i2c.client, outbuff, 4);
	data = (outbuff[0] + outbuff[1] * 256 + outbuff[2] * 256 * 256 + outbuff[3] * 256 * 256 * 256);
	printk("%s, data=0x%x, outbuff[0]=%x, outbuff[1]=%x, outbuff[2]=%x, outbuff[3]=%x\n", __func__, data, outbuff[0], outbuff[1], outbuff[2], outbuff[3]);
	return data;
}

static int outwrite(unsigned int address, unsigned int data) {
	int ret;
	char outbuff[64];
	outbuff[0] = 0x25;
	outbuff[1] = (char)((address & 0x000000FF) >> 0);
	outbuff[2] = (char)((address & 0x0000FF00) >> 8);
	outbuff[3] = (char)((address & 0x00FF0000) >> 16);
	outbuff[4] = (char)((data & 0x000000FF) >> 0);
	outbuff[5] = (char)((data & 0x0000FF00) >> 8);
	outbuff[6] = (char)((data & 0x00FF0000) >> 16);
	outbuff[7] = (char)((data & 0xFF000000) >> 24);
	ret = ilitek_i2c_only_write(i2c.client, outbuff, 8);
	return 0;   
}

static void clear_program_key(void) {
	int ret;
	char buf[64];
	buf[0] = 0X25;
	buf[1] = 0X14;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0x00;
	buf[5] = 0x00;
	ret = ilitek_i2c_only_write(i2c.client, buf, 6);
}

static void set_standby_key(unsigned int chip_id_H, unsigned int chip_id_L) {
	int ret;
	char buf[64];
	//Set StandBy Key
	buf[0] = 0x25;
	buf[1] = 0x10;
	buf[2] = 0x00;
	buf[3] = 0x04;
	buf[4] = chip_id_L;
	buf[5] = chip_id_H;
	ret = ilitek_i2c_only_write(i2c.client, buf, 6);
}

static void mtp_control_reg(int data) {
	char buf[64];
	int i, ret;
	buf[0] = 0x0;
	for(i = 0; i < 500; i++) {
		if(buf[0] == 0) {
			buf[0] = 0x25;
			buf[1] = data;
			buf[2] = 0x10;
			buf[3] = 0x04;
			ret = ilitek_i2c_only_write(i2c.client, buf, 4);

			ret = ilitek_i2c_only_read(i2c.client, buf, 4);

			msleep(1);
		} else {
			break;
		}
	}

	if(i == 500) {
		printk("%s,mtp_control_reg\n",__func__);
	} else {
		outwrite(0x41030, 0x00000000);
	}
}

static void set_standby(int data) {
	int ret;
	char buf[64];
	buf[0] = 0x25;
	buf[1] = 0x24;
	buf[2] = 0x00;
	buf[3] = 0x04;
	buf[4] = 0x01;
	ret = ilitek_i2c_only_write(i2c.client, buf, 5);
	mtp_control_reg(data);
}

/*
description
	hex to dec
prarmeter
	hex
		hex value
	len
		translation size
return
	dec. value
*/
static unsigned long jni_hex_2_dec(char *hex, int len) {
	unsigned long ret = 0, temp = 0;
	int i, shift = (len - 1) * 4;
	for(i = 0; i < len; shift -= 4, i++) {
		if((hex[i] >= '0') && (hex[i] <= '9')) {
			temp = hex[i] - '0';
		} else if((hex[i] >= 'a') && (hex[i] <= 'z')) {
			temp = (hex[i] - 'a') + 10;
		} else {
			temp = (hex[i] - 'A') + 10;
		}
		ret |= (temp << shift);
	}
	return ret;
}
   
static int ilitek_upgrade_firmware(u32 size)
{
	int ret = 0, upgrade_status = 0, i, j, k = 0, ap_len = 0, df_len = 0, ic_model = 0, ucCount = 0;
	unsigned char buf[128] = {0}, chip_id_H = 0, chip_id_L = 0;
	unsigned long ice_checksum = 0, temp = 0;
	unsigned char firmware_ver[4];
	unsigned int  bl_ver = 0, flow_flag = 0, uiData, usStart, usEnd, usSize;
	struct i2c_msg msgs[] = {
		{.addr = i2c.client->addr, .flags = 0, .len = 0, .buf = buf,}
	};
	unsigned long addr, exaddr = 0;
	unsigned long start_addr = 0xFFFF, ap_startaddr = 0xFFFF, df_startaddr = 0xFFFF;
	unsigned long end_addr = 0, ap_endaddr = 0, df_endaddr = 0;
	unsigned long len, type, checksum = 0, check = 0, ap_checksum = 0, df_checksum = 0;
	int test = 0;

	for(i = 0; i < size; ) {
		int offset;
		len = jni_hex_2_dec(&pbt_buf[i + 1], 2);
		addr = jni_hex_2_dec(&pbt_buf[i + 3], 4);
		type = jni_hex_2_dec(&pbt_buf[i + 7], 2);
		test = 0;

		//calculate checksum
		checksum = 0;
		for(j = 8; j < (2 + 4 + 2 + (len * 2)); j += 2) {
			if(type == 0x00) {
				check = check + jni_hex_2_dec(&pbt_buf[i + 1 + j], 2);
				if(addr + (j - 8) / 2 < df_startaddr) {
					ap_checksum = ap_checksum + jni_hex_2_dec(&pbt_buf[i + 1 + j], 2);
					//printf("addr = 0x%04X, ap_check = 0x%06X, type = 0x%02X\n", addr + (j - 8) / 2 , ap_check, jni_hex_2_dec(&pbuf[i + 1 + j], 2));
				} else {
					df_checksum = df_checksum + jni_hex_2_dec(&pbt_buf[i + 1 + j], 2);
					//printf("addr = 0x%04X, df_check = 0x%06X, type = 0x%02X\n", addr + (j - 8)/2 , df_check, jni_hex_2_dec(&pbuf[i + 1 + j], 2));
				}
			} else {
				checksum = 0;
			}
		}
		//printk("addr = 0x%4x,check = 0x%4x,total check = 0x%6x\n",addr,test,check);
		if(type == 0x04) {
			exaddr = jni_hex_2_dec(&pbt_buf[i + 9], 4);
		}
		addr = addr + (exaddr << 16);
		if(pbt_buf[i + 1 + j + 2] == 0x0D) {
			offset = 2;
		} else {
			offset = 1;
		}
		if(addr < df_startaddr) {
			ap_checksum = ap_checksum + checksum;
		} else {
			df_checksum = df_checksum + checksum;
		}
		
		if(type == 0x00) {
			if(addr > 0x40000) {
				ret = ILITEK_ERR_INVALID_HEX_FILE_FORMAT;
				printk("%s, invalid hex format\n", __func__);
				return ret;
			}
			if(addr < start_addr) {
				start_addr = addr;
			}
			if((addr + len) > end_addr) {
				end_addr = addr + len;
			}
			if(addr < ap_startaddr) {
				ap_startaddr = addr;
			}
			if((addr + len) > ap_endaddr && (addr < df_startaddr)) {
				ap_endaddr = addr + len - 1;
				if(ap_endaddr > df_startaddr) {
					ap_endaddr = df_startaddr - 1;
				}
			}
			if((addr + len) > df_endaddr && (addr >= df_startaddr)) {
				df_endaddr = addr + len;
				//printk("df_end_addr = 0x%X\n", df_end_addr);
			}

			//fill data
			for(j = 0, k = 0; j < (len * 2); j += 2, k++) {
				CTPM_FW[addr + k] = jni_hex_2_dec(&pbt_buf[i + 9 + j], 2);
			}
		}
		i += 1 + 2 + 4 + 2 + (len * 2) + 2 + offset;
	}

	//ap_startaddr = ( CTPM_FW[0] << 16 ) + ( CTPM_FW[1] << 8 ) + CTPM_FW[2];
	//ap_endaddr = ( CTPM_FW[3] << 16 ) + ( CTPM_FW[4] << 8 ) + CTPM_FW[5];
	//ap_checksum = ( CTPM_FW[6] << 16 ) + ( CTPM_FW[7] << 8 ) + CTPM_FW[8];
	//firmware_ver[0] = CTPM_FW[18];
	//firmware_ver[1] = CTPM_FW[19];
	//firmware_ver[2] = CTPM_FW[20];
	//firmware_ver[3] = CTPM_FW[21];
	//printk("start=0x%x,end=0x%x,checksum=0x%x\n",ap_startaddr,ap_endaddr,ap_checksum);
	//ilitek_i2c_reset();
	msleep(300);
	buf[0] = 0xF2;
	buf[1] = 0x01;
	msgs[0].len = 2;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	msleep(100);
	//check system busy
	for(i = 0; i < 50 ; i++) {
		buf[0] = 0x80;
		msgs[0].len = 1;
		ilitek_i2c_read(i2c.client, buf[0], buf, 5);
		msleep(50);
		if(buf[0] == 0x50) {
			break;
		}
	}
	if(buf[0] != 0x50 &&( buf[0] != 0x0 && buf[0] != 0xFF)) {
		printk("[Ilitek] SYSTEM IS BUSY!!!\n");
		return ERROR_SYSTEM_BUSY;
	}
	buf[0] = 0x61;
	msgs[0].len = 1;
	ret = ilitek_i2c_read(i2c.client, buf[0], buf, 5);
	if(ret < 0)
		return ERROR_I2C;
	if(buf[0]==0x07 && buf[1] == 0x00) {
		chip_id_H = 0x21;
		chip_id_L = 0x15;
		ic_model = (chip_id_H << 8) + chip_id_L;
		printk("IC is 0x%X\n", ic_model);
	}
	if(buf[0]==0x08 && buf[1] == 0x00) {
		chip_id_H = 0x21;
		chip_id_L = 0x16;
		ic_model = (chip_id_H << 8) + chip_id_L;
		printk("IC is 0x%X\n", ic_model);
	}
	if((buf[0]==0xFF && buf[1] == 0xFF) || (buf[0]==0x00 && buf[1] == 0x00)) {
		printk("IC is NULL\n");
	}

	buf[0] = 0x25;
	buf[1] = 0x62;
	buf[2] = 0x10;
	buf[3] = 0x18;
	msgs[0].len = 4;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	if(ret < 0)
		return ERROR_I2C;
	buf[0] = 0x25;
	buf[1] = 0x9b;
	buf[2] = 0x00;
	buf[3] = 0x04;
	msgs[0].len = 4;
	ret = ilitek_i2c_only_write(i2c.client, buf, 4);
	ret = ilitek_i2c_only_read(i2c.client, buf, 1);
	
	if(buf[0] == 0x01 || buf[0] == 0x02 || buf[0] == 0x03 || buf[0] == 0x04 || buf[0] == 0x11 || buf[0] == 0x12 || buf[0] == 0x13 || buf[0] == 0x14) {
		printk("%s, ic is 2115\n", __func__);
		chip_id_H = 0x21;
		chip_id_L = 0x15;
		ic_model = (chip_id_H << 8) + chip_id_L;
	}
	if(buf[0] == 0x81 || buf[0] == 0x82 || buf[0] == 0x83 || buf[0] == 0x84 || buf[0] == 0x91 || buf[0] == 0x92 || buf[0] == 0x93 || buf[0] == 0x94) {
		printk("%s, ic is 2116\n", __func__);
		chip_id_H = 0x21;
		chip_id_L = 0x16;
		ic_model = (chip_id_H << 8) + chip_id_L;
	}
	printk("ic=0x%04x\n", ic_model);
	
	buf[0] = 0x25;
	buf[1] = 0x00;
	buf[2] = 0x20;
	buf[3] = 0x04;
	buf[4] = 0x27;
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	
	buf[0] = 0x25;
	buf[1] = 0x10;
	buf[2] = 0x00;
	buf[3] = 0x04;
	buf[4] = chip_id_L;
	buf[5] = chip_id_H;
	msgs[0].len = 6;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	
	uiData = inwrite(0x42008);
	
	buf[0] = 0x25;
	buf[1] = 0x08;
	buf[2] = 0x20;
	buf[3] = 0x04;
	buf[4] = ((unsigned char)((uiData & 0x000000FF) >> 0));
	buf[5] = ((unsigned char)((uiData & 0x0000FF00) >> 8));
	buf[6] = ((unsigned char)((uiData & 0x00FF0000) >> 16));
	buf[7] = ((unsigned char)((uiData & 0xFF000000) >> 24) | 4);
	msgs[0].len = 8;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	
	buf[0] = 0x25;
	buf[1] = 0x24;
	buf[2] = 0x00;
	buf[3] = 0x04;
	buf[4] = 0x01;
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	
	buf[0] = 0x25;
	buf[1] = 0x00;
	buf[2] = 0x20;
	buf[3] = 0x04;
	buf[4] = 0x00;	 
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	clear_program_key();
	
	set_standby_key(chip_id_H, chip_id_L);
	//3.set preprogram
	//3-1 set program key
	buf[0] = 0X25;
	buf[1] = 0X14;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0xAE;
	buf[5] = 0x7E;
	msgs[0].len = 6;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	//3-2 set standby key
	set_standby_key(chip_id_H, chip_id_L);
	//
	buf[0] = 0x25;
	buf[1] = 0x00;
	buf[2] = 0x20;
	buf[3] = 0x04;
	buf[4] = 0x27;
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	//Setting program start address and size (0x41018)
	usEnd = usEnd / 16;
	usSize = usEnd - usStart;
	printk("usSize=0x%x",usSize);
	buf[0] = 0X25;
	buf[1] = 0X18;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = (char)((0x0000 & 0x00FF) >> 0);
	buf[5] = (char)((0x0000 & 0xFF00) >> 8);
	buf[6] = 0xEF;
	buf[7] = 0x07;
	msgs[0].len = 8;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	//d.Setting pre-program data (0x4101c)
	buf[0] = 0X25;
	buf[1] = 0X1C;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0xFF;
	buf[5] = 0xFF;
	buf[6] = 0xFF;
	buf[7] = 0xFF;
	msgs[0].len = 8;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	//e.Enable pre-program (0x41032)
	buf[0] = 0X25;
	buf[1] = 0X32;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0XB6;
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	//f.Enable standby (0x40024)
	//g.Wait program time (260us*size) or check MTP_busy (0x4102c)
	set_standby(0x32);
	//h.Clear program key (0x41014)
	clear_program_key();
	//4. Set chip erase
	for(i = 0; i < 3; i++) {
		buf[0] = 0X25;
		buf[1] = 0X24;
		buf[2] = 0X10;
		buf[3] = 0X04;
		buf[4] = (char)((0x7FFF & 0x00FF) >> 0);
		buf[5] = (char)((0x7FFF & 0xFF00) >> 8);
		msgs[0].len = 6;
		ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
		printk("%s, a.Setting lock page (0x41024), jni_write_data, ret = %d\n", __func__, ret);
		temp = inwrite(0x41024);
		printk("temp = 0x%08x\n", temp);
		if((temp | 0x00FF) == 0x7FFF) {
			break;
		}
	}
	//b.Setting chip erase key (0x41010)
	buf[0] = 0X25;
	buf[1] = 0X10;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0XAE;
	buf[5] = 0XCE;
	buf[6] = 0X00;
	buf[7] = 0X00;
	msgs[0].len = 8;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, b.Setting chip erase key (0x41010), jni_write_data, ret = %d\n", __func__, ret);
	//c.Setting standby key (0x40010)
	printk("%s, c.Setting standby key (0x40010)\n", __func__);
	set_standby_key(chip_id_H,chip_id_L);

	//d.Enable chip erase (0x41030)
	buf[0] = 0X25;
	buf[1] = 0X30;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0X7E;
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, d.Enable chip erase (0x41030), jni_write_data, ret = %d\n", __func__, ret);

	//e.Enable standby (0x40024)
	//f.Wait chip erase (94ms) or check MTP_busy (0x4102c)
	printk("%s, Enable standby\n", __func__);
	set_standby(0x30);

	//g.Clear lock page (0x41024)
	buf[0] = 0X25;
	buf[1] = 0X24;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0xFF;
	buf[5] = 0xFF;
	msgs[0].len = 6;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, g.Clear lock page (0x41024), jni_write_data, ret = %d\n", __func__, ret);

	//Set sector erase
	for(i = 0x78; i <= 0x7E; i++) {
	//a.Setting sector erase key (0x41012)
	buf[0] = 0X25;
	buf[1] = 0X12;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0X12;
	buf[5] = 0XA5;
	ret = ilitek_i2c_only_write(i2c.client, buf, 6);
	printk("%s, a.Setting sector erase key (0x41012), jni_write_data, ret = %d\n", __func__, ret);
	//b.Setting standby key (0x40010)
	set_standby_key(chip_id_H, chip_id_L);

	//c.Setting sector number (0x41018)
	buf[0] = 0X25;
	buf[1] = 0X18;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = (char)((i & 0x00FF) >> 0);
	buf[5] = (char)((i & 0xFF00) >> 8);
	buf[6] = 0x00;
	buf[7] = 0x00;
	ret = ilitek_i2c_only_write(i2c.client, buf, 8);
	printk("%s, c.Setting sector number (0x41018), jni_write_data, ret = %d\n", __func__, ret);

	//d.Enable sector erase (0x41031)
	buf[0] = 0X25;
	buf[1] = 0X31;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0XA5;
	ret = ilitek_i2c_only_write(i2c.client, buf, 5);
	printk("%s, d.Enable sector erase (0x41031), jni_write_data, ret = %d\n", __func__, ret);

    //e.Enable standby (0x40024)
	//f.Wait chip erase (94ms) or check MTP_busy (0x4102c)
	set_standby(0x31);
	}
	
	if(((ap_endaddr + 1) % 4) != 0) {
		ap_endaddr += 4;
	}
	buf[0] = 0X25;
	buf[1] = 0X14;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0xAE;
	buf[5] = 0x7E;
	msgs[0].len = 6;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, jni_write_data, ret = %d", __func__, ret);
	j = 0;

	for(i = ap_startaddr; i < ap_endaddr; i += 16) {
		buf[0] = 0x25;
		buf[3] = (char)((i  & 0x00FF0000) >> 16);
		buf[2] = (char)((i  & 0x0000FF00) >> 8);
		buf[1] = (char)((i  & 0x000000FF));
		for(k = 0; k < 16; k++) {
			buf[4 + k] = CTPM_FW[i + k];
		}
		//msgs[0].len = 20;
		ret = ilitek_i2c_only_write_dma(i2c.client, buf, 20);
		
		upgrade_status = ((i * 100)) / ap_endaddr;
		if(upgrade_status > j){
			printk("%cILITEK: Firmware Upgrade(AP), %02d%c. ", 0x0D, upgrade_status, '%');
			j = j+10;
		}
	}
	printk("\nILITEK:%s, upgrade firmware completed\n", __func__);
	buf[0] = 0X25;
	buf[1] = 0X14;
	buf[2] = 0X10;
	buf[3] = 0X04;
	buf[4] = 0x00;
	buf[5] = 0x00;
	msgs[0].len = 6;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, jni_write_data, ret = %d", __func__, ret);
	buf[0] = 0x25;
	buf[1] = 0x9b;
	buf[2] = 0x00;
	buf[3] = 0x04;
	ilitek_i2c_only_write(i2c.client, buf, 4);
	ilitek_i2c_only_read(i2c.client, buf, 1);

	//check 2115 or 2116 again
	if(buf[0] == 0x01 || buf[0] == 0x02 || buf[0] == 0x03 || buf[0] == 0x04 || buf[0] == 0x11 || buf[0] == 0x12 || buf[0] == 0x13 || buf[0] == 0x14) {
		printk("%s, ic is 2115\n", __func__);
		chip_id_H = 0x21;
		chip_id_L = 0x15;
		ic_model = (chip_id_H << 8) + chip_id_L;
	}
	if(buf[0] == 0x81 || buf[0] == 0x82 || buf[0] == 0x83 || buf[0] == 0x84 || buf[0] == 0x91 || buf[0] == 0x92 || buf[0] == 0x93 || buf[0] == 0x94) {
		printk("%s, ic is 2116\n", __func__);
		chip_id_H = 0x21;
		chip_id_L = 0x16;
		ic_model = (chip_id_H << 8) + chip_id_L;
	}
	set_standby_key(chip_id_H, chip_id_L);
	//get checksum start and end address
	buf[0] = 0x25;
	buf[1] = 0x20;
	buf[2] = 0x10;
	buf[3] = 0x04;
	buf[4] = (char)((ap_startaddr & 0x00FF) >> 0);
	buf[5] = (char)((ap_startaddr & 0xFF00) >> 8);
	buf[6] = (char)(((ap_endaddr) & 0x00FF) >> 0);
	buf[7] = (char)(((ap_endaddr ) & 0xFF00) >> 8);
	msgs[0].len = 8;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, jni_write_data, ret = %d", __func__, ret);

	buf[0] = 0x25;
	buf[1] = 0x38;
	buf[2] = 0x10;
	buf[3] = 0x04;
	buf[4] = 0x01;
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, jni_write_data, ret = %d", __func__, ret);

	buf[0] = 0x25;
	buf[1] = 0x33;
	buf[2] = 0x10;
	buf[3] = 0x04;
	buf[4] = 0xCD;
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, jni_write_data, ret = %d", __func__, ret);

	buf[0] = 0x25;
	buf[1] = 0x24;
	buf[2] = 0x00;
	buf[3] = 0x04;
	buf[4] = 0x01;
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, jni_write_data, ret = %d", __func__, ret);

	for(i = 0; i < 500; i++) {
		ilitek_i2c_only_write(i2c.client, buf, 8);
		ilitek_i2c_only_read(i2c.client, buf, 1);
		if(buf[0] == 0x01) {
			break;
		} else {
			msleep(1);
		}
	}
	if(i == 500) {
		printk("%s,update finished ,please reboot\n");
	}

	//Get checksum
	ice_checksum = inwrite(0x41028);
	if(ap_checksum != ice_checksum) {
		printk("upgrade Fail, hex checksum = 0x%6x, ic checksum = 0x%6x\n", ap_checksum, ice_checksum);
	} else {
		printk("checksum equal, hex checksum = 0x%6x, ic checksum = 0x%6x\n", ap_checksum, ice_checksum);
	}

	//RESET CPU
	buf[0] = 0x25;
	buf[1] = 0x40;
	buf[2] = 0x00;
	buf[3] = 0x04;
	buf[4] = 0xAE;
	msgs[0].len = 5;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, jni_write_data, ret = %d\n", __func__, ret);
	msleep(100);
	////1.Software Reset
	//1-1 Clear Reset Done Flag
	outwrite(0x41048, 0x00000000);
	//1-2 Set CDC/Core Reset
	outwrite(0x41040, 0xAE003F03);
	for(i = 0; i < 5000 ;i++){
		msleep(1);
		if((inwrite(0x040048)&0x00010000) == 0x00010000)
			break;
	}
	//Exit ICE
	buf[0] = 0x1B;
	buf[1] = 0x62;
	buf[2] = 0x10;
	buf[3] = 0x18;
	msgs[0].len = 4;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);	
	printk("%s, jni_write_data, ret = %d\n", __func__, ret);
	msleep(1000);

	if(ap_checksum == ice_checksum) {
		for(i = 0; i < 50 ; i++) {
			buf[0] = 0x80;
			msgs[0].len = 1;
			ilitek_i2c_read(i2c.client, buf[0], buf, 1);
			msleep(50);
			if(buf[0] == 0x50){
				printk("System is not busy,i=%d\n", i);
				break;
			}
		}
	}
	if(i == 50 && buf[0] != 0x50){
		printk("update pass please reboot,buf=0x%x\n", buf[0]);
		ilitek_i2c_reset();
	}
}

static int 
ilitek_i2c_only_write(struct i2c_client *client,
                      uint8_t *data, int length)
{
	int ret;

    struct i2c_msg msgs[] = {
		{.addr = client->addr, .flags = !I2C_M_RD, .len = length, .buf = data,}
    };

    ret = ilitek_i2c_transfer(client, msgs, 1);

	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c write error, ret %d\n", __func__, ret);
	}

	return ret;
}

static int
ilitek_i2c_only_write_dma(struct i2c_client *client,
                          uint8_t *data, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
#ifdef MTK_I2C_DMA
		{
			.addr = client->addr & I2C_MASK_FLAG,
			.ext_flag = (client->addr | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
			.flags = !I2C_M_RD,
			.len = length,
			.buf = gpDMABuf_pa,
		},
#else
		{.addr = client->addr, .flags = !I2C_M_RD, .len = length, .buf = data,}
#endif
	};

#ifdef MTK_I2C_DMA
	memcpy(gpDMABuf_va, data, length);
#endif
	ret = ilitek_i2c_transfer(client, msgs, 1);

	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c write error, ret %d\n", __func__, ret);
	}

	return ret;
}

static int GetFirmwareSize(char *firmware_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	char filepath[128];
	memset(filepath, 0, sizeof(filepath));

	sprintf(filepath, "%s", firmware_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}

static int ReadFirmware(char *firmware_name, unsigned char *firmware_buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s", firmware_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, firmware_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}


int fw_upgrade_with_file(char *firmware_name)
{
	int i_ret;
	int fwsize = GetFirmwareSize(firmware_name);

	printk("FW size = 0x%x\n", fwsize);

	if (fwsize <= 0) {
		printk(ILITEK_DEBUG_LEVEL "%s ERROR:Get firmware size failed\n",
		__func__);
		return -EIO;
	}

	/*=========FW upgrade========================*/
	pbt_buf = kmalloc(fwsize + 1, GFP_ATOMIC);

	if (ReadFirmware(firmware_name, pbt_buf)) {
		printk(ILITEK_DEBUG_LEVEL "%s() - ERROR: request_firmware failed\n", __func__);
		kfree(pbt_buf);
		return -EIO;
	}

	CTPM_FW = kmalloc(fwsize + 1, GFP_ATOMIC);
	memset(CTPM_FW, 0, (fwsize + 1) * sizeof(unsigned long));
	
	i_ret = ilitek_upgrade_firmware(fwsize);
	
	kfree(pbt_buf);
	kfree(CTPM_FW);

	return i_ret;
}

static ssize_t tpd_tpfwver_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
	ssize_t num_read_chars = 0;
	unsigned char buffer[64]={0};
	
	if(ilitek_i2c_read_info(i2c.client, ILITEK_TP_CMD_GET_FIRMWARE_VERSION, buffer, 4) < 0) {
		return -1;
	}
	printk("%s, firmware version %d.%d.%d.%d\n", __func__, buffer[0], buffer[1], buffer[2], buffer[3]);
	
	return num_read_chars = snprintf(buf, PAGE_SIZE, "%d%d%d%d\n", buffer[0], buffer[1], buffer[2], buffer[3]);
}

static ssize_t tpd_tpfwver_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count)
{
	/*place holder for future use*/
	return -EPERM;
}

static ssize_t tpd_fwupgrade_show(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}

static ssize_t tpd_fwupgrade_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
	char fwname[128];
	int ret = -1;
	unsigned long val = 0;

	memset(fwname, 0, sizeof(fwname));

	ret = kstrtoul(buf, 10, &val);
	if (ret) {
		/* Using customer input filename */
		sprintf(fwname, "%s", buf);
		fwname[count - 1] = '\0';
	} else {
		/* There is no mean for 0x571,
		 * just avoiding update firmware with accident.
		 */
		//if (val == 0x571) {
			strcat(fwname, FW_FILE_PATH);
			strcat(fwname, FW_FILE_NAME);
		//}
	}
	printk("%s: fwname = %s\n", __func__, fwname);

	fw_upgrade_with_file(fwname);

	return count;
}

/*sysfs */
static DEVICE_ATTR(fw_ver, S_IRUGO | S_IWUSR, tpd_tpfwver_show, tpd_tpfwver_store);
static DEVICE_ATTR(fw_upgrade_app, S_IRUGO | S_IWUSR, tpd_fwupgrade_show, tpd_fwupgrade_store);

/*add your attr in here*/
static struct attribute *tpd_attributes[] = {
	&dev_attr_fw_ver.attr,
	&dev_attr_fw_upgrade_app,
	NULL
};

static struct attribute_group tpd_attribute_group = {
	.attrs = tpd_attributes
};

/*
description
	when adapter detects the i2c device, this function will be invoked.
parameters
	client
		i2c client data
	id
		i2c data
return
	status
*/
static int 
#ifdef MTK_PLATFORM
ilitek_tpd_i2c_probe(
#else
ilitek_i2c_probe(
#endif /* MTK_PLATFORM */
	struct i2c_client *client, 
	const struct i2c_device_id *id)
{
	// register i2c device
	int ret = 0; 

#ifdef MTK_PLATFORM

	// power on
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, TPD_POWER_SOURCE_VOL, "TP");
#endif
	udelay(500);

	// gpio setting
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
   	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	ilitek_i2c_reset();

	// set INT mode
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

#endif /* MTK_PLATFORM */

	// allocate character device driver buffer
	ret = alloc_chrdev_region(&dev.devno, 0, 1, ILITEK_FILE_DRIVER_NAME);
	if(ret){
		printk(ILITEK_ERROR_LEVEL "%s, can't allocate chrdev\n", __func__);
	return ret;
	}
	printk(ILITEK_DEBUG_LEVEL "%s, register chrdev(%d, %d)\n", __func__, MAJOR(dev.devno), MINOR(dev.devno));
	
	// initialize character device driver
	cdev_init(&dev.cdev, &ilitek_fops);
	dev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev.cdev, dev.devno, 1);
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, add character device error, ret %d\n", __func__, ret);
	return ret;
	}
	dev.class = class_create(THIS_MODULE, ILITEK_FILE_DRIVER_NAME);
	if(IS_ERR(dev.class)){
		printk(ILITEK_ERROR_LEVEL "%s, create class, error\n", __func__);
	return ret;
	}
	device_create(dev.class, NULL, dev.devno, NULL, "ilitek_ctrl");
	Report_Flag = 0;
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
		printk(ILITEK_ERROR_LEVEL "%s, I2C_FUNC_I2C not support\n", __func__);
		return -1;
	}
	i2c.client = client;
	printk(ILITEK_DEBUG_LEVEL "%s, i2c new style format\n", __func__);
	printk("%s, IRQ: 0x%X\n", __func__, client->irq);
	
	ilitek_i2c_register_device();
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, register i2c device, error\n", __func__);
		return ret;
	}
	
#ifdef SYSFS_DEBUG
	sysfs_create_group(&client->dev.kobj, &tpd_attribute_group);
#endif

	tpd_load_status = 1;

	return 0;
}

/*
description
	when the i2c device want to detach from adapter, this function will be invoked.
parameters
	client
		i2c client data
return
	status
*/
static int 
#ifdef MTK_PLATFORM
ilitek_tpd_i2c_remove(
#else
ilitek_i2c_remove(
#endif /* MTK_PLATFORM */
	struct i2c_client *client)
{
	printk( "%s\n", __func__);
	i2c.stop_polling = 1;
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&i2c.early_suspend);
#endif
	// delete i2c driver
	if(i2c.client->irq != 0){
		if(i2c.valid_irq_request != 0){
			free_irq(i2c.client->irq, &i2c);
			printk(ILITEK_DEBUG_LEVEL "%s, free irq\n", __func__);
			if(i2c.irq_work_queue){
				destroy_workqueue(i2c.irq_work_queue);
				printk(ILITEK_DEBUG_LEVEL "%s, destory work queue\n", __func__);
			}
		}
	}
	else{
		if(i2c.thread != NULL){
			kthread_stop(i2c.thread);
			printk(ILITEK_DEBUG_LEVEL "%s, stop i2c thread\n", __func__);
		}
	}
	if(i2c.valid_input_register != 0){
		input_unregister_device(i2c.input_dev);
		printk(ILITEK_DEBUG_LEVEL "%s, unregister i2c input device\n", __func__);
	}
        
	// delete character device driver
	cdev_del(&dev.cdev);
	unregister_chrdev_region(dev.devno, 1);
	device_destroy(dev.class, dev.devno);
	class_destroy(dev.class);
	printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
	return 0;
}

/*
description
	read data from i2c device with delay between cmd & return data
parameter
	client
		i2c client data
	addr
		i2c address
	data
		data for transmission
	length
		data length
return
	status
*/
static int 
ilitek_i2c_read_info(
	struct i2c_client *client,
	uint8_t cmd, 
	uint8_t *data, 
	int length)
{
	int ret;
	struct i2c_msg msgs_cmd[] = {
#ifdef MTK_I2C_DMA
		{
			.addr = client->addr & I2C_MASK_FLAG,
			.flags = 0,
			.len = 1,
			.buf = &cmd,
		},
#else
	{.addr = client->addr, .flags = 0, .len = 1, .buf = &cmd,},
#endif
	};
	
	struct i2c_msg msgs_ret[] = {
#ifdef MTK_I2C_DMA
		{
			.addr = client->addr & I2C_MASK_FLAG,
			.ext_flag = (client->addr | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
			.flags = I2C_M_RD,
			.len = length,
			.buf = gpDMABuf_pa,
		},
#else
	{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,}
#endif
	};

	ret = ilitek_i2c_transfer(client, msgs_cmd, 1);
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret %d\n", __func__, ret);
	}
	
	msleep(10);
	ret = ilitek_i2c_transfer(client, msgs_ret, 1);
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret %d\n", __func__, ret);		
	}

#ifdef MTK_I2C_DMA
	memcpy(data, gpDMABuf_va, length);
#endif

	return ret;
}

/*
description
	read touch information
parameters
	none
return
	status
*/
static int
ilitek_i2c_read_tp_info(
	void)
{
	int res_len, i,j;
	unsigned char buf[64]={0};
	
	// read driver version
	printk(ILITEK_DEBUG_LEVEL "%s, Driver Version:%d.%d.%d\n",__func__,driver_information[0],driver_information[1],driver_information[2]);
	printk(ILITEK_DEBUG_LEVEL "%s, customer information:%d.%d.%d.%d\n",__func__,driver_information[3],driver_information[4],driver_information[5],driver_information[6]);
	printk(ILITEK_DEBUG_LEVEL "%s, Engineer id:%d\n",__func__,driver_information[6]);
	// read firmware version
	if(ilitek_i2c_read_info(i2c.client, ILITEK_TP_CMD_GET_FIRMWARE_VERSION, buf, 4) < 0){
		return -1;
	}
	printk(ILITEK_DEBUG_LEVEL "%s, firmware version %d.%d.%d.%d\n", __func__, buf[0], buf[1], buf[2], buf[3]);

	// read protocol version
	res_len = 6;
	if(ilitek_i2c_read_info(i2c.client, ILITEK_TP_CMD_GET_PROTOCOL_VERSION, buf, 2) < 0){
		return -1;
	}	
	i2c.protocol_ver = (((int)buf[0]) << 8) + buf[1];
	printk(ILITEK_DEBUG_LEVEL "%s, protocol version: %d.%d\n", __func__, buf[0], buf[1]);
	//if(i2c.protocol_ver == 0x200){
	//	res_len = 8;
	//}
	//else if(i2c.protocol_ver == 0x300){
		res_len = 10;
	//}

    // read touch resolution
	i2c.max_tp = 2;
	#ifdef TRANSFER_LIMIT
 	if(ilitek_i2c_read_info(i2c.client, ILITEK_TP_CMD_GET_RESOLUTION, buf, 8) < 0){
		return -1;
	}   
	if(ilitek_i2c_only_read(i2c.client, buf+8, 2) < 0){
		return -1;
	}
	#else
	if(ilitek_i2c_read_info(i2c.client, ILITEK_TP_CMD_GET_RESOLUTION, buf, res_len) < 0){
		return -1;
	}
	#endif
	
	//if(i2c.protocol_ver == 0x200){
	//	// maximum touch point
	//	i2c.max_tp = buf[6];
	//	// maximum button number
	//	i2c.max_btn = buf[7];
	//}
	//else if(i2c.protocol_ver & 0x300) == 0x300){
		// maximum touch point
		i2c.max_tp = buf[6];
		// maximum button number
		i2c.max_btn = buf[7];
		// key count
		i2c.keycount = buf[8];
	//}
	
	// calculate the resolution for x and y direction
	i2c.max_x = buf[0];
	i2c.max_x+= ((int)buf[1]) * 256;
	i2c.max_y = buf[2];
	i2c.max_y+= ((int)buf[3]) * 256;
	i2c.x_ch = buf[4];
	i2c.y_ch = buf[5];
	printk(ILITEK_DEBUG_LEVEL "%s, max_x: %d, max_y: %d, ch_x: %d, ch_y: %d\n", 
	__func__, i2c.max_x, i2c.max_y, i2c.x_ch, i2c.y_ch);
	
	if(i2c.protocol_ver == 0x200){
		printk(ILITEK_DEBUG_LEVEL "%s, max_tp: %d, max_btn: %d\n", __func__, i2c.max_tp, i2c.max_btn);
	}
	else if((i2c.protocol_ver & 0x300) == 0x300){
		printk(ILITEK_DEBUG_LEVEL "%s, max_tp: %d, max_btn: %d, key_count: %d\n", __func__, i2c.max_tp, i2c.max_btn, i2c.keycount);
		//get key infotmation		
		#ifdef TRANSFER_LIMIT
		if(ilitek_i2c_read(i2c.client, ILITEK_TP_CMD_GET_KEY_INFORMATION, buf, 8) < 0){
			return -1;
		}
		for(i = 1, j = 1; j < i2c.keycount ; i++){
			if (i2c.keycount > j){
				if(ilitek_i2c_only_read(i2c.client, buf+i*8, 8) < 0){
					return -1;
				}
				j = (4+8*i)/5;
			}
		}
		for(j = 29; j < (i+1)*8; j++)
			buf[j] = buf[j+3];
		#else
		if(i2c.keycount){
			if(ilitek_i2c_read(i2c.client, ILITEK_TP_CMD_GET_KEY_INFORMATION, buf, 29) < 0){
				return -1;
			}
			if (i2c.keycount > 5){
				if(ilitek_i2c_only_read(i2c.client, buf+29, 25) < 0){
					return -1;
				}
			}
		}
		#endif
			i2c.key_xlen = (buf[0] << 8) + buf[1];
			i2c.key_ylen = (buf[2] << 8) + buf[3];
			printk(ILITEK_DEBUG_LEVEL "%s, key_xlen: %d, key_ylen: %d\n", __func__, i2c.key_xlen, i2c.key_ylen);
			
			//print key information
			for(i = 0; i < i2c.keycount; i++){
				i2c.keyinfo[i].id = buf[i*5+4];	
				i2c.keyinfo[i].x = (buf[i*5+5] << 8) + buf[i*5+6];
				i2c.keyinfo[i].y = (buf[i*5+7] << 8) + buf[i*5+8];
				i2c.keyinfo[i].status = 0;
				printk(ILITEK_DEBUG_LEVEL "%s, key_id: %d, key_x: %d, key_y: %d, key_status: %d\n", __func__, i2c.keyinfo[i].id, i2c.keyinfo[i].x, i2c.keyinfo[i].y, i2c.keyinfo[i].status);
			}
		
	}
	
	return 0;
}

/*
description
	register i2c device and its input device
parameters
	none
return
	status
*/
static int 
ilitek_i2c_register_device(
	void)
{
	int ret = 0;
	printk(ILITEK_DEBUG_LEVEL "%s, client.addr: 0x%X\n", __func__, (unsigned int)i2c.client->addr);
	printk(ILITEK_DEBUG_LEVEL "%s, client.adapter: 0x%X\n", __func__, (unsigned int)i2c.client->adapter);
	printk(ILITEK_DEBUG_LEVEL "%s, client.driver: 0x%X\n", __func__, (unsigned int)i2c.client->driver);
	if((i2c.client->addr == 0) || (i2c.client->adapter == 0) || (i2c.client->driver == 0)){
		printk(ILITEK_ERROR_LEVEL "%s, invalid register\n", __func__);
		return ret;
	}
	// read touch parameter
	ilitek_i2c_read_tp_info();
	// register input device
	i2c.input_dev = input_allocate_device();
	if(i2c.input_dev == NULL){
		printk(ILITEK_ERROR_LEVEL "%s, allocate input device, error\n", __func__);
		return -1;
	}
	ilitek_set_input_param(i2c.input_dev, i2c.max_tp, i2c.max_x, i2c.max_y);
	ret = input_register_device(i2c.input_dev);
	if(ret){
		printk(ILITEK_ERROR_LEVEL "%s, register input device, error\n", __func__);
		return ret;
	}
	printk(ILITEK_ERROR_LEVEL "%s, register input device, success\n", __func__);
	i2c.valid_input_register = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
	i2c.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	i2c.early_suspend.suspend = ilitek_i2c_early_suspend;
	i2c.early_suspend.resume = ilitek_i2c_late_resume;
	register_early_suspend(&i2c.early_suspend);
#endif

#ifdef MTK_PLATFORM
	/*
	   Sakia Lien add 2014-12-01
	   - In the MTK platform, there is no registry of I2C's IRQ
	 */
	i2c.client->irq = 1;
#endif /* MTK_PLATFORM */

	if(i2c.client->irq != 0 ){ 
		i2c.irq_work_queue = create_singlethread_workqueue("ilitek_i2c_irq_queue");
		if(i2c.irq_work_queue){
			INIT_WORK(&i2c.irq_work, ilitek_i2c_irq_work_queue_func);
			#ifdef CLOCK_INTERRUPT
#ifdef MTK_PLATFORM
			mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_FALLING, ilitek_i2c_isr, 1);
			mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#else
			if(request_irq(i2c.client->irq, ilitek_i2c_isr, IRQF_TRIGGER_FALLING , "ilitek_i2c_irq", &i2c)){
				printk(ILITEK_ERROR_LEVEL "%s, request irq, error\n", __func__);
			}
			else{
				i2c.valid_irq_request = 1;
				i2c.irq_status = 1;
				printk(ILITEK_ERROR_LEVEL "%s, request irq(Trigger Falling, success\n", __func__);
			}				
#endif /* MTK_PLATFORM */				
			#else
			init_timer(&i2c.timer);
			i2c.timer.data = (unsigned long)&i2c;
			i2c.timer.function = ilitek_i2c_timer;
			if(request_irq(i2c.client->irq, ilitek_i2c_isr, IRQF_TRIGGER_LOW, "ilitek_i2c_irq", &i2c)){
				printk(ILITEK_ERROR_LEVEL "%s, request irq, error\n", __func__);
			}
			else{
				i2c.valid_irq_request = 1;
				i2c.irq_status = 1;
				printk(ILITEK_ERROR_LEVEL "%s, request irq(Trigger Low), success\n", __func__);
			}
			#endif
		}
	}
	else{
		i2c.stop_polling = 0;
		i2c.thread = kthread_create(ilitek_i2c_polling_thread, NULL, "ilitek_i2c_thread");
		printk(ILITEK_ERROR_LEVEL "%s, polling mode \n", __func__);
		if(i2c.thread == (struct task_struct*)ERR_PTR){
			i2c.thread = NULL;
			printk(ILITEK_ERROR_LEVEL "%s, kthread create, error\n", __func__);
		}
		else{
			wake_up_process(i2c.thread);
		}
	}
	
	return 0;
}

/*
description
	initiali function for driver to invoke.
parameters

	nothing
return
	status
*/
static int __init
#ifdef MTK_PLATFORM
ilitek_tpd_local_init(
#else
ilitek_init(
#endif /* MTK_PLATFORM */
	void)
{
	int ret = -1;
	// initialize global variable
	memset(&dev, 0, sizeof(struct dev_data));
	memset(&i2c, 0, sizeof(struct i2c_data));

	// initialize mutex object
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)	
	init_MUTEX(&i2c.wr_sem);
#else
	sema_init(&i2c.wr_sem,1);
#endif
	i2c.wr_sem.count = 1;
	i2c.report_status = 1;

#ifdef MTK_I2C_DMA
	gpDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 64, &gpDMABuf_pa, GFP_KERNEL);
	if(!gpDMABuf_va){
		DBG("[Error] Allocate DMA I2C Buffer failed!\n");
	}
#endif

#ifdef MTK_PLATFORM
	ret = i2c_add_driver(&ilitek_tpd_i2c_driver);
#else
	ret = i2c_add_driver(&ilitek_i2c_driver);
#endif /* MTK_PLATFORM */
	if(ret == 0){
		i2c.valid_i2c_register = 1;
		printk(ILITEK_DEBUG_LEVEL "%s, add i2c device, success\n", __func__);
		if(i2c.client == NULL){
			printk(ILITEK_ERROR_LEVEL "%s, no i2c board information\n", __func__);
		} else {
			ret = 1;
		}
	}
	else{
		printk(ILITEK_ERROR_LEVEL "%s, add i2c device, error\n", __func__);
	}

	return ret;
}

/*
description
	driver exit function
parameters
	none
return
	nothing
*/
static void __exit
ilitek_exit(
	void)
{
	printk("%s,enter\n",__func__);
	if(i2c.valid_i2c_register != 0){
		printk(ILITEK_DEBUG_LEVEL "%s, delete i2c driver\n", __func__);
#ifdef MTK_PLATFORM
		i2c_del_driver(&ilitek_tpd_i2c_driver);
#else
		i2c_del_driver(&ilitek_i2c_driver);
#endif /* MTK_PLATFORM */
		printk(ILITEK_DEBUG_LEVEL "%s, delete i2c driver\n", __func__);
    }
	else
		printk(ILITEK_DEBUG_LEVEL "%s, delete i2c driver Fail\n", __func__);
}

#ifdef MTK_PLATFORM
static struct tpd_driver_t ilitek_tpd_device_driver = {
                .tpd_device_name = "ili2116",
                .tpd_local_init = ilitek_tpd_local_init,
#ifdef CONFIG_HAS_EARLYSUSPEND    
                .suspend = ilitek_i2c_early_suspend,
                .resume = ilitek_i2c_late_resume,
#endif

#ifdef TPD_HAVE_BUTTON
                .tpd_have_button = 1,
#else
                .tpd_have_button = 0,
#endif
};

/* called when loaded into kernel */
static int __init ilitek_tpd_driver_init(void)
{
    printk("MediaTek ili2116 touch panel driver init\n");
    i2c_register_board_info(TPD_I2C_NUMBER, &ilitek_i2c_tpd, 1);
    if(tpd_driver_add(&ilitek_tpd_device_driver) < 0)
            TPD_DMESG("add generic driver failed\n");
            
    return 0;
}

/* should never be called */
static void __exit ilitek_tpd_driver_exit(void) {
    TPD_DMESG("MediaTek ili2116 touch panel driver exit\n");
	ilitek_exit();
    //input_unregister_device(tpd->dev);
    tpd_driver_remove(&ilitek_tpd_device_driver);
}

/* set init and exit function for this module */
module_init(ilitek_tpd_driver_init);
module_exit(ilitek_tpd_driver_exit);
#else
/* set init and exit function for this module */
module_init(ilitek_init);
module_exit(ilitek_exit);
#endif /* MTK_PLATFORM */


