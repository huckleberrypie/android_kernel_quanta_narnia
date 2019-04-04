/*****************************************************************************/
/* Copyright (c) 2009 NXP Semiconductors BV                                  */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation, using version 2 of the License.             */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307       */
/* USA.                                                                      */
/*                                                                           */
/*****************************************************************************/
#define TMFL_TDA19989

#define _tx_c_
#include <linux/autoconf.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/disp_assert_layer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/switch.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <mach/dma.h>
#include <mach/irqs.h>
#include <asm/tlbflush.h>
#include <asm/page.h>

#include <mach/m4u.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>

#include "hdmitx.h"
#include "internal_hdmi_drv.h"

#include "hdmi_utils.h"

#include "mach/eint.h"
#include "mach/irqs.h"


#ifdef I2C_DBG
#include "tmbslHdmiTx_types.h"
#include "tmbslTDA9989_local.h"
#endif


#define HDMI_DEVNAME "hdmitx"

#undef OUTREG32
#define OUTREG32(x, y) {/*printk("[hdmi]write 0x%08x to 0x%08x\n", (y), (x)); */__OUTREG32((x),(y))}
#define __OUTREG32(x,y) {*(unsigned int*)(x)=(y);}

#define RETIF(cond, rslt)       if ((cond)){HDMI_LOG("return in %d\n",__LINE__);return (rslt);}
#define RET_VOID_IF(cond)       if ((cond)){HDMI_LOG("return in %d\n",__LINE__);return;}
#define RETIF_NOLOG(cond, rslt)       if ((cond)){return (rslt);}
#define RET_VOID_IF_NOLOG(cond)       if ((cond)){return;}
#define RETIFNOT(cond, rslt)    if (!(cond)){HDMI_LOG("return in %d\n",__LINE__);return (rslt);}

#define HDMI_DPI(suffix)        DPI1 ## suffix
#define HMID_DEST_DPI    		DISP_MODULE_DPI1
static int hdmi_bpp = 3;


#define ALIGN_TO(x, n)  \
    (((x) + ((n) - 1)) & ~((n) - 1))

extern const HDMI_DRIVER* HDMI_GetDriver(void);

extern UINT32 DISP_GetScreenHeight(void);
extern UINT32 DISP_GetScreenWidth(void);
extern BOOL DISP_IsVideoMode(void);
extern int disp_lock_mutex(void);
extern int disp_unlock_mutex(int id);
static void hdmi_update_impl(void);

static size_t hdmi_log_on = 1;
static struct switch_dev hdmi_switch_data;
HDMI_PARAMS _s_hdmi_params = {0};
HDMI_PARAMS *hdmi_params = &_s_hdmi_params;
static HDMI_DRIVER *hdmi_drv = NULL;

 static size_t hdmi_colorspace = HDMI_RGB;
 int flag_resolution_interlace(HDMI_VIDEO_RESOLUTION resolution)
{
  if((resolution==HDMI_VIDEO_1920x1080i_60Hz)||
  	 (resolution==HDMI_VIDEO_1920x1080i_50Hz)||
  	 (resolution==HDMI_VIDEO_1920x1080i3d_60Hz)||
  	 (resolution==HDMI_VIDEO_1920x1080i3d_60Hz))
  	 return true;
  else
  	 return false;
}
 int flag_resolution_3d(HDMI_VIDEO_RESOLUTION resolution)
{
  if((resolution==HDMI_VIDEO_1280x720p3d_60Hz)||
  	 (resolution==HDMI_VIDEO_1280x720p3d_50Hz)||
  	 (resolution==HDMI_VIDEO_1920x1080i3d_60Hz)||
  	 (resolution==HDMI_VIDEO_1920x1080i3d_60Hz)||
  	 (resolution==HDMI_VIDEO_1920x1080p3d_24Hz)||
  	 (resolution==HDMI_VIDEO_1920x1080p3d_23Hz))
  	 return true;
  else
  	 return false;
}
 void hdmi_dpi_colorspace(unsigned char ui1colorspace)
 {
   if(ui1colorspace==HDMI_RGB)
   {
     MASKREG32(DISPSYS_BASE + 0xf010, 0, (1<<7)||(1<<6)||(1<<5));
   }
   else if(ui1colorspace==HDMI_YCBCR_444)
   {
     MASKREG32(DISPSYS_BASE + 0xf010, (1<<7), (1<<7)||(1<<6)||(1<<5));
   }
   else if(ui1colorspace==HDMI_YCBCR_422)
   {
     MASKREG32(DISPSYS_BASE + 0xf010, (1<<7)||(1<<5), (1<<7)||(1<<6)||(1<<5));
   }
 }

void hdmi_log_enable(int enable)
{
	printk("hdmi log %s\n", enable?"enabled":"disabled");
	hdmi_log_on = enable;
	hdmi_drv->log_enable(enable);
}

static DEFINE_SEMAPHORE(hdmi_update_mutex);
typedef struct{
    bool is_reconfig_needed;    // whether need to reset HDMI memory
    bool is_enabled;    // whether HDMI is enabled or disabled by user
    bool is_force_disable; 		//used for camera scenario.
    bool is_clock_on;   // DPI is running or not
    atomic_t state; // HDMI_POWER_STATE state
    int 	lcm_width;  // LCD write buffer width
    int		lcm_height; // LCD write buffer height
    bool    lcm_is_video_mode;
    int		hdmi_width; // DPI read buffer width
    int		hdmi_height; // DPI read buffer height
    HDMI_VIDEO_RESOLUTION		output_video_resolution;
    HDMI_AUDIO_FORMAT           output_audio_format;
    int		orientation;    // MDP's orientation, 0 means 0 degree, 1 means 90 degree, 2 means 180 degree, 3 means 270 degree
    HDMI_OUTPUT_MODE    output_mode;
    int     scaling_factor;
}_t_hdmi_context;

struct hdmi_video_buffer_list {
    struct hdmi_video_buffer_info buffer_info;
    pid_t  pid;
    void*  file_addr;
    unsigned int buffer_mva;
    struct list_head list;
};

static struct list_head hdmi_video_mode_buffer_list;
static struct list_head *hdmi_video_buffer_list_head = &hdmi_video_mode_buffer_list;
DEFINE_SEMAPHORE(hdmi_video_mode_mutex);
static atomic_t hdmi_video_mode_flag = ATOMIC_INIT(0);
static int hdmi_add_video_buffer(struct hdmi_video_buffer_info *buffer_info, struct file *file);
static struct hdmi_video_buffer_list* hdmi_search_video_buffer(struct hdmi_video_buffer_info *buffer_info, struct file *file);
static void hdmi_destory_video_buffer(void);
#define IS_HDMI_IN_VIDEO_MODE()        atomic_read(&hdmi_video_mode_flag)
#define SET_HDMI_TO_VIDEO_MODE()       atomic_set(&hdmi_video_mode_flag, 1)
#define SET_HDMI_LEAVE_VIDEO_MODE()    atomic_set(&hdmi_video_mode_flag, 0)
static wait_queue_head_t hdmi_video_mode_wq;
//static atomic_t hdmi_video_mode_event = ATOMIC_INIT(0);
//static atomic_t hdmi_video_mode_dpi_change_address = ATOMIC_INIT(0);
#define IS_HDMI_VIDEO_MODE_DPI_IN_CHANGING_ADDRESS()    atomic_read(&hdmi_video_mode_dpi_change_address)
#define SET_HDMI_VIDEO_MODE_DPI_CHANGE_ADDRESS()        atomic_set(&hdmi_video_mode_dpi_change_address, 1)
#define SET_HDMI_VIDEO_MODE_DPI_CHANGE_ADDRESS_DONE()   atomic_set(&hdmi_video_mode_dpi_change_address, 0)


static _t_hdmi_context hdmi_context;
static _t_hdmi_context *p = &hdmi_context;

#define IS_HDMI_ON()			(HDMI_POWER_STATE_ON == atomic_read(&p->state))
#define IS_HDMI_OFF()			(HDMI_POWER_STATE_OFF == atomic_read(&p->state))
#define IS_HDMI_STANDBY()	    (HDMI_POWER_STATE_STANDBY == atomic_read(&p->state))

#define IS_HDMI_NOT_ON()		(HDMI_POWER_STATE_ON != atomic_read(&p->state))
#define IS_HDMI_NOT_OFF()		(HDMI_POWER_STATE_OFF != atomic_read(&p->state))
#define IS_HDMI_NOT_STANDBY()	(HDMI_POWER_STATE_STANDBY != atomic_read(&p->state))

#define SET_HDMI_ON()	        atomic_set(&p->state, HDMI_POWER_STATE_ON)
#define SET_HDMI_OFF()	        atomic_set(&p->state, HDMI_POWER_STATE_OFF)
#define SET_HDMI_STANDBY()	    atomic_set(&p->state, HDMI_POWER_STATE_STANDBY)

int hdmi_allocate_hdmi_buffer(void);
int hdmi_free_hdmi_buffer(void);
int hdmi_display_path_overlay_config(bool enable);
int hdmi_dst_display_path_config(bool enable);
int hdmi_src_display_path_config(bool enable);
static int dp_mutex_src = -1, dp_mutex_dst = -1;
static unsigned int temp_mva_r, temp_mva_w, temp_va, hdmi_va, hdmi_mva_r, hdmi_mva_w;


static dev_t hdmi_devno;
static struct cdev *hdmi_cdev;
static struct class *hdmi_class = NULL;

#include <linux/mmprofile.h>
struct HDMI_MMP_Events_t
{
    MMP_Event HDMI;
    MMP_Event DDPKBitblt;
    MMP_Event OverlayDone;
    MMP_Event SwitchRDMABuffer;
    MMP_Event SwitchOverlayBuffer;
    MMP_Event StopOverlayBuffer;
    MMP_Event RDMA1RegisterUpdated;
    MMP_Event WDMA1RegisterUpdated;
} HDMI_MMP_Events;


// ---------------------------------------------------------------------------
//  Information Dump Routines
// ---------------------------------------------------------------------------

void init_hdmi_mmp_events(void)
{
    if (HDMI_MMP_Events.HDMI == 0)
    {
        HDMI_MMP_Events.HDMI = MMProfileRegisterEvent(MMP_RootEvent, "HDMI");
        HDMI_MMP_Events.OverlayDone = MMProfileRegisterEvent(HDMI_MMP_Events.HDMI, "OverlayDone");
        HDMI_MMP_Events.DDPKBitblt = MMProfileRegisterEvent(HDMI_MMP_Events.HDMI, "DDPKBitblt");
        HDMI_MMP_Events.SwitchRDMABuffer = MMProfileRegisterEvent(HDMI_MMP_Events.HDMI, "SwitchRDMABuffer");
        HDMI_MMP_Events.RDMA1RegisterUpdated = MMProfileRegisterEvent(HDMI_MMP_Events.HDMI, "RDMA1RegisterUpdated");
        HDMI_MMP_Events.SwitchOverlayBuffer = MMProfileRegisterEvent(HDMI_MMP_Events.HDMI, "SwitchOverlayBuffer");
        HDMI_MMP_Events.WDMA1RegisterUpdated = MMProfileRegisterEvent(HDMI_MMP_Events.HDMI, "WDMA1RegisterUpdated");
        HDMI_MMP_Events.StopOverlayBuffer = MMProfileRegisterEvent(HDMI_MMP_Events.HDMI, "StopOverlayBuffer");
        MMProfileEnableEventRecursive(HDMI_MMP_Events.HDMI, 1);
    }
}

//static int hdmi_default_width = 1280;
//static int hdmi_default_height = 720;

#define ENABLE_HDMI_FPS_CONTROL_LOG 1
#if ENABLE_HDMI_FPS_CONTROL_LOG
static unsigned int hdmi_fps_control_fps_wdma0 = 0;
static unsigned long hdmi_fps_control_time_base_wdma0 = 0;
static unsigned int hdmi_fps_control_fps_wdma1 = 0;
static unsigned long hdmi_fps_control_time_base_wdma1 = 0;
static unsigned int hdmi_fps_control_fps_rdma1 = 0;
static unsigned long hdmi_fps_control_time_base_rdma1 = 0;
#endif

typedef enum
{
    HDMI_OVERLAY_STATUS_STOPPED,
    HDMI_OVERLAY_STATUS_STOPPING,
    HDMI_OVERLAY_STATUS_STARTING,
    HDMI_OVERLAY_STATUS_STARTED,
}HDMI_OVERLAY_STATUS;

static unsigned int hdmi_fps_control_dpi = 0;
static unsigned int hdmi_fps_control_overlay = 0;
static HDMI_OVERLAY_STATUS hdmi_overlay_status = HDMI_OVERLAY_STATUS_STOPPED;
static unsigned int hdmi_rdma_switch_count = 0;

static int hdmi_buffer_write_id = 0;
static int hdmi_buffer_read_id = 0;
static int hdmi_buffer_read_id_tmp = 0;
static int hdmi_buffer_lcdw_id = 0;
static int hdmi_buffer_lcdw_id_tmp = 0;

//static DPI_POLARITY clk_pol, de_pol, hsync_pol, vsync_pol;
static unsigned int dpi_clk_div, dpi_clk_duty, hsync_pulse_width, hsync_back_porch, hsync_front_porch, vsync_pulse_width, vsync_back_porch, vsync_front_porch, intermediat_buffer_num;
static HDMI_COLOR_ORDER rgb_order;

static struct task_struct *hdmi_update_task = NULL;
static wait_queue_head_t hdmi_update_wq;
static atomic_t hdmi_update_event = ATOMIC_INIT(0);

static struct task_struct *hdmi_overlay_config_task = NULL;
static wait_queue_head_t hdmi_overlay_config_wq;
static atomic_t hdmi_overlay_config_event = ATOMIC_INIT(0);

static struct task_struct *hdmi_rdma_config_task = NULL;
static wait_queue_head_t hdmi_rdma_config_wq;
static atomic_t hdmi_rdma_config_event = ATOMIC_INIT(0);

static unsigned int hdmi_resolution_param_table[][3] =
{

		{720,   480,    60},
        {720,   576,    50},
        {1280,  720,    60},
        {1280,  720,    50},
        
        {1920,  1080,   60},
        {1920,  1080,   50},
        {1920,  1080,   30},
        {1920,  1080,   25},
        {1920,  1080,   24},
        {1920,  1080,   23},
        {1920,  1080,   29},
        
        {1920,  1080,   60},
        {1920,  1080,   50},

};

#define ENABLE_HDMI_BUFFER_LOG 1
#if ENABLE_HDMI_BUFFER_LOG
bool enable_hdmi_buffer_log = 0;
#define HDMI_BUFFER_LOG(fmt, arg...) \
    do { \
        if(enable_hdmi_buffer_log){printk("[hdmi_buffer] "); printk(fmt, ##arg);} \
    }while (0)
#else
bool enable_hdmi_buffer_log = 0;
#define HDMI_BUFFER_LOG(fmt, arg...)
#endif

int hdmi_rdma_buffer_switch_mode = 0; // 0: switch in rdma1 frame done interrupt, 1: switch after DDPK_Bitblt done
static int hdmi_buffer_num = 0;
static int* hdmi_buffer_available = 0;
static int* hdmi_buffer_queue = 0;
static int hdmi_buffer_end = 0;
static int hdmi_buffer_start = 0;
static int hdmi_buffer_fill_count = 0;
static DEFINE_SEMAPHORE(hdmi_buffer_mutex);

static void hdmi_buffer_init(int num)
{}

static void hdmi_buffer_deinit(void)
{}

static void hdmi_dump_buffer_queue(void)
{}

static int hdmi_is_buffer_empty(void)
{}

static void hdmi_release_buffer(int index)
{}

static int hdmi_acquire_buffer(void)
{}

static int hdmi_dequeue_buffer(void)
{}

static void hdmi_enqueue_buffer(int index)
{}

static unsigned long get_current_time_us(void)
{
    struct timeval t;
    do_gettimeofday(&t);
    return t.tv_sec * 1000 + t.tv_usec / 1000;
}

static void hdmi_udelay(unsigned int us)
{
	udelay(us);
}

static void hdmi_mdelay(unsigned int ms)
{
	msleep(ms);
}

static unsigned int hdmi_get_width(HDMI_VIDEO_RESOLUTION r)
{
    ASSERT(r < HDMI_VIDEO_RESOLUTION_NUM);
    return hdmi_resolution_param_table[r][0];
}

static unsigned int hdmi_get_height(HDMI_VIDEO_RESOLUTION r)
{
    ASSERT(r < HDMI_VIDEO_RESOLUTION_NUM);
    return hdmi_resolution_param_table[r][1];
}


static atomic_t hdmi_fake_in = ATOMIC_INIT(false);
#define IS_HDMI_FAKE_PLUG_IN()  (true == atomic_read(&hdmi_fake_in))
#define SET_HDMI_FAKE_PLUG_IN() (atomic_set(&hdmi_fake_in, true))
#define SET_HDMI_NOT_FAKE()     (atomic_set(&hdmi_fake_in, false))

// For Debugfs
void hdmi_cable_fake_plug_in(void)
{
    SET_HDMI_FAKE_PLUG_IN();
    HDMI_LOG("[HDMIFake]Cable Plug In\n");
    if(p->is_force_disable == false)
    {
        if (IS_HDMI_STANDBY())
        {
            hdmi_resume( );
            msleep(1000);
            switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
        }
    }
}

// For Debugfs
void hdmi_cable_fake_plug_out(void)
{
    SET_HDMI_NOT_FAKE();
    HDMI_LOG("[HDMIFake]Disable\n");
    if(p->is_force_disable == false)
    {
        if (IS_HDMI_ON())
        {
            if (hdmi_drv->get_state() != HDMI_STATE_ACTIVE)
            {
                hdmi_suspend( );
                switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
            }
        }
    }
}

void hdmi_set_mode(unsigned char ucMode)
{
    HDMI_FUNC();

    hdmi_drv->set_mode(ucMode);

    return;
}

void hdmi_reg_dump(void)
{
	hdmi_drv->dump();
}
void hdmi_read_reg(unsigned char u1iReg, unsigned int *p4Data)
{
	hdmi_drv->read(u1iReg, p4Data);
}

void hdmi_write_reg(unsigned char ui1Reg, unsigned char ui1Data)
{
	hdmi_drv->write(ui1Reg, ui1Data);
}

/* Used for HDMI Driver update */
static int hdmi_update_kthread(void *data)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
    sched_setscheduler(current, SCHED_RR, &param);

    for( ;; ) {
        wait_event_interruptible(hdmi_update_wq, atomic_read(&hdmi_update_event));
        //HDMI_LOG("wq wakeup\n");
        //hdmi_update_impl();

        atomic_set(&hdmi_update_event,0);
        hdmi_update_impl();
        if (kthread_should_stop())
            break;
    }

    return 0;
}

/* Will be called in LCD Interrupt handler to check whether HDMI is actived */
bool is_hdmi_active(void)
{
	return IS_HDMI_ON();
}

void hdmi_wdma1_done(void)
{
#if ENABLE_HDMI_FPS_CONTROL_LOG
    unsigned long currentTime = get_current_time_us();
    hdmi_fps_control_fps_wdma1++;

    if(currentTime - hdmi_fps_control_time_base_wdma1 >= 1000)
    {
        HDMI_LOG("overlay>wdma1 fps:%d\n", hdmi_fps_control_fps_wdma1);
        hdmi_fps_control_time_base_wdma1 = currentTime;
        hdmi_fps_control_fps_wdma1 = 0;
    }
#endif
}

void hdmi_wdma0_done(void)
{
#if ENABLE_HDMI_FPS_CONTROL_LOG
    unsigned long currentTime = get_current_time_us();
    hdmi_fps_control_fps_wdma0++;

    if(currentTime - hdmi_fps_control_time_base_wdma0 >= 1000)
    {
        HDMI_LOG("rot>scl>wdma0 fps:%d\n", hdmi_fps_control_fps_wdma0);
        hdmi_fps_control_time_base_wdma0 = currentTime;
        hdmi_fps_control_fps_wdma0 = 0;
    }
#endif
}

void hdmi_rdma1_done(void)
{
#if ENABLE_HDMI_FPS_CONTROL_LOG
    unsigned long currentTime = get_current_time_us();
    hdmi_fps_control_fps_rdma1++;

    if(currentTime - hdmi_fps_control_time_base_rdma1 >= 1000)
    {
        HDMI_LOG("rdma1>dpi fps:%d(%d)\n", hdmi_fps_control_fps_rdma1, hdmi_rdma_switch_count);
        hdmi_fps_control_time_base_rdma1 = currentTime;
        hdmi_fps_control_fps_rdma1 = 0;
        hdmi_rdma_switch_count = 0;
    }
#endif
}

/* Used for HDMI Driver update */
static int hdmi_overlay_config_kthread(void *data)
{}

/* Switch LCD write buffer, will be called in LCD Interrupt handler */
void hdmi_source_buffer_switch(void)
{}

void hdmi_set_rdma_address(int bufferIndex)
{}

void hdmi_rdma_buffer_switch(void)
{}

/* Switch DPI read buffer, will be called in DPI Interrupt handler */
void hdmi_update_buffer_switch(void)
{}

static int hdmi_rdma_config_kthread(void *data)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
    sched_setscheduler(current, SCHED_RR, &param);

    for( ;; ) {
        wait_event_interruptible(hdmi_rdma_config_wq, atomic_read(&hdmi_rdma_config_event));
        atomic_set(&hdmi_rdma_config_event, 0);

        hdmi_rdma_buffer_switch();

        if (kthread_should_stop())
            break;
    }

    return 0;
}

extern void DBG_OnTriggerHDMI(void);
extern void DBG_OnHDMIDone(void);

/* hdmi update api, will be called in LCD Interrupt handler */
void hdmi_update(void)
{
    //HDMI_FUNC();
#if 1
    RET_VOID_IF(IS_HDMI_NOT_ON());
    RET_VOID_IF_NOLOG(p->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS);

    atomic_set(&hdmi_update_event, 1);
    wake_up_interruptible(&hdmi_update_wq); //wake up hdmi_update_kthread() to do update
#else
    hdmi_update_impl();
#endif
}

static void hdmi_update_impl(void)
{}
#define HDMI_ANALOG_BASE		0xF0209000

#define MHL_TVDPLL_CON0	0x294
	#define RG_TVDPLL_EN			(1)
	#define RG_TVDPLL_POSDIV				(6)
	#define RG_TVDPLL_POSDIV_MASK			(0x07 << 6)
#define MHL_TVDPLL_CON1	0x298
	#define RG_TVDPLL_SDM_PCW				(0)
	#define RG_TVDPLL_SDM_PCW_MASK			(0x7FFFFFFF)
#define MHL_TVDPLL_PWR	0x2AC
	#define RG_TVDPLL_PWR_ON		(1)
#define vWriteHdmiANA(dAddr, dVal)  (*((volatile unsigned int *)(HDMI_ANALOG_BASE + dAddr)) = (dVal))
#define dReadHdmiANA(dAddr)         (*((volatile unsigned int *)(HDMI_ANALOG_BASE + dAddr)))
#define vWriteHdmiANAMsk(dAddr, dVal, dMsk) (vWriteHdmiANA((dAddr), (dReadHdmiANA(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

//--------------------------FIXME-------------------------------
void hdmi_config_pll(HDMI_VIDEO_RESOLUTION resolution)
{
    unsigned int con1, con0;
	vWriteHdmiANAMsk(0x0,0x1131,0x1131);
	vWriteHdmiANAMsk(MHL_TVDPLL_PWR,RG_TVDPLL_PWR_ON,RG_TVDPLL_PWR_ON);
	mdelay(1);
	vWriteHdmiANAMsk(0x0,0x1133,0x1133);
	
    switch(resolution)
    {
		case HDMI_VIDEO_720x480p_60Hz:
		case HDMI_VIDEO_720x576p_50Hz:
			vWriteHdmiANAMsk(MHL_TVDPLL_CON0,0,RG_TVDPLL_EN);
			vWriteHdmiANAMsk(MHL_TVDPLL_CON0,(0x04 << RG_TVDPLL_POSDIV),RG_TVDPLL_POSDIV_MASK);
			vWriteHdmiANAMsk(MHL_TVDPLL_CON1,(1115039586 << RG_TVDPLL_SDM_PCW),RG_TVDPLL_SDM_PCW_MASK);
			vWriteHdmiANAMsk(MHL_TVDPLL_CON0,RG_TVDPLL_EN,RG_TVDPLL_EN);

			break;
			
		case HDMI_VIDEO_1920x1080p_30Hz:
		case HDMI_VIDEO_1280x720p_50Hz:
		case HDMI_VIDEO_1920x1080i_50Hz:
		case HDMI_VIDEO_1920x1080p_25Hz:
		case HDMI_VIDEO_1920x1080p_24Hz:
		case HDMI_VIDEO_1920x1080p_50Hz:
		case HDMI_VIDEO_1280x720p3d_50Hz:	
		case HDMI_VIDEO_1920x1080i3d_50Hz:
		case HDMI_VIDEO_1920x1080p3d_24Hz:	
			vWriteHdmiANAMsk(MHL_TVDPLL_CON0,0,RG_TVDPLL_EN);
			vWriteHdmiANAMsk(MHL_TVDPLL_CON0,(0x04 << RG_TVDPLL_POSDIV),RG_TVDPLL_POSDIV_MASK);
			vWriteHdmiANAMsk(MHL_TVDPLL_CON1,(1533179431 << RG_TVDPLL_SDM_PCW),RG_TVDPLL_SDM_PCW_MASK);
			vWriteHdmiANAMsk(MHL_TVDPLL_CON0,RG_TVDPLL_EN,RG_TVDPLL_EN);

			break;

		case HDMI_VIDEO_1280x720p_60Hz:
		case HDMI_VIDEO_1920x1080i_60Hz:
		case HDMI_VIDEO_1920x1080p_23Hz:
		case HDMI_VIDEO_1920x1080p_29Hz:
		case HDMI_VIDEO_1920x1080p_60Hz:
		case HDMI_VIDEO_1280x720p3d_60Hz:
		case HDMI_VIDEO_1920x1080i3d_60Hz:
		case HDMI_VIDEO_1920x1080p3d_23Hz:		
			vWriteHdmiANAMsk(MHL_TVDPLL_CON0,0,RG_TVDPLL_EN);
			vWriteHdmiANAMsk(MHL_TVDPLL_CON0,(0x04 << RG_TVDPLL_POSDIV),RG_TVDPLL_POSDIV_MASK);
			vWriteHdmiANAMsk(MHL_TVDPLL_CON1,(1531630765 << RG_TVDPLL_SDM_PCW),RG_TVDPLL_SDM_PCW_MASK);
			vWriteHdmiANAMsk(MHL_TVDPLL_CON0,RG_TVDPLL_EN,RG_TVDPLL_EN);

			break;
        default:
        {
            printk("[hdmi] not supported format, %s, %d, format = %d\n", __func__, __LINE__, resolution);
        }
    }
	
}

static void _rdma1_irq_handler(unsigned int param)
{}

static void _register_updated_irq_handler(unsigned int param)
{}
 
/* Allocate memory, set M4U, LCD, MDP, DPI */
/* LCD overlay to memory -> MDP resize and rotate to memory -> DPI read to HDMI */
/* Will only be used in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
static HDMI_STATUS hdmi_drv_init(void)
{}

//free IRQ
/*static*/ void hdmi_dpi_free_irq(void)
{}

/* Release memory */
/* Will only be used in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
static  HDMI_STATUS hdmi_drv_deinit(void)
{}

static void hdmi_dpi_config_update(void)
{}


/* Will only be used in hdmi_drv_init(), this means that will only be use in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
/*static*/ void hdmi_dpi_config_clock(void)
{}


int hdmi_allocate_hdmi_buffer(void)
{}

int hdmi_free_hdmi_buffer(void)
{}

int hdmi_rdma_address_config(bool enable, void* hdmi_mva)
{}
int hdmi_dst_display_path_config(bool enable)
{}

int hdmi_src_display_path_config(bool enable)
{}

int hdmi_display_path_overlay_config(bool enable)
{}

/* Switch DPI Power for HDMI Driver */
/*static*/ void hdmi_dpi_power_switch(bool enable)
{}

/* Configure video attribute */
static int hdmi_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin, HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	HDMI_FUNC();
	RETIF(IS_HDMI_NOT_ON(), 0);

    //hdmi_allocate_hdmi_buffer();
    //hdmi_src_display_path_config(true);
    //hdmi_dst_display_path_config(true);

    hdmi_fps_control_overlay = 0;
    hdmi_fps_control_dpi = 0;

    return hdmi_drv->video_config(vformat, vin, vout);
}

/* Configure audio attribute, will be called by audio driver */
int hdmi_audio_config(int samplerate)
{
    HDMI_FUNC();
	RETIF(!p->is_enabled, 0);
	RETIF(IS_HDMI_NOT_ON(), 0);

    HDMI_LOG("sample rate=%d\n", samplerate);
    if(samplerate == 48000)
    {
        p->output_audio_format = HDMI_AUDIO_PCM_16bit_48000;
    }
    else if(samplerate == 44100)
    {
        p->output_audio_format = HDMI_AUDIO_PCM_16bit_44100;
    }
    else if(samplerate == 32000)
	{
        p->output_audio_format = HDMI_AUDIO_PCM_16bit_32000;
    }
    else
    {
        HDMI_LOG("samplerate not support:%d\n", samplerate);
    }


    hdmi_drv->audio_config(p->output_audio_format);

    return 0;
}

/* No one will use this function */
/*static*/ int hdmi_video_enable(bool enable)
{
    HDMI_FUNC();


	return hdmi_drv->video_enable(enable);
}

/* No one will use this function */
/*static*/ int hdmi_audio_enable(bool enable)
{
    HDMI_FUNC();


	return hdmi_drv->audio_enable(enable);
}

struct timer_list timer;
void __timer_isr(unsigned long n)
{
    HDMI_FUNC();
    if(hdmi_drv->audio_enable) hdmi_drv->audio_enable(true);

    del_timer(&timer);
}

int hdmi_audio_delay_mute(int latency)
{
    HDMI_FUNC();
    memset((void*)&timer, 0, sizeof(timer));
    timer.expires = jiffies +  ( latency * HZ / 1000 );
    timer.function = __timer_isr;
    init_timer(&timer);
    add_timer(&timer);
    if(hdmi_drv->audio_enable) hdmi_drv->audio_enable(false);
    return 0;
}

/* Reset HDMI Driver state */
static void hdmi_state_reset(void)
{
    HDMI_FUNC();

    if(hdmi_drv->get_state() == HDMI_STATE_ACTIVE)
    {
        switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
    }
    else
    {
        switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
    }
}

/* HDMI Driver state callback function */
void hdmi_state_callback(HDMI_STATE state)
{

    printk("[hdmi]%s, state = %d\n", __func__, state);

    RET_VOID_IF((p->is_force_disable == true));
    RET_VOID_IF(IS_HDMI_FAKE_PLUG_IN());

    switch(state)
    {
        case HDMI_STATE_NO_DEVICE:
        {
            hdmi_suspend();
            switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);

            break;
        }
        case HDMI_STATE_ACTIVE:
        {
            hdmi_resume();
            //force update screen
            //DISP_UpdateScreen(0, 0, DISP_GetScreenWidth(), DISP_GetScreenHeight());
            if (HDMI_OUTPUT_MODE_LCD_MIRROR == p->output_mode) 
            {
                //msleep(1000);
            }
            //switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE); 

            break;
        }
        default:
        {
            printk("[hdmi]%s, state not support\n", __func__);
            break;
        }
    }

    return;
}

/*static*/ void hdmi_power_on(void)
{
    HDMI_FUNC();

    RET_VOID_IF(IS_HDMI_NOT_OFF());

	if (down_interruptible(&hdmi_update_mutex)) {
			printk("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
			return;
	}

	// Why we set power state before calling hdmi_drv->power_on()?
	// Because when power on, the hpd irq will come immediately, that means hdmi_resume will be called before hdmi_drv->power_on() retuen here.
	// So we have to ensure the power state is STANDBY before hdmi_resume() be called.
	SET_HDMI_STANDBY();

    hdmi_drv->power_on();

	// When camera is open, the state will only be changed when camera exits.
	// So we bypass state_reset here, if camera is open.
	// The related scenario is: suspend in camera with hdmi enabled.
	// Why need state_reset() here?
	// When we suspend the phone, and then plug out hdmi cable, the hdmi chip status will change immediately
	// But when we resume the phone and check hdmi status, the irq will never come again
	// So we have to reset hdmi state manually, to ensure the status is the same between the host and hdmi chip.
	#if (!defined(MTK_MT8193_HDMI_SUPPORT))&&( !defined(MTK_INTERNAL_HDMI_SUPPORT))
    if(p->is_force_disable == false)
    {
        if (IS_HDMI_FAKE_PLUG_IN())
        {
            //FixMe, deadlock may happened here, due to recursive use mutex
            hdmi_resume();
            msleep(1000);
            switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
        }
        else
        {
            hdmi_state_reset();
            // this is just a ugly workaround for some tv sets...
            //if(hdmi_drv->get_state() == HDMI_STATE_ACTIVE)
            //	hdmi_resume();
        }
    }
    #endif
    up(&hdmi_update_mutex);

    return;
}

/*static*/ void hdmi_power_off(void)
{
    HDMI_FUNC();

    RET_VOID_IF(IS_HDMI_OFF());

	if (down_interruptible(&hdmi_update_mutex)) {
			printk("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
			return;
	}

	hdmi_drv->power_off();
    //hdmi_dpi_power_switch(false);
	SET_HDMI_OFF();
	up(&hdmi_update_mutex);

    return;
}

/*static*/ void hdmi_suspend(void)
{
    HDMI_FUNC();

    RET_VOID_IF(IS_HDMI_NOT_ON());

    if (down_interruptible(&hdmi_update_mutex)) {
        printk("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
        return;
    }
    //hdmi_dpi_power_switch(false);
    hdmi_drv->suspend();
    SET_HDMI_STANDBY();
    up(&hdmi_update_mutex);
}

/*static*/ void hdmi_resume(void)
{
    HDMI_FUNC();

    RET_VOID_IF(IS_HDMI_NOT_STANDBY());
    SET_HDMI_ON();


}

/* Set HDMI orientation, will be called in mtkfb_ioctl(SET_ORIENTATION) */
/*static*/ void hdmi_setorientation(int orientation)
{
	HDMI_FUNC();
    RET_VOID_IF(!p->is_enabled);

	if(down_interruptible(&hdmi_update_mutex))
	{
		printk("[hdmi][HDMI] can't get semaphore in %s\n", __func__);
		return;
	}

	p->orientation = orientation;
	p->is_reconfig_needed = true;

//done:
	up(&hdmi_update_mutex);
}

static int hdmi_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int hdmi_open(struct inode *inode, struct file *file)
{
	return 0;
}

static BOOL hdmi_drv_init_context(void);

static void dpi_setting_res(unsigned char arg)
{
	switch(arg)
	 {
	  case HDMI_VIDEO_720x480p_60Hz:
	  {
	
	*(unsigned int*)0xf400f000 = 0x00000001;
	*(unsigned int*)0xf400f004 = 0x00000000;
	*(unsigned int*)0xf400f008 = 0x00000001;
	*(unsigned int*)0xf400f00c = 0x00000000;
	*(unsigned int*)0xf400f010 = 0x00000000;
	*(unsigned int*)0xf400f014 = 0x82000200;
	*(unsigned int*)0xf400f018 = 0x01e002d0;
	*(unsigned int*)0xf400f01c = 0x0000003e;
	*(unsigned int*)0xf400f020 = 0x0010003c;
	*(unsigned int*)0xf400f024 = 0x00000006;
	*(unsigned int*)0xf400f028 = 0x0009001e;
	*(unsigned int*)0xf400f02c = 0x00000000;
	*(unsigned int*)0xf400f030 = 0x00000000;
	*(unsigned int*)0xf400f034 = 0x00000000;
	*(unsigned int*)0xf400f038 = 0x00000000;
	*(unsigned int*)0xf400f03c = 0x00000000;
	*(unsigned int*)0xf400f040 = 0x00000000;

	*(unsigned int*)0xf400f05c = 0x00000000;
	
	*(unsigned int*)0xf400ff00 = 0x00000041;

	*(unsigned int*)0xf400f080 = 0x00000000;
	*(unsigned int*)0xf400f084 = 0x00000000;
	*(unsigned int*)0xf400f094 = 0x00000000;

		 break;
	  }
	  case HDMI_VIDEO_720x576p_50Hz:
	  {
	
	*(unsigned int*)0xf400f000 = 0x00000001;
	*(unsigned int*)0xf400f004 = 0x00000000;
	*(unsigned int*)0xf400f008 = 0x00000001;
	*(unsigned int*)0xf400f00c = 0x00000000;
	*(unsigned int*)0xf400f010 = 0x00000000;
	*(unsigned int*)0xf400f014 = 0x82000200;
	*(unsigned int*)0xf400f018 = 0x024002d0;
	*(unsigned int*)0xf400f01c = 0x00000040;
	*(unsigned int*)0xf400f020 = 0x000c0044;
	*(unsigned int*)0xf400f024 = 0x00000005;
	*(unsigned int*)0xf400f028 = 0x00050027;
	*(unsigned int*)0xf400f02c = 0x00000000;
	*(unsigned int*)0xf400f030 = 0x00000000;
	*(unsigned int*)0xf400f034 = 0x00000000;
	*(unsigned int*)0xf400f038 = 0x00000000;
	*(unsigned int*)0xf400f03c = 0x00000000;
	*(unsigned int*)0xf400f040 = 0x00000000;

	*(unsigned int*)0xf400f05c = 0x00000000;
	
	*(unsigned int*)0xf400ff00 = 0x00000041;

	*(unsigned int*)0xf400f080 = 0x00000000;
	*(unsigned int*)0xf400f084 = 0x00000000;
	*(unsigned int*)0xf400f094 = 0x00000000;

		 break;
	  }
	  case HDMI_VIDEO_1280x720p_60Hz:
	  {
	
	*(unsigned int*)0xf400f000 = 0x00000001;
	*(unsigned int*)0xf400f004 = 0x00000000;
	*(unsigned int*)0xf400f008 = 0x00000001;
	*(unsigned int*)0xf400f00c = 0x00000000;
	*(unsigned int*)0xf400f010 = 0x00000000;
	*(unsigned int*)0xf400f014 = 0x82000200;
	*(unsigned int*)0xf400f018 = 0x02d00500;
	*(unsigned int*)0xf400f01c = 0x00000028;
	*(unsigned int*)0xf400f020 = 0x006e00dc;
	*(unsigned int*)0xf400f024 = 0x00000005;
	*(unsigned int*)0xf400f028 = 0x00050014;
	*(unsigned int*)0xf400f02c = 0x00000000;
	*(unsigned int*)0xf400f030 = 0x00000000;
	*(unsigned int*)0xf400f034 = 0x00000000;
	*(unsigned int*)0xf400f038 = 0x00000000;
	*(unsigned int*)0xf400f03c = 0x00000000;
	*(unsigned int*)0xf400f040 = 0x00000000;
	*(unsigned int*)0xf400f05c = 0x00000003;
	*(unsigned int*)0xf400ff00 = 0x00000041;

	*(unsigned int*)0xf400f080 = 0x00000000;
	*(unsigned int*)0xf400f084 = 0x00000000;
	*(unsigned int*)0xf400f094 = 0x00000000;

		 break;
	  }
	  case HDMI_VIDEO_1280x720p_50Hz:
	  {
	
	*(unsigned int*)0xf400f000 = 0x00000001;
	*(unsigned int*)0xf400f004 = 0x00000000;
	*(unsigned int*)0xf400f008 = 0x00000001;
	*(unsigned int*)0xf400f00c = 0x00000000;
	*(unsigned int*)0xf400f010 = 0x00000000;
	*(unsigned int*)0xf400f014 = 0x82000200;
	*(unsigned int*)0xf400f018 = 0x02d00500;
	*(unsigned int*)0xf400f01c = 0x00000028;
	*(unsigned int*)0xf400f020 = 0x01b800dc;
	*(unsigned int*)0xf400f024 = 0x00000005;
	*(unsigned int*)0xf400f028 = 0x00050014;
	*(unsigned int*)0xf400f02c = 0x00000000;
	*(unsigned int*)0xf400f030 = 0x00000000;
	*(unsigned int*)0xf400f034 = 0x00000000;
	*(unsigned int*)0xf400f038 = 0x00000000;
	*(unsigned int*)0xf400f03c = 0x00000000;
	*(unsigned int*)0xf400f040 = 0x00000000;
	*(unsigned int*)0xf400f05c = 0x00000003;
	*(unsigned int*)0xf400ff00 = 0x00000041;

	*(unsigned int*)0xf400f080 = 0x00000000;
	*(unsigned int*)0xf400f084 = 0x00000000;
	*(unsigned int*)0xf400f094 = 0x00000000;

		 break;
	  }
	  case HDMI_VIDEO_1920x1080p_24Hz:  	
	  {
		  *(unsigned int*)0xf400f000 = 0x00000001;
		  *(unsigned int*)0xf400f004 = 0x00000000;
		  *(unsigned int*)0xf400f008 = 0x00000001;
		  *(unsigned int*)0xf400f00c = 0x00000000;
		  *(unsigned int*)0xf400f010 = 0x00000000;
		  *(unsigned int*)0xf400f014 = 0x82000200;
		  *(unsigned int*)0xf400f018 = 0x04380780;
		  *(unsigned int*)0xf400f01c = 0x0000002c;
		  *(unsigned int*)0xf400f020 = 0x027e0094;
		  *(unsigned int*)0xf400f024 = 0x00000005;
		  *(unsigned int*)0xf400f028 = 0x00040024;
		  *(unsigned int*)0xf400f02c = 0x00000000;
		  *(unsigned int*)0xf400f030 = 0x00000000;
		  *(unsigned int*)0xf400f034 = 0x00000000;
		  *(unsigned int*)0xf400f038 = 0x00000000;
		  *(unsigned int*)0xf400f03c = 0x00000000;
		  *(unsigned int*)0xf400f040 = 0x00000000;
		  *(unsigned int*)0xf400f05c = 0x00000003;
		  *(unsigned int*)0xf400ff00 = 0x00000041;
		  
		  *(unsigned int*)0xf400f080 = 0x00000000;
		  *(unsigned int*)0xf400f084 = 0x00000000;
		  *(unsigned int*)0xf400f094 = 0x00000000;

		 break;
	  }
	  case HDMI_VIDEO_1920x1080p_25Hz: 	
	  	{}
	  case HDMI_VIDEO_1920x1080p_30Hz:  	
	  	{}
	  case HDMI_VIDEO_1920x1080p_29Hz:	
	  	{}
	  case HDMI_VIDEO_1920x1080p_23Hz: 	
	  	{}

    	case HDMI_VIDEO_1920x1080p_60Hz:
    	{
      
	  *(unsigned int*)0xf400f000 = 0x00000001;
	  *(unsigned int*)0xf400f004 = 0x00000000;
	  *(unsigned int*)0xf400f008 = 0x00000001;
	  *(unsigned int*)0xf400f00c = 0x00000000;
	  *(unsigned int*)0xf400f010 = 0x00000000;
	  *(unsigned int*)0xf400f014 = 0x82000200;
	  *(unsigned int*)0xf400f018 = 0x04380780;
	  *(unsigned int*)0xf400f01c = 0x0000002c;
	  *(unsigned int*)0xf400f020 = 0x00580094;
	  *(unsigned int*)0xf400f024 = 0x00000005;
	  *(unsigned int*)0xf400f028 = 0x00040024;
	  *(unsigned int*)0xf400f02c = 0x00000000;
	  *(unsigned int*)0xf400f030 = 0x00000000;
	  *(unsigned int*)0xf400f034 = 0x00000000;
	  *(unsigned int*)0xf400f038 = 0x00000000;
	  *(unsigned int*)0xf400f03c = 0x00000000;
	  *(unsigned int*)0xf400f040 = 0x00000000;
	  *(unsigned int*)0xf400f05c = 0x00000003;
	  
	  *(unsigned int*)0xf400f064 = 0x1e751f8b;
	  *(unsigned int*)0xf400f068 = 0x00da0200;
	  *(unsigned int*)0xf400f06c = 0x004a02dc;
	  *(unsigned int*)0xf400f070 = 0x1e2f0200;
	  *(unsigned int*)0xf400f074 = 0x00001fd1;
	  *(unsigned int*)0xf400f080 = 0x01000800;
	  *(unsigned int*)0xf400f084 = 0x00000800;
	  *(unsigned int*)0xf400f094 = 0x00000001;
	  *(unsigned int*)0xf400f0a8 = 0x00000000;
	  
	  *(unsigned int*)0xf400ff00 = 0x00000041;


    	   break;
    	}
    	case HDMI_VIDEO_1920x1080p_50Hz:
    	{
      
	  *(unsigned int*)0xf400f000 = 0x00000001;
	  *(unsigned int*)0xf400f004 = 0x00000000;
	  *(unsigned int*)0xf400f008 = 0x00000001;
	  *(unsigned int*)0xf400f00c = 0x00000000;
	  *(unsigned int*)0xf400f010 = 0x00000000;
	  *(unsigned int*)0xf400f014 = 0x82000200;
	  *(unsigned int*)0xf400f018 = 0x04380780;
	  *(unsigned int*)0xf400f01c = 0x0000002c;
	  *(unsigned int*)0xf400f020 = 0x02100094;
	  *(unsigned int*)0xf400f024 = 0x00000005;
	  *(unsigned int*)0xf400f028 = 0x00040024;
	  *(unsigned int*)0xf400f02c = 0x00000000;
	  *(unsigned int*)0xf400f030 = 0x00000000;
	  *(unsigned int*)0xf400f034 = 0x00000000;
	  *(unsigned int*)0xf400f038 = 0x00000000;
	  *(unsigned int*)0xf400f03c = 0x00000000;
	  *(unsigned int*)0xf400f040 = 0x00000000;
	  *(unsigned int*)0xf400f05c = 0x00000003;
	  
	  *(unsigned int*)0xf400f064 = 0x1e751f8b;
	  *(unsigned int*)0xf400f068 = 0x00da0200;
	  *(unsigned int*)0xf400f06c = 0x004a02dc;
	  *(unsigned int*)0xf400f070 = 0x1e2f0200;
	  *(unsigned int*)0xf400f074 = 0x00001fd1;
	  *(unsigned int*)0xf400f080 = 0x01000800;
	  *(unsigned int*)0xf400f084 = 0x00000800;
	  *(unsigned int*)0xf400f094 = 0x00000001;
	  *(unsigned int*)0xf400f0a8 = 0x00000000;
	  
	  *(unsigned int*)0xf400ff00 = 0x00000041;

    	   break;
    	}
    	case HDMI_VIDEO_1920x1080i_60Hz:
    	{
      
	  *(unsigned int*)0xf400f000 = 0x00000001;
	  *(unsigned int*)0xf400f004 = 0x00000000;
	  *(unsigned int*)0xf400f008 = 0x00000001;
	  *(unsigned int*)0xf400f00c = 0x00000000;
	  *(unsigned int*)0xf400f010 = 0x00000004;
	  *(unsigned int*)0xf400f014 = 0x82000200;
	  *(unsigned int*)0xf400f018 = 0x021c0780;
	  *(unsigned int*)0xf400f01c = 0x0000002c;
	  *(unsigned int*)0xf400f020 = 0x00580094;
	  *(unsigned int*)0xf400f024 = 0x00000005;
	  *(unsigned int*)0xf400f028 = 0x0002000f;
	  *(unsigned int*)0xf400f02c = 0x00000005;
	  *(unsigned int*)0xf400f030 = 0x0102010f;
	  *(unsigned int*)0xf400f034 = 0x00000000;
	  *(unsigned int*)0xf400f038 = 0x00000000;
	  *(unsigned int*)0xf400f03c = 0x00000000;
	  *(unsigned int*)0xf400f040 = 0x00000000;
	  *(unsigned int*)0xf400f05c = 0x00000003;
	  *(unsigned int*)0xf400ff00 = 0x00000041;
	  
	  *(unsigned int*)0xf400f080 = 0x00000000;
	  *(unsigned int*)0xf400f084 = 0x00000000;
	  *(unsigned int*)0xf400f094 = 0x00000000;

    	   break;
    	}
    	case HDMI_VIDEO_1920x1080i_50Hz:
    	{
      
	  *(unsigned int*)0xf400f000 = 0x00000001;
	  *(unsigned int*)0xf400f004 = 0x00000000;
	  *(unsigned int*)0xf400f008 = 0x00000001;
	  *(unsigned int*)0xf400f00c = 0x00000000;
	  *(unsigned int*)0xf400f010 = 0x00000004;
	  *(unsigned int*)0xf400f014 = 0x82000200;
	  *(unsigned int*)0xf400f018 = 0x021c0780;
	  *(unsigned int*)0xf400f01c = 0x0000002c;
	  *(unsigned int*)0xf400f020 = 0x02100094;
	  *(unsigned int*)0xf400f024 = 0x00000005;
	  *(unsigned int*)0xf400f028 = 0x0002000f;
	  *(unsigned int*)0xf400f02c = 0x00000005;
	  *(unsigned int*)0xf400f030 = 0x0102010f;
	  *(unsigned int*)0xf400f034 = 0x00000000;
	  *(unsigned int*)0xf400f038 = 0x00000000;
	  *(unsigned int*)0xf400f03c = 0x00000000;
	  *(unsigned int*)0xf400f040 = 0x00000000;
	  *(unsigned int*)0xf400f05c = 0x00000003;
	  *(unsigned int*)0xf400ff00 = 0x00000041;
	  
	  *(unsigned int*)0xf400f080 = 0x00000000;
	  *(unsigned int*)0xf400f084 = 0x00000000;
	  *(unsigned int*)0xf400f094 = 0x00000000;


    	   break;
    	}		
		case HDMI_VIDEO_1920x1080i3d_60Hz:	
    				{}

		case HDMI_VIDEO_1920x1080i3d_50Hz:	
    			{}
      case HDMI_VIDEO_1280x720p3d_60Hz:
	  {
	
	*(unsigned int*)0xf400f000 = 0x00000001;
	*(unsigned int*)0xf400f004 = 0x00000000;
	*(unsigned int*)0xf400f008 = 0x00000001;
	*(unsigned int*)0xf400f00c = 0x00000000;
	*(unsigned int*)0xf400f010 = 0x000000a8;
	*(unsigned int*)0xf400f014 = 0x82000200;
	*(unsigned int*)0xf400f018 = 0x02d00500;
	*(unsigned int*)0xf400f01c = 0x00000028;
	*(unsigned int*)0xf400f020 = 0x006e00dc;
	*(unsigned int*)0xf400f024 = 0x00000005;
	*(unsigned int*)0xf400f028 = 0x00050014;
	*(unsigned int*)0xf400f02c = 0x00000000;
	*(unsigned int*)0xf400f030 = 0x00000000;
	*(unsigned int*)0xf400f034 = 0x00000005;
	*(unsigned int*)0xf400f038 = 0x00050014;
	*(unsigned int*)0xf400f03c = 0x00000000;
	*(unsigned int*)0xf400f040 = 0x00000000;
	*(unsigned int*)0xf400f05c = 0x00000003;
	*(unsigned int*)0xf400ff00 = 0x00000041;

	*(unsigned int*)0xf400f064 = 0x1e751f8b;
	*(unsigned int*)0xf400f068 = 0x00da0200;
	*(unsigned int*)0xf400f06c = 0x004a02dc;
	*(unsigned int*)0xf400f070 = 0x1e2f0200;
	*(unsigned int*)0xf400f074 = 0x00001fd1;
	*(unsigned int*)0xf400f080 = 0x01000800;
	*(unsigned int*)0xf400f084 = 0x00000800;
	*(unsigned int*)0xf400f094 = 0x00000001;
	*(unsigned int*)0xf400f0a8 = 0x00000020;	 
		 break;
	  }		
      case HDMI_VIDEO_1280x720p3d_50Hz:
	  	  	{}
      case HDMI_VIDEO_1920x1080p3d_24Hz:
	  {
	*(unsigned int*)0xf400f000 = 0x00000001;
	*(unsigned int*)0xf400f004 = 0x00000000;
	*(unsigned int*)0xf400f008 = 0x00000001;
	*(unsigned int*)0xf400f00c = 0x00000000;
	*(unsigned int*)0xf400f010 = 0x000000a8;
	*(unsigned int*)0xf400f014 = 0x82000200;
	*(unsigned int*)0xf400f018 = 0x04380780;
	*(unsigned int*)0xf400f01c = 0x0000002c;
	*(unsigned int*)0xf400f020 = 0x027e0094;
	*(unsigned int*)0xf400f024 = 0x00000005;
	*(unsigned int*)0xf400f028 = 0x00040024;
	*(unsigned int*)0xf400f02c = 0x00000000;
	*(unsigned int*)0xf400f030 = 0x00000000;
	*(unsigned int*)0xf400f034 = 0x00000005;
	*(unsigned int*)0xf400f038 = 0x00040024;
	*(unsigned int*)0xf400f03c = 0x00000000;
	*(unsigned int*)0xf400f040 = 0x00000000;
	*(unsigned int*)0xf400f05c = 0x00000003;
	*(unsigned int*)0xf400ff00 = 0x00000041;

	*(unsigned int*)0xf400f064 = 0x1e751f8b;
	*(unsigned int*)0xf400f068 = 0x00da0200;
	*(unsigned int*)0xf400f06c = 0x004a02dc;
	*(unsigned int*)0xf400f070 = 0x1e2f0200;
	*(unsigned int*)0xf400f074 = 0x00001fd1;
	*(unsigned int*)0xf400f080 = 0x01000800;
	*(unsigned int*)0xf400f084 = 0x00000800;
	*(unsigned int*)0xf400f094 = 0x00000001;
	*(unsigned int*)0xf400f0a8 = 0x00000020; 
		 break;
	  }	  
     case HDMI_VIDEO_1920x1080p3d_23Hz:
	  	  	{}

	  default:
	  	break;
	 }

     mdelay(10);
     *((volatile unsigned int *)(0xf400f004)) = (1);
     mdelay(40);
     *((volatile unsigned int *)(0xf400f004)) = (0);

}


static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	int r = 0;
    hdmi_device_write w_info;
	hdmi_hdcp_key key;
	send_slt_data send_sltdata;
	CEC_SLT_DATA get_sltdata;
    hdmi_para_setting data_info;
	HDMI_EDID_INFO_T pv_get_info;
	CEC_FRAME_DESCRIPTION cec_frame;
	CEC_ADDRESS cecaddr;
	CEC_DRV_ADDR_CFG_T cecsetAddr; 
	CEC_SEND_MSG_T cecsendframe;
	READ_REG_VALUE regval;
	HDMIDRV_AUDIO_PARA audio_para;
	HDMI_LOG("[HDMI] hdmi ioctl= %s(%d), arg = %lu\n", _hdmi_ioctl_spy(cmd),cmd&0xff, arg);

    switch(cmd)
    {
	   case MTK_HDMI_AUDIO_SETTING:
       {
           if (copy_from_user(&audio_para, (void __user *)arg, sizeof(audio_para))) {
               HDMI_LOG("copy_from_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           } else {
               hdmi_drv->audiosetting(&audio_para);
           }            
           break;
       }	   
       case MTK_HDMI_WRITE_DEV:
       {
           if (copy_from_user(&w_info, (void __user *)arg, sizeof(w_info))) {
               HDMI_LOG("copy_from_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           } else {
               hdmi_drv->write(w_info.u4Addr, w_info.u4Data);
           }            
           break;
       }

	   case MTK_HDMI_INFOFRAME_SETTING:
       {
           if (copy_from_user(&data_info, (void __user *)arg, sizeof(data_info))) {
               HDMI_LOG("copy_from_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           } else {
               hdmi_drv->InfoframeSetting(data_info.u4Data1 & 0xFF, data_info.u4Data2 & 0xFF);
           }            
           break;
       }
	   
	   case MTK_HDMI_HDCP_KEY:
       {
           if (copy_from_user(&key, (void __user *)arg, sizeof(key))) {
               HDMI_LOG("copy_from_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           } else {
               hdmi_drv->hdcpkey((UINT8*)&key);
           }            
           break;
       }
	   
	   case MTK_HDMI_SETLA:
       {
           if (copy_from_user(&cecsetAddr, (void __user *)arg, sizeof(cecsetAddr))) {
               HDMI_LOG("copy_from_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           } else {
               hdmi_drv->setcecla(&cecsetAddr);
           }            
           break;
       }
	   
	   case MTK_HDMI_SENDSLTDATA:
       {
           if (copy_from_user(&send_sltdata, (void __user *)arg, sizeof(send_sltdata))) {
               HDMI_LOG("copy_from_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           } else {
               hdmi_drv->sendsltdata((UINT8*)&send_sltdata);
           }            
           break;
       }	
	   
	   case MTK_HDMI_SET_CECCMD:
       {
           if (copy_from_user(&cecsendframe, (void __user *)arg, sizeof(cecsendframe))) {
               HDMI_LOG("copy_from_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           } else {
               hdmi_drv->setceccmd(&cecsendframe);
           }            
           break;
       }	 

	   case MTK_HDMI_CEC_ENABLE:
	   {
		   hdmi_drv->cecenable(arg & 0xFF); 	
		   break;
	   }

	   
	   case MTK_HDMI_GET_EDID:
       {
	   	   hdmi_drv->getedid(&pv_get_info);
           if (copy_to_user((void __user *)arg, &pv_get_info, sizeof(pv_get_info))) {
               HDMI_LOG("copy_to_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           }           
           break;
       }
	   
	   case MTK_HDMI_GET_CECCMD:
       {
	   	   hdmi_drv->getceccmd(&cec_frame);
           if (copy_to_user((void __user *)arg, &cec_frame, sizeof(cec_frame))) {
               HDMI_LOG("copy_to_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           }           
           break;
       }
	   
	   case MTK_HDMI_GET_SLTDATA:
       {
	   	   hdmi_drv->getsltdata(&get_sltdata);
           if (copy_to_user((void __user *)arg, &get_sltdata, sizeof(get_sltdata))) {
               HDMI_LOG("copy_to_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           }           
           break;
       }

	   case MTK_HDMI_GET_CECADDR:
       {
	   	   hdmi_drv->getcecaddr(&cecaddr);
           if (copy_to_user((void __user *)arg, &cecaddr, sizeof(cecaddr))) {
               HDMI_LOG("copy_to_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           }       
           break;
       }

	   case MTK_HDMI_COLOR_DEEP:
       {
           if (copy_from_user(&data_info, (void __user *)arg, sizeof(data_info))) {
               HDMI_LOG("copy_from_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
           } else {
               hdmi_drv->colordeep(data_info.u4Data1 & 0xFF, data_info.u4Data2 & 0xFF);
           }
		   hdmi_colorspace = (unsigned char)data_info.u4Data1;
           break;
       }
	   
       case MTK_HDMI_READ_DEV:
       {
           if (copy_from_user(&regval, (void __user *)arg, sizeof(regval))) {
           HDMI_LOG("copy_from_user failed! line:%d \n", __LINE__);
           r = -EFAULT;
           } else {
              hdmi_drv->read(regval.u1adress, &regval.pu1Data);    
           }
		   
           if (copy_to_user((void __user *)arg, &regval, sizeof(regval))) {
           HDMI_LOG("copy_to_user failed! line:%d \n", __LINE__);
           r = -EFAULT;
           }   
           break;
       }

	   case MTK_HDMI_ENABLE_LOG:
       {
           hdmi_drv->log_enable(arg & 0xFFFF);     
           break;
       }
	   
	   case MTK_HDMI_ENABLE_HDCP:
       {
           hdmi_drv->enablehdcp(arg & 0xFFFF);     
           break;
       }
	   
	   case MTK_HDMI_CECRX_MODE:
       {
           hdmi_drv->setcecrxmode(arg & 0xFFFF);     
           break;
       }

	   case MTK_HDMI_STATUS:
       {
           hdmi_drv->hdmistatus();     
           break;
       }

	   case MTK_HDMI_CHECK_EDID:
       {
           hdmi_drv->checkedid(arg & 0xFF);     
           break;
       }
	   
       case MTK_HDMI_AUDIO_VIDEO_ENABLE:
       {
            if (arg)
            {
 

                //HDMI_CHECK_RET(hdmi_drv_init());
                //hdmi_dpi_config_clock(); // configure dpi clock
                //hdmi_dpi_power_switch(false);   // but dpi power is still off
               // if(hdmi_drv->enter) hdmi_drv->enter();
                hdmi_power_on();
                p->is_enabled = true;
            }
             else
            {
   

                //when disable hdmi, HPD is disabled
                //switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);

                //hdmi_dst_display_path_config(false);

                hdmi_power_off();

                //wait hdmi finish update

                p->is_enabled = false;
            }           

            break;
        }
		
        case MTK_HDMI_POWER_ENABLE:
        {
			RETIF(!p->is_enabled, 0);

            if (arg)
            {
                hdmi_power_on();
            }
            else
            {
                switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
                hdmi_power_off();
            }
            break;
        }
        case MTK_HDMI_AUDIO_ENABLE:
        {
			RETIF(!p->is_enabled, 0);

            if (arg)
            {
                HDMI_CHECK_RET(hdmi_audio_enable(true));
            }
            else
            {
                HDMI_CHECK_RET(hdmi_audio_enable(false));
            }

            break;
        }
        case MTK_HDMI_VIDEO_ENABLE:
        {
			RETIF(!p->is_enabled, 0);
            break;
        }
        case MTK_HDMI_VIDEO_CONFIG:
        {
            HDMI_LOG("video resolution configuration, arg=%ld\n", arg);


            RETIF(!p->is_enabled, 0);

            RETIF(IS_HDMI_NOT_ON(),0);

            if(down_interruptible(&hdmi_update_mutex))
            {
                HDMI_LOG("[HDMI] can't get semaphore in\n");
                return EAGAIN;
            }

            //hdmi_dst_display_path_config(false);
            //hdmi_src_display_path_config(false);
            //hdmi_free_hdmi_buffer();
			hdmi_drv->tmdsonoff(0);

			//FIXME, arg => vin format
            hdmi_config_pll(arg);

            dpi_setting_res((unsigned char)arg);
            hdmi_video_config(arg, HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);
			//vMhlSetDigital(arg);

            //DPI_CHECK_RET(HDMI_DPI(_DisableClk)());
    	    //DPI_CHECK_RET(HDMI_DPI(_ConfigHDMI)());
            //hdmi_dpi_config_update();
            //DPI_CHECK_RET(HDMI_DPI(_EnableClk)());
            p->is_clock_on = true;

            up(&hdmi_update_mutex);
            break;
        }
        case MTK_HDMI_AUDIO_CONFIG:
        {
			RETIF(!p->is_enabled, 0);

            break;
        }
        case MTK_HDMI_IS_FORCE_AWAKE:
        {
            if (!hdmi_drv_init_context())
	        {
	            printk("[hdmi]%s, hdmi_drv_init_context fail\n", __func__);
		        return HDMI_STATUS_NOT_IMPLEMENTED;
	        }
            r = copy_to_user(argp, &hdmi_params->is_force_awake, sizeof(hdmi_params->is_force_awake)) ? -EFAULT : 0;
            break;
        }
        case MTK_HDMI_FACTORY_MODE_ENABLE:
        {
			if (hdmi_drv->power_on())
            {
                r = -EAGAIN;
                HDMI_LOG("Error factory mode test fail\n");
            }
            else
            {
                HDMI_LOG("before power off\n");
                hdmi_drv->power_off();
                HDMI_LOG("after power off\n");
            }
            break;
        }
        default:
        {
            printk("[hdmi][HDMI] arguments error\n");
            break;
        }
    }

	return r;
}


//VIDEO MODE
#if 1
void dump_video_buffer(char* banner)
{
#if 0
    struct hdmi_video_buffer_list *buffer_list;
    struct list_head *temp;
    HDMI_LOG("before add a node,list node Addr:\n");
    list_for_each(temp, hdmi_video_buffer_list_head)
    {
        buffer_list = list_entry(temp, struct hdmi_video_buffer_list, list);
        HDMI_LOG("0x%08x,    ", (unsigned int)temp);
    }
    HDMI_LOG("\n");
#else
    struct hdmi_video_buffer_list *temp;
    HDMI_LOG("%s\n", banner);
    list_for_each_entry(temp, hdmi_video_buffer_list_head, list)
    {
        HDMI_LOG("0x%x,    ", (unsigned int)temp);
    }
    HDMI_LOG("\n");
#endif
}
static int hdmi_add_video_buffer(struct hdmi_video_buffer_info *buffer_info, struct file *file)
{}
static struct hdmi_video_buffer_list* hdmi_search_video_buffer(struct hdmi_video_buffer_info *buffer_info, struct file *file)
{}

static void hdmi_destory_video_buffer(void)
{}
#endif

static int hdmi_remove(struct platform_device *pdev)
{
	return 0;
}

static BOOL hdmi_drv_init_context(void)
{
	static const HDMI_UTIL_FUNCS hdmi_utils =
	{
		.udelay             	= hdmi_udelay,
		.mdelay             	= hdmi_mdelay,
		.state_callback			= hdmi_state_callback,
	};

	if (hdmi_drv != NULL)
		return TRUE;


    hdmi_drv = (HDMI_DRIVER*)HDMI_GetDriver();

	if (NULL == hdmi_drv) return FALSE;

	hdmi_drv->set_util_funcs(&hdmi_utils);
	hdmi_drv->get_params(hdmi_params);

	return TRUE;
}

static void __exit hdmi_exit(void)
{
	device_destroy(hdmi_class, hdmi_devno);
	class_destroy(hdmi_class);
	cdev_del(hdmi_cdev);
	unregister_chrdev_region(hdmi_devno, 1);

}

struct file_operations hdmi_fops = {
	.owner   = THIS_MODULE,
	.unlocked_ioctl   = hdmi_ioctl,
	.open    = hdmi_open,
	.release = hdmi_release,
};

static int hdmi_probe(struct platform_device *pdev)
{
    int ret = 0;
	struct class_device *class_dev = NULL;

    printk("[hdmi]%s\n", __func__);

    /* Allocate device number for hdmi driver */
	ret = alloc_chrdev_region(&hdmi_devno, 0, 1, HDMI_DEVNAME);
	if(ret)
	{
		printk("[hdmi]alloc_chrdev_region fail\n");
		return -1;
	}

    /* For character driver register to system, device number binded to file operations */
	hdmi_cdev = cdev_alloc();
	hdmi_cdev->owner = THIS_MODULE;
	hdmi_cdev->ops = &hdmi_fops;
	ret = cdev_add(hdmi_cdev, hdmi_devno, 1);

	/* For device number binded to device name(hdmitx), one class is corresponeded to one node */
	hdmi_class = class_create(THIS_MODULE, HDMI_DEVNAME);
	/* mknod /dev/hdmitx */
	class_dev = (struct class_device *)device_create(hdmi_class, NULL, hdmi_devno, NULL, HDMI_DEVNAME);

	printk("[hdmi][%s] current=0x%08x\n", __func__, (unsigned int)current);

    if (!hdmi_drv_init_context())
	{
	    printk("[hdmi]%s, hdmi_drv_init_context fail\n", __func__);
		return HDMI_STATUS_NOT_IMPLEMENTED;
	}

    init_waitqueue_head(&hdmi_video_mode_wq);
    INIT_LIST_HEAD(&hdmi_video_mode_buffer_list);

    init_waitqueue_head(&hdmi_update_wq);
    hdmi_update_task = kthread_create(hdmi_update_kthread, NULL, "hdmi_update_kthread");
    wake_up_process(hdmi_update_task);

    init_waitqueue_head(&hdmi_overlay_config_wq);
    hdmi_overlay_config_task = kthread_create(hdmi_overlay_config_kthread, NULL, "hdmi_overlay_config_kthread");
    wake_up_process(hdmi_overlay_config_task);

    init_waitqueue_head(&hdmi_rdma_config_wq);

    return 0;
}

static struct platform_driver hdmi_driver = {
    .probe  = hdmi_probe,
    .remove = hdmi_remove,
    .driver = { .name = HDMI_DEVNAME }
};

static int __init hdmi_init(void)
{
    int ret = 0;
    printk("[hdmi]%s\n", __func__);


    if (platform_driver_register(&hdmi_driver))
    {
        printk("[hdmi]failed to register mtkfb driver\n");
        return -1;
    }

    memset((void*)&hdmi_context, 0, sizeof(_t_hdmi_context));
    SET_HDMI_OFF();

    init_hdmi_mmp_events();

    if (!hdmi_drv_init_context())
    {
        printk("[hdmi]%s, hdmi_drv_init_context fail\n", __func__);
        return HDMI_STATUS_NOT_IMPLEMENTED;
    }

    p->output_mode = hdmi_params->output_mode;
    hdmi_drv->init();
    HDMI_LOG("Output mode is %s\n", (hdmi_params->output_mode==HDMI_OUTPUT_MODE_DPI_BYPASS)?"HDMI_OUTPUT_MODE_DPI_BYPASS":"HDMI_OUTPUT_MODE_LCD_MIRROR");

    if(hdmi_params->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS)
    {
        p->output_video_resolution = HDMI_VIDEO_RESOLUTION_NUM;
    }


    hdmi_switch_data.name = "hdmi";
    hdmi_switch_data.index = 0;
    hdmi_switch_data.state = NO_DEVICE;

    // for support hdmi hotplug, inform AP the event
    ret = switch_dev_register(&hdmi_switch_data);
    if(ret)
    {
        printk("[hdmi][HDMI]switch_dev_register returned:%d!\n", ret);
        return 1;
    }

    return 0;
}

module_init(hdmi_init);
module_exit(hdmi_exit);
MODULE_AUTHOR("Xuecheng, Zhang <xuecheng.zhang@mediatek.com>");
MODULE_DESCRIPTION("HDMI Driver");
MODULE_LICENSE("GPL");

