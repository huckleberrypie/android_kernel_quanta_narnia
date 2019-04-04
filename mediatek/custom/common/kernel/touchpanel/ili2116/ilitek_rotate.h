#define MTK_PLATFORM
#ifdef MTK_PLATFORM
#define MTK_I2C_DMA
#endif

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include <linux/wait.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#ifndef MTK_PLATFORM
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#endif /* ! MTK_PLATFORM */
#endif

#ifdef MTK_PLATFORM
#include <linux/gpio.h>
#include <cust_eint.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include "tpd.h"
#include "cust_gpio_usage.h"

#define TPD_I2C_NUMBER  1     // i2c bus number
#define TPD_I2C_ADDR    0x41  // i2c slave address
#if defined(QCI_PROJECT_UYT)
#define TPD_POWER_SOURCE_CUSTOM		MT6323_POWER_LDO_VGP1   // define your power source for tp if needed
#define TPD_POWER_SOURCE_VOL			VOL_3300
#endif
#endif /* MTK_PLATFORM */

#ifdef MTK_I2C_DMA
#include <linux/dma-mapping.h>
#endif

// sysfs option
#define SYSFS_DEBUG

#define FW_FILE_PATH				"/system/etc/firmware/"
#define FW_FILE_NAME				"ili2116_fw.hex"

// driver information
#define DERVER_VERSION_MAJOR 		2
#define DERVER_VERSION_MINOR 		3
#define RELEASE_VERSION				0
#define CUSTOMER_ID 				0
#define MODULE_ID					0
#define PLATFORM_ID					0
#define PLATFORM_MODULE				0
#define ENGINEER_ID					511	

//#define VIRTUAL_KEY_PAD
#define VIRTUAL_FUN_1	1	//0X81 with key_id
#define VIRTUAL_FUN_2	2	//0x81 with x position
#define VIRTUAL_FUN_3	3	//Judge x & y position
//#define VIRTUAL_FUN		VIRTUAL_FUN_2
#define BTN_DELAY_TIME	500 //ms

#define TOUCH_POINT    0x80
#define TOUCH_KEY      0xC0
#define RELEASE_KEY    0x40
#define RELEASE_POINT    0x00
//#define ROTATE_FLAG
#define ROTATE_180
#ifndef MTK_I2C_DMA
#define TRANSFER_LIMIT /* i2c transfer limit control */
#endif
#define CLOCK_INTERRUPT /* interrupt control */
#define SET_RESET

// define key pad range
#define KEYPAD01_X1	0
#define KEYPAD01_X2	1000
#define KEYPAD02_X1	1000
#define KEYPAD02_X2	2000
#define KEYPAD03_X1	2000
#define KEYPAD03_X2	3000
#define KEYPAD04_X1	3000
#define KEYPAD04_X2	3968
#define KEYPAD_Y	2100
// definitions
#define ILITEK_I2C_RETRY_COUNT			3
#define ILITEK_I2C_DRIVER_NAME			"ilitek_i2c"
#define ILITEK_FILE_DRIVER_NAME			"ilitek_file"
#define ILITEK_DEBUG_LEVEL			KERN_INFO
#define ILITEK_ERROR_LEVEL			KERN_ALERT

// i2c command for ilitek touch screen
#define ILITEK_TP_CMD_READ_DATA			0x10
#define ILITEK_TP_CMD_READ_SUB_DATA		0x11
#define ILITEK_TP_CMD_GET_RESOLUTION		0x20
#define ILITEK_TP_CMD_GET_KEY_INFORMATION	0x22
#define ILITEK_TP_CMD_GET_FIRMWARE_VERSION	0x40
#define ILITEK_TP_CMD_GET_PROTOCOL_VERSION	0x42
#define	ILITEK_TP_CMD_CALIBRATION			0xCC
#define	ILITEK_TP_CMD_CALIBRATION_STATUS	0xCD
#define ILITEK_TP_CMD_ERASE_BACKGROUND		0xCE

// i2c command for Protocol 3.1
#define ILITEK_TP_CMD_TOUCH_STATUS			0x0F

// define the application command
#define ILITEK_IOCTL_BASE                       100
#define ILITEK_IOCTL_I2C_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 0, unsigned char*)
#define ILITEK_IOCTL_I2C_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 2, unsigned char*)
#define ILITEK_IOCTL_I2C_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 3, int)
#define ILITEK_IOCTL_USB_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 4, unsigned char*)
#define ILITEK_IOCTL_USB_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 5, int)
#define ILITEK_IOCTL_USB_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 6, unsigned char*)
#define ILITEK_IOCTL_USB_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 7, int)
#define ILITEK_IOCTL_DRIVER_INFORMATION		    _IOWR(ILITEK_IOCTL_BASE, 8, int)
#define ILITEK_IOCTL_USB_UPDATE_RESOLUTION      _IOWR(ILITEK_IOCTL_BASE, 9, int)
#define ILITEK_IOCTL_I2C_INT_FLAG	            _IOWR(ILITEK_IOCTL_BASE, 10, int)
#define ILITEK_IOCTL_I2C_UPDATE                 _IOWR(ILITEK_IOCTL_BASE, 11, int)
#define ILITEK_IOCTL_STOP_READ_DATA             _IOWR(ILITEK_IOCTL_BASE, 12, int)
#define ILITEK_IOCTL_START_READ_DATA            _IOWR(ILITEK_IOCTL_BASE, 13, int)
#define ILITEK_IOCTL_GET_INTERFANCE				_IOWR(ILITEK_IOCTL_BASE, 14, int)//default setting is i2c interface
#define ILITEK_IOCTL_I2C_SWITCH_IRQ				_IOWR(ILITEK_IOCTL_BASE, 15, int)
#define ILITEK_IOCTL_UPDATE_FLAG				_IOWR(ILITEK_IOCTL_BASE, 16, int)
#define ILITEK_IOCTL_I2C_UPDATE_FW				_IOWR(ILITEK_IOCTL_BASE, 18, int)
#define ERROR_SYSTEM_BUSY						-1
#define ERROR_I2C								-2
#define DBG(fmt, args...)   if (DBG_FLAG)printk("%s(%d): " fmt, __func__,__LINE__,  ## args)

//Error flag
#define ILITEK_ERR_INVALID_HEX_FILE_FORMAT		-1

// module information
MODULE_AUTHOR("Steward_Fu");
MODULE_DESCRIPTION("ILITEK I2C touch driver for Android platform");
MODULE_LICENSE("GPL");

// all implemented global functions must be defined in here 
// in order to know how many function we had implemented
static int ilitek_i2c_register_device(void);
static void ilitek_set_input_param(struct input_dev*, int, int, int);
static int ilitek_i2c_read_tp_info(void);
#ifndef MTK_PLATFORM
static int ilitek_init(void);
static void ilitek_exit(void);
#endif /* ! MTK_PLATFORM */

// i2c functions
static int ilitek_i2c_transfer(struct i2c_client*, struct i2c_msg*, int);
static int ilitek_i2c_read(struct i2c_client*, uint8_t, uint8_t*, int);
static int ilitek_i2c_read_info(struct i2c_client *client, uint8_t cmd, uint8_t *data, int length);
static int ilitek_i2c_process_and_report(void);
static int ilitek_i2c_suspend(struct i2c_client*, pm_message_t);
static int ilitek_i2c_resume(struct i2c_client*);
static void ilitek_i2c_shutdown(struct i2c_client*);
static int ilitek_i2c_only_write(struct i2c_client *client, uint8_t *data, int length);
static int ilitek_i2c_only_write_dma(struct i2c_client *client, uint8_t *data, int length);
#ifndef MTK_PLATFORM
static int ilitek_i2c_probe(struct i2c_client*, const struct i2c_device_id*);
static int ilitek_i2c_remove(struct i2c_client*);
#endif /* ! MTK_PLATFORM */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ilitek_i2c_early_suspend(struct early_suspend *h);
static void ilitek_i2c_late_resume(struct early_suspend *h);
#endif
static int ilitek_i2c_polling_thread(void*);

#ifdef MTK_PLATFORM
static void ilitek_i2c_isr(void);
#else
static irqreturn_t ilitek_i2c_isr(int, void*);
#endif /* MTK_PLATFORM */
static void ilitek_i2c_irq_work_queue_func(struct work_struct*);

// file operation functions
static int ilitek_file_open(struct inode*, struct file*);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
static int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
#endif
static int ilitek_file_open(struct inode*, struct file*);
static ssize_t ilitek_file_write(struct file*, const char*, size_t, loff_t*);
static ssize_t ilitek_file_read(struct file*, char*, size_t, loff_t*);
static int ilitek_file_close(struct inode*, struct file*);

static void ilitek_i2c_irq_enable(void);
static void ilitek_i2c_irq_disable(void);

static int ilitek_i2c_reset(void);

// key
struct key_info {
	int id;
	int x;
	int y;
	int status;
	int flag;
};

// declare i2c data member
struct i2c_data {
	// input device
	struct input_dev *input_dev;
	// i2c client
	struct i2c_client *client;
	// polling thread
	struct task_struct *thread;
	// maximum x
	int max_x;
	// maximum y
	int max_y;
	// maximum touch point
	int max_tp;
	// maximum key button
	int max_btn;
	// the total number of x channel
	int x_ch;
	// the total number of y channel
	int y_ch;
	// check whether i2c driver is registered success
	int valid_i2c_register;
	// check whether input driver is registered success
	int valid_input_register;
	// check whether the i2c enter suspend or not
	int stop_polling;
	// read semaphore
	struct semaphore wr_sem;
	// protocol version
	int protocol_ver;
	// valid irq request
	int valid_irq_request;
	// work queue for interrupt use only
	struct workqueue_struct *irq_work_queue;
	// work struct for work queue
	struct work_struct irq_work;
	
	struct timer_list timer;
	
	int report_status;
	
	int irq_status;
	// irq_status enable:1 disable:0
	struct completion complete;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	int keyflag;
	int keycount;
	int key_xlen;
	int key_ylen;
	struct key_info keyinfo[10];
};
// device data
struct dev_data {
	// device number
	dev_t devno;
	// character device
	struct cdev cdev;
	// class device
	struct class *class;
};
