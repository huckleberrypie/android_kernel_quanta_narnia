
#include "hdmi_ctrl.h"
#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/div64.h>


#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>



//example to set hdmi clk
void vSetClk(void)
{
    //hdmi_i2c_write();
    //hdmi_i2c_write();
    //hdmi_i2c_write();
}

unsigned char  fgHDMIDDCByteWrite(unsigned char ui1Device, unsigned char ui1Data_Addr, unsigned char  u1Data)
{
  unsigned char fgResult = 0; 
  
  //fgResult = fgTxDataWrite(ui1Device/2, ui1Data_Addr, 1, &u1Data);
    
  
  if (fgResult== TRUE)
  {
    return TRUE;
  }	
  else
  {
    return FALSE;
  }	
}

unsigned char  fgHDMIDDCDataWrite(unsigned char ui1Device, unsigned char ui1Data_Addr, unsigned char u1Count, const unsigned char  *pr_u1Data)
{
	unsigned char fgResult = 0; 
	
	//fgResult = fgTxDataWrite(ui1Device/2, ui1Data_Addr, 1, &u1Data);
	  
	
	if (fgResult== TRUE)
	{
	  return TRUE;
	} 
	else
	{
	  return FALSE;
	} 

}

unsigned char fgHDMIDDCByteRead(unsigned char ui1Device, unsigned char ui1Data_Addr, unsigned char * pu1Data)
{
  unsigned char fgResult = 0;
  
 
  //fgResult= fgTxDataRead(ui1Device/2, ui1Data_Addr, 1, pu1Data);
  
  
  if (fgResult== TRUE)
  {
    return TRUE;
  }	
  else
  {
    return FALSE;
  }	
}

unsigned char fgHDMIDDCDataRead(unsigned char ui1Device, unsigned char ui1Data_Addr, unsigned char u1Count, unsigned char * pu1Data)
{
  unsigned char fgResult = 0;
  
  
  //fgResult= fgTxDataRead(ui1Device/2, ui1Data_Addr, 1, pu1Data);
  
  
  if (fgResult== TRUE)
  {
    return TRUE;
  } 
  else
  {
    return FALSE;
  } 
}

