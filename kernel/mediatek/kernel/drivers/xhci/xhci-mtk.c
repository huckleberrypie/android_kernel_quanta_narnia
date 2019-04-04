#include <xhci.h>
#include <linux/xhci/xhci-mtk.h>
#include <linux/xhci/xhci-mtk-power.h>
#include <linux/xhci/xhci-mtk-scheduler.h>
#include <linux/kernel.h>       /* printk() */
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#ifdef CONFIG_USB_MTK_DUALMODE
#include <mach/eint.h>
#include <linux/switch.h>
#include <linux/sched.h>
#include <linux/module.h>
#ifdef CONFIG_PROJECT_PHY
#include <linux/mu3phy/mtk-phy-asic.h>
#endif
#endif
#ifdef CONFIG_MTK_LDVT
#undef DRV_Reg32
#undef DRV_WriteReg32
#include <mach/mt_gpio.h>
#endif

#define mtk_xhci_mtk_log(fmt, args...) \
    printk("%s(%d): " fmt, __func__, __LINE__, ##args)

#define RET_SUCCESS 0
#define RET_FAIL 1

struct xhci_hcd	*mtk_xhci = NULL;

static struct wake_lock mtk_xhci_wakelock;

#ifdef CONFIG_USB_MTK_DUALMODE
#define IDPIN_IN MT_EINT_POL_NEG
#define IDPIN_OUT MT_EINT_POL_POS

static bool mtk_idpin_cur_stat = IDPIN_OUT;
static struct switch_dev mtk_otg_state;

static struct delayed_work mtk_xhci_delaywork;
int mtk_iddig_debounce = 400;
module_param(mtk_iddig_debounce, int, 0644);


static void mtk_set_iddig_out_detect(void){
    mt_eint_set_polarity(IDDIG_EINT_PIN, MT_EINT_POL_POS);
	mt_eint_unmask(IDDIG_EINT_PIN);
}

static void mtk_set_iddig_in_detect(void){
    mt_eint_set_polarity(IDDIG_EINT_PIN, MT_EINT_POL_NEG);
	mt_eint_unmask(IDDIG_EINT_PIN);
}

#if 0
void xhci_regs_print(void){
    mtk_xhci_mtk_log("0xf1270000, (0x%x)\n", readl((__le32 __iomem *)0xf1270000));
    mtk_xhci_mtk_log("0xf1270420, (0x%x)\n", readl((__le32 __iomem *)0xf1270420));
    mtk_xhci_mtk_log("0xf1280750, (0x%x)\n", readl((__le32 __iomem *)0xf1280750));
}
#endif

static int mtk_xhci_hcd_init(void)
{
	int retval;

    mtk_xhci_ip_init();

	retval = xhci_register_plat();
	if (retval < 0) {
		printk(KERN_DEBUG "Problem registering platform driver.");
		return retval;
	}
    retval = xhci_attrs_init();
    if(retval < 0){
        printk(KERN_DEBUG "Problem creating xhci attributes.");
        goto unreg_plat;
    }
	/*
	 * Check the compiler generated sizes of structures that must be laid
	 * out in specific ways for hardware access.
	 */
	BUILD_BUG_ON(sizeof(struct xhci_doorbell_array) != 256*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_slot_ctx) != 8*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_ep_ctx) != 8*32/8);
	/* xhci_device_control has eight fields, and also
	 * embeds one xhci_slot_ctx and 31 xhci_ep_ctx
	 */
	BUILD_BUG_ON(sizeof(struct xhci_stream_ctx) != 4*32/8);
	BUILD_BUG_ON(sizeof(union xhci_trb) != 4*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_erst_entry) != 4*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_cap_regs) != 7*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_intr_reg) != 8*32/8);
	/* xhci_run_regs has eight fields and embeds 128 xhci_intr_regs */
	BUILD_BUG_ON(sizeof(struct xhci_run_regs) != (8+8*128)*32/8);
	return 0;

unreg_plat:
    xhci_unregister_plat();
	return retval;
}

static void mtk_xhci_hcd_cleanup(void)
{
	xhci_unregister_plat();
    xhci_attrs_exit();
}

static void mtk_xhci_imod_set(u32 imod){
	u32 temp;

    temp = xhci_readl(mtk_xhci, &mtk_xhci->ir_set->irq_control);
	temp &= ~0xFFFF;
	temp |= imod;
	xhci_writel(mtk_xhci, temp, &mtk_xhci->ir_set->irq_control);
}

static int mtk_xhci_driver_load(void){
    int ret = 0;

    /* recover clock/power setting and deassert reset bit of mac */
    usb_phy_recover(0);
	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL) & (~SSUSB_IP_SW_RST), (void __iomem *)SSUSB_IP_PW_CTRL);
    ret = mtk_xhci_hcd_init();
    if(ret)
        goto _err;

    /* for performance, fixed the interrupt moderation from 0xA0(default) to 0x30 */
    mtk_xhci_imod_set(0x30);
    enableXhciAllPortPower(mtk_xhci);

    return 0;

_err:
    mtk_xhci_mtk_log("ret(%d)\n", ret);
    writel(readl((void __iomem *)SSUSB_IP_PW_CTRL) & (SSUSB_IP_SW_RST), (void __iomem *)SSUSB_IP_PW_CTRL);
    usb_phy_savecurrent(1);
    return ret;
}

static void mtk_xhci_driver_unload(void){
    disableXhciAllPortPower(mtk_xhci);
    mtk_xhci_hcd_cleanup();
    writel(readl((void __iomem *)SSUSB_IP_PW_CTRL) | (SSUSB_IP_SW_RST), (void __iomem *)SSUSB_IP_PW_CTRL);
    /* close clock/power setting and assert reset bit of mac */
    usb_phy_savecurrent(1);
}

void mtk_xhci_switch_init(void){
    mtk_otg_state.name = "otg_state";
	mtk_otg_state.index = 0;
	mtk_otg_state.state = 0;

	if(switch_dev_register(&mtk_otg_state))
		mtk_xhci_mtk_log("switch_dev_register fail\n");
	else
        mtk_xhci_mtk_log("switch_dev register success\n");
}

void mtk_xhci_mode_switch(struct work_struct *work){
    static bool is_load = false;
    int ret = 0;

    if(mtk_idpin_cur_stat == IDPIN_OUT){
        /* make id pin to detect the plug-out */
        mtk_set_iddig_out_detect();
        /* expect next isr is for id-pin out action */
        mtk_idpin_cur_stat = IDPIN_IN;

        is_load = false;
        ret = mtk_xhci_driver_load();
        if(!ret){
            is_load = true;
            mtk_xhci_wakelock_lock();
            switch_set_state(&mtk_otg_state, 1);
        }
    }
    else{ /* IDPIN_OUT */
        /* make id pin to detect the plug-in */
        mtk_set_iddig_in_detect();
        /* expect next isr is for id-pin in action */
        mtk_idpin_cur_stat = IDPIN_OUT;

        if(is_load){
            //mtk_switch2device(false);
            mtk_xhci_driver_unload();
            is_load = false;

            switch_set_state(&mtk_otg_state, 0);
            mtk_xhci_wakelock_unlock();

       }
    }

    mtk_xhci_mtk_log("current mode is %s, ret(%d), switch(%d)\n",
                        (mtk_idpin_cur_stat == IDPIN_IN)? "host": "device",
                        ret, mtk_otg_state.state);
}

static void xhci_eint_iddig_isr(void){
    int ret;
    //schedule_delayed_work(&mtk_xhci_delay_work, mtk_iddig_debounce*HZ/1000);
    ret = schedule_delayed_work_on(0, &mtk_xhci_delaywork, 0);
    mtk_xhci_mtk_log("schedule to delayed work, ret(%d)\n", ret);
}

void mtk_xhci_eint_iddig_init(void){
    INIT_DELAYED_WORK(&mtk_xhci_delaywork, mtk_xhci_mode_switch);

    mt_eint_set_sens(IDDIG_EINT_PIN, MT_LEVEL_SENSITIVE);
	mt_eint_set_hw_debounce(IDDIG_EINT_PIN,64);
	mt_eint_registration(IDDIG_EINT_PIN, EINTF_TRIGGER_LOW, xhci_eint_iddig_isr, false);
    mtk_xhci_mtk_log("external iddig register done.\n");

    /* set in-detect and umask the iddig interrupt */
    mtk_set_iddig_in_detect();
}

#endif

void mtk_xhci_wakelock_init(void){
    wake_lock_init(&mtk_xhci_wakelock, WAKE_LOCK_SUSPEND, "xhci.wakelock");
}

void mtk_xhci_wakelock_lock(void){
    if(!wake_lock_active(&mtk_xhci_wakelock))
        wake_lock(&mtk_xhci_wakelock);
    mtk_xhci_mtk_log("done\n");
}

void mtk_xhci_wakelock_unlock(void){
    if(wake_lock_active(&mtk_xhci_wakelock))
        wake_unlock(&mtk_xhci_wakelock);
    mtk_xhci_mtk_log("done\n");
}

void mtk_xhci_set(struct xhci_hcd *xhci){
    mtk_xhci_mtk_log("mtk_xhci = 0x%x\n", (unsigned int)xhci);
    mtk_xhci = xhci;
}

bool mtk_is_host_mode(void){
    return (mtk_idpin_cur_stat == IDPIN_IN)? true: false;
}

void mtk_xhci_ck_timer_init(void){
	__u32 __iomem *addr;
	u32 temp = 0;
	int num_u3_port;

    num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP));
    if(num_u3_port ){
    	//set MAC reference clock speed
    	addr = (__u32 __iomem *)(SSUSB_U3_MAC_BASE+U3_UX_EXIT_LFPS_TIMING_PAR);
    	temp = readl(addr);
    	temp &= ~(0xff << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
    	temp |= (U3_RX_UX_EXIT_LFPS_REF << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
    	writel(temp, addr);
    	addr = (__u32 __iomem *)(SSUSB_U3_MAC_BASE+U3_REF_CK_PAR);
    	temp = readl(addr);
    	temp &= ~(0xff);
    	temp |= U3_REF_CK_VAL;
    	writel(temp, addr);

    	//set SYS_CK
    	addr = (__u32 __iomem *)(SSUSB_U3_SYS_BASE+U3_TIMING_PULSE_CTRL);
    	temp = readl(addr);
    	temp &= ~(0xff);
    	temp |= MTK_CNT_1US_VALUE;
    	writel(temp, addr);
    }

	addr = (__u32 __iomem *)(SSUSB_U2_SYS_BASE+USB20_TIMING_PARAMETER);
	temp &= ~(0xff);
	temp |= MTK_TIME_VALUE_1US;
	writel(temp, addr);

    if(num_u3_port){
    	//set LINK_PM_TIMER=3
    	addr = (__u32 __iomem *)(SSUSB_U3_SYS_BASE+LINK_PM_TIMER);
    	temp = readl(addr);
    	temp &= ~(0xf);
    	temp |= MTK_PM_LC_TIMEOUT_VALUE;
    	writel(temp, addr);
    }
}

static void setLatchSel(void){
	__u32 __iomem *latch_sel_addr;
	u32 latch_sel_value;
	int num_u3_port;

    num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP));
    if(num_u3_port <= 0)
        return;

	latch_sel_addr = (__u32 __iomem *)U3_PIPE_LATCH_SEL_ADD;
	latch_sel_value = ((U3_PIPE_LATCH_TX)<<2) | (U3_PIPE_LATCH_RX);
	writel(latch_sel_value, latch_sel_addr);
}

#ifndef CONFIG_USB_MTK_DUALMODE
static int mtk_xhci_phy_init(int argc, char**argv)
{
	/* initialize PHY related data structure */
	if (!u3phy_ops)
		u3phy_init();

    /* USB 2.0 slew rate calibration */
	if(u3phy_ops->u2_slew_rate_calibration)
		u3phy_ops->u2_slew_rate_calibration(u3phy);
	else
		printk(KERN_ERR "WARN: PHY doesn't implement u2 slew rate calibration function\n");

    /* phy initialization */
	if(u3phy_ops->init(u3phy) != PHY_TRUE)
	    return RET_FAIL;

    printk(KERN_ERR "phy registers and operations initial done\n");
    return RET_SUCCESS;
}
#endif

void mtk_xhci_ip_init(void){
    #ifdef CONFIG_MTK_LDVT
    mt_set_gpio_mode(121, 4);
    #endif

    /* phy initialization is done by device, if target runs on dual mode */
    #ifndef CONFIG_USB_MTK_DUALMODE
    mtk_xhci_phy_init(0, NULL);
    #endif

    /* reset ip, power on host and power on/enable ports */
    #if 0
    #ifndef CONFIG_USB_MTK_DUALMODE
	enableAllClockPower(1); /* host do reset ip */
    #else
    enableAllClockPower(0); /* device do reset ip */
    #endif
    #endif

    enableAllClockPower(0); /* host do reset ip */

	setLatchSel();
    mtk_xhci_ck_timer_init();
	mtk_xhci_scheduler_init();
}

int mtk_xhci_get_port_num(void){
    return SSUSB_U3_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP))
        + SSUSB_U2_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP));
}

