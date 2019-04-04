/*
 *  Quanta header file
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *  See the file "COPYING" in the main directory of this archive
 *  for more details.
 *
 */
#ifndef __MT_HARDWARE_INFO__
#define __MT_HARDWARE_INFO__

enum {
	HW_REV_A	= 0,
	HW_REV_B	= 1,
	HW_REV_C	= 2,
	HW_REV_D	= 3,
	HW_REV_E	= 4,
	HW_REV_F	= 5,
	HW_REV_RESERVE,
};

enum {
	MEM_ELPIDA	= 0,
	MEM_SAMSUNG,
	MEM_HYNIX,
	MEM_KINGSTON,
	MEM_RESERVE,
};

enum {
	MAINCAM_MCNEX	= 0,
	MAINCAM_LITEON,
	MAINCAM_BISON,
};

enum {
	SUBCAM_MCNEX	= 0,
	SUBCAM_LITEON,
	SUBCAM_BISON,
};


///////////////////////////////////////////////////////////////////////
// Function Description
//    this function is for FG2 mainboard detection
// Parameter
//    none
// Return
//    will return the version of mainboard
int qci_mainboard_version(void);

/******************************************************************
 Function Description
    Used to get memory vendor
 Parameter
    none
 Return
    Will return memory vendor ID
******************************************************************/
int qci_mem_vendor(void);
int qci_maincam_vendor(void);
int qci_subcam_vendor(void);

#endif
