#include <mach/mt_clkmgr.h>
#include <mach/sync_write.h>
#include "mt_smi.h"

#define CHECK_CLOCK_IS_ON 1
unsigned long u4SMILarbBaseAddr[SMI_LARB_NUMBER];
unsigned long u4SMICommBaseAddr[SMI_COMM_NUMBER];

void SMI_Init(void)
{
	u4SMILarbBaseAddr[0] = MET_SMI_LARB0_BASE;
	u4SMILarbBaseAddr[1] = MET_SMI_LARB1_BASE;
	u4SMILarbBaseAddr[2] = MET_SMI_LARB2_BASE;
	u4SMICommBaseAddr[0] = MET_SMI_COM_BASE;
}

#ifdef CHECK_CLOCK_IS_ON
static int SMI_CheckClockOn(unsigned long u4LarbNo)
{

    int retVal = 0;// default off

    switch(u4LarbNo)
    {
        case 0 :
            retVal = ((0 != clock_is_on(MT_CG_DISP0_SMI_LARB0)) || (0 != clock_is_on(MT_CG_DISP0_SMI_COMMON))) ? 1 : 0;
            break;
        case 1 :
            retVal = ((0 != clock_is_on(MT_CG_VDEC1_LARB)) || (0 != clock_is_on(MT_CG_DISP0_SMI_COMMON))) ? 1 : 0;
            break;
        case 2 :
            retVal = ((0 != clock_is_on(MT_CG_IMAGE_LARB2_SMI)) || (0 != clock_is_on(MT_CG_DISP0_SMI_COMMON))) ? 1 : 0;
            break;
        case 3 :
            break;
        default :
        break;
    }
    return retVal;
}
#endif

void SMI_SetSMIBMCfg(unsigned long larbno, unsigned long portno,
                                         unsigned long desttype, unsigned long rwtype)
{
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return;
    }
#endif
	mt65xx_reg_sync_writel(portno , SMI_LARB_MON_PORT(u4SMILarbBaseAddr[larbno]));
	mt65xx_reg_sync_writel( ((rwtype<< 2) | desttype) , SMI_LARB_MON_TYPE(u4SMILarbBaseAddr[larbno]));
}

void SMI_SetMonitorControl (SMIBMCfg_Ext *cfg_ex) {

	int i;
	unsigned long u4RegVal;

	if(cfg_ex != NULL) {
		u4RegVal = (((unsigned long)cfg_ex->uStarvationTime << 16) | ((unsigned long)cfg_ex->bStarvationEn << 15) |
		((unsigned long)cfg_ex->bRequestSelection << 6) |
		((unsigned long)cfg_ex->bMaxPhaseSelection << 5) | ((unsigned long)cfg_ex->bDPSelection << 4) |
		((unsigned long)cfg_ex->uIdleOutStandingThresh << 1) | cfg_ex->bIdleSelection);
//        printk("Ex configuration %lx\n", u4RegVal);
	} else {// default
		u4RegVal = (((unsigned long)8 << 16) | ((unsigned long)1 << 15) |
		((unsigned long)1 << 5) | ((unsigned long)1 << 4) |
		((unsigned long)3 << 1) | 1);
//        printk("default configuration %lx\n", u4RegVal);
	}

	for (i=0; i<SMI_LARB_NUMBER; i++) {
#ifdef CHECK_CLOCK_IS_ON
		if(0 == SMI_CheckClockOn(i))
		{
		       continue;
		}
#endif
		mt65xx_reg_sync_writel(u4RegVal , SMI_LARB_MON_CON(u4SMILarbBaseAddr[i]));
	}

}

void SMI_Disable(unsigned long larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return;
    }
#endif
    mt65xx_reg_sync_writel(0 , SMI_LARB_MON_ENA(u4SMILarbBaseAddr[larbno]));//G2D
}

void SMI_Enable(unsigned long larbno, unsigned long bustype) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return;
    }
#endif
	//if(cfg->bBusType == 0) //GMC
	if( bustype ==0 ) {//GMC
		mt65xx_reg_sync_writel(0x1 , SMI_LARB_MON_ENA(u4SMILarbBaseAddr[larbno]));//G2D
	} else {//AXI
		mt65xx_reg_sync_writel(0x3 , SMI_LARB_MON_ENA(u4SMILarbBaseAddr[larbno]));//GPU
	}
}

void SMI_Comm_Disable(unsigned long commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return;
    }
#endif
	mt65xx_reg_sync_writel(0 , SMI_COMM_MON_ENA(u4SMICommBaseAddr[commonno]));
}

void SMI_Comm_Enable(unsigned long commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return;
    }
#endif
	mt65xx_reg_sync_writel(0x1 , SMI_COMM_MON_ENA(u4SMICommBaseAddr[commonno]));
}

void SMI_Comm_Clear(int commonno)
{
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return;
    }
#endif
//Clear Counter
	mt65xx_reg_sync_writel(1 , SMI_COMM_MON_CLR(u4SMICommBaseAddr[commonno]));
	mt65xx_reg_sync_writel(0 , SMI_COMM_MON_CLR(u4SMICommBaseAddr[commonno]));
}

void SMI_SetCommBMCfg(unsigned long commonno, unsigned long portno,
                      unsigned long desttype, unsigned long rwtype)
{
	unsigned long u4RegVal;
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return;
    }
#endif
	mt65xx_reg_sync_writel( ((rwtype<< 2) | desttype) , SMI_COMM_MON_TYPE(u4SMICommBaseAddr[commonno]));

	u4RegVal = (((unsigned long)portno << 18) |
          ((unsigned long)1 << 4) | ((unsigned long)3 << 1) | 1);
	mt65xx_reg_sync_writel(u4RegVal , SMI_COMM_MON_CON(u4SMICommBaseAddr[commonno]));

}

void SMI_Pause(int larbno)
{
//Pause Counter
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return;
    }
#endif
	mt65xx_reg_sync_writel(0 , SMI_LARB_MON_ENA(u4SMILarbBaseAddr[larbno]));
}

void SMI_Clear(int larbno)
{
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return;
    }
#endif
//Clear Counter
	mt65xx_reg_sync_writel(1 , SMI_LARB_MON_CLR(u4SMILarbBaseAddr[larbno]));
	mt65xx_reg_sync_writel(0 , SMI_LARB_MON_CLR(u4SMILarbBaseAddr[larbno]));
}

//Get SMI result
/* result->u4ActiveCnt = ioread32(MT6575SMI_MON_ACT_CNT(u4SMIBaseAddr));
    result->u4RequestCnt = ioread32(MT6575SMI_MON_REQ_CNT(u4SMIBaseAddr));
    result->u4IdleCnt = ioread32(MT6575SMI_MON_IDL_CNT(u4SMIBaseAddr));
    result->u4BeatCnt = ioread32(MT6575SMI_MON_BEA_CNT(u4SMIBaseAddr));
    result->u4ByteCnt = ioread32(MT6575SMI_MON_BYT_CNT(u4SMIBaseAddr));
    result->u4CommPhaseAccum = ioread32(MT6575SMI_MON_CP_CNT(u4SMIBaseAddr));
    result->u4DataPhaseAccum = ioread32(MT6575SMI_MON_DP_CNT(u4SMIBaseAddr));
    result->u4MaxCommOrDataPhase = ioread32(MT6575SMI_MON_CDP_MAX(u4SMIBaseAddr));
    result->u4MaxOutTransaction = ioread32(MT6575SMI_MON_COS_MAX(u4SMIBaseAddr));
*/


int SMI_GetPortNo(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_PORT(u4SMILarbBaseAddr[larbno]));
}

/* === get counter === */
int SMI_GetActiveCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_ACT_CNT(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetRequestCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_REQ_CNT(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetIdleCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif

	return ioread32(SMI_LARB_MON_IDL_CNT(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetBeatCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_BEA_CNT(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetByteCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_BYT_CNT(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetCPCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_CP_CNT(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetDPCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_DP_CNT(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetCDP_MAX(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_CDP_MAX(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetCOS_MAX(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_COS_MAX(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetBUS_REQ0(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_BUS_REQ0(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetBUS_REQ1(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_BUS_REQ1(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetWDTCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_WDT_CNT(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetRDTCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_RDT_CNT(u4SMILarbBaseAddr[larbno]));
}

int SMI_GetOSTCnt(int larbno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == SMI_CheckClockOn(larbno))
    {
        return 0;
    }
#endif
	return ioread32(SMI_LARB_MON_OST_CNT(u4SMILarbBaseAddr[larbno]));
}


//get common counter
int SMI_Comm_GetActiveCnt(int commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return 0;
    }
#endif

	return ioread32(SMI_COMM_MON_ACT_CNT(u4SMICommBaseAddr[commonno]));
}

int SMI_Comm_GetRequestCnt(int commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return 0;
    }
#endif
	return ioread32(SMI_COMM_MON_REQ_CNT(u4SMICommBaseAddr[commonno]));
}

int SMI_Comm_GetIdleCnt(int commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return 0;
    }
#endif
	return ioread32(SMI_COMM_MON_IDL_CNT(u4SMICommBaseAddr[commonno]));
}

int SMI_Comm_GetBeatCnt(int commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return 0;
    }
#endif
	return ioread32(SMI_COMM_MON_BEA_CNT(u4SMICommBaseAddr[commonno]));
}

int SMI_Comm_GetByteCnt(int commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return 0;
    }
#endif
	return ioread32(SMI_COMM_MON_BYT_CNT(u4SMICommBaseAddr[commonno]));
}

int SMI_Comm_GetCPCnt(int commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return 0;
    }
#endif

	return ioread32(SMI_COMM_MON_CP_CNT(u4SMICommBaseAddr[commonno]));
}

int SMI_Comm_GetDPCnt(int commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return 0;
    }
#endif
	return ioread32(SMI_COMM_MON_DP_CNT(u4SMICommBaseAddr[commonno]));
}

int SMI_Comm_GetCDP_MAX(int commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return 0;
    }
#endif

	return ioread32(SMI_COMM_MON_CDP_MAX(u4SMICommBaseAddr[commonno]));
}

int SMI_Comm_GetCOS_MAX(int commonno) {
#ifdef CHECK_CLOCK_IS_ON
    if(0 == clock_is_on(MT_CG_DISP0_SMI_COMMON))
    {
        return 0;
    }
#endif
	return ioread32(SMI_COMM_MON_COS_MAX(u4SMICommBaseAddr[commonno]));
}

int met_larb_clock_on(int larb_id)
{
    char name[30];
    sprintf(name, "smi+%d", larb_id);

    switch(larb_id)
    {
        case 0:
           enable_clock(MT_CG_DISP0_SMI_LARB0, name);
           enable_clock(MT_CG_DISP0_SMI_COMMON, name);
           break;
        case 1:
           enable_clock(MT_CG_VDEC1_LARB, name);
           break;
        case 2:
           enable_clock(MT_CG_IMAGE_LARB2_SMI, name);
           break;
        default:
            break;
    }

    return 0;
}

int met_larb_clock_off(int larb_id)
{

    char name[30];
    sprintf(name, "smi+%d", larb_id);

    switch(larb_id)
    {
        case 0:
           disable_clock(MT_CG_DISP0_SMI_LARB0, name);
           disable_clock(MT_CG_DISP0_SMI_COMMON, name);
           break;
        case 1:
           disable_clock(MT_CG_VDEC1_LARB, name);
           break;
        case 2:
           disable_clock(MT_CG_IMAGE_LARB2_SMI, name);
           break;
        default:
            break;
    }

    return 0;
}

void SMI_PowerOn(void)
{
	int i;

	for (i=0; i<SMI_LARB_NUMBER; i++) {
		met_larb_clock_on(i);
	}

//printk("SMI clock on\n");
}

void SMI_PowerOff(void)
{
	int i;

	for (i=SMI_LARB_NUMBER-1; i>=0; i--) {
		met_larb_clock_off(i);
	}
//printk("SMI clock off\n");
}
