#include <linux/kernel.h>
#include <linux/module.h>

#include "core/met_drv.h"

static const char strTopology[] = "LITTLE:0,1,2,3";

extern struct metdevice met_emi;
extern struct metdevice met_smi;
extern struct metdevice met_dramc;

#ifndef NO_MTK_THERMAL_GET_TEMP
#define NO_MTK_THERMAL_GET_TEMP 1
#endif
#if NO_MTK_THERMAL_GET_TEMP == 0
extern struct metdevice met_thermal;
#endif

#ifndef NO_MTK_TSCPU_HANDLER
#define NO_MTK_TSCPU_HANDLER 1
#endif
#if NO_MTK_TSCPU_HANDLER == 0
extern struct metdevice met_mtktscpu;
#endif

#ifndef NO_MTK_PTPOP_HANDLER
#define NO_MTK_PTPOP_HANDLER 1
#endif
#if NO_MTK_PTPOP_HANDLER == 0
extern struct metdevice met_ptpod;
#endif

#ifndef NO_SPM_TWAM_REGISTER_HANDLER
#define NO_SPM_TWAM_REGISTER_HANDLER 1
#endif
#if NO_SPM_TWAM_REGISTER_HANDLER == 0
extern struct metdevice met_spmtwam;
#endif

#ifndef NO_MTK_GPU_HANDLER
#define NO_MTK_GPU_HANDLER 1
#endif
#if NO_MTK_GPU_HANDLER == 0
extern struct metdevice met_gpu;
#endif


int met_reg_ext(void);
int met_dereg_ext(void);

static int __init met_plf_init(void)
{
	met_register(&met_emi);
	met_register(&met_smi);
	met_register(&met_dramc);

#if NO_MTK_GPU_HANDLER == 0
        met_register(&met_gpu);
#endif

#if NO_MTK_THERMAL_GET_TEMP == 0
	met_register(&met_thermal);
#endif
#if NO_MTK_TSCPU_HANDLER == 0
	met_register(&met_mtktscpu);
#endif
	met_reg_ext();
#if NO_MTK_PTPOP_HANDLER == 0
	met_register(&met_ptpod);
#endif
#if NO_SPM_TWAM_REGISTER_HANDLER == 0
	met_register(&met_spmtwam);
#endif
	met_set_platform("mt8127", 1);
	met_set_topology(strTopology, 1);

	return 0;
}

static void __exit met_plf_exit(void)
{
	met_dereg_ext();
	met_deregister(&met_emi);
	met_deregister(&met_smi);
	met_deregister(&met_dramc);

#if NO_MTK_GPU_HANDLER == 0
        met_deregister(&met_gpu);
#endif

#if NO_MTK_THERMAL_GET_TEMP == 0
	met_deregister(&met_thermal);
#endif
#if NO_MTK_TSCPU_HANDLER == 0
	met_deregister(&met_mtktscpu);
#endif
#if NO_MTK_PTPOP_HANDLER == 0
	met_deregister(&met_ptpod);
#endif
#if NO_SPM_TWAM_REGISTER_HANDLER == 0
	met_deregister(&met_spmtwam);
#endif
	met_set_platform(NULL, 0);
	met_set_topology(NULL, 0);
}

module_init(met_plf_init);
module_exit(met_plf_exit);
MODULE_AUTHOR("DT_DM5");
MODULE_DESCRIPTION("MET_MT8127");
MODULE_LICENSE("GPL");
