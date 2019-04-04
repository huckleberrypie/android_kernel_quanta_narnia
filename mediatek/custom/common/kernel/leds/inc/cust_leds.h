#ifndef _CUST_LEDS_H
#define _CUST_LEDS_H

#include <mach/mt_typedefs.h>
enum mt65xx_led_type
{
#if 0 //Barkly
	MT65XX_LED_TYPE_RED = 0,
	MT65XX_LED_TYPE_GREEN,
	MT65XX_LED_TYPE_BLUE,
	MT65XX_LED_TYPE_JOGBALL,
#else
	MT65XX_LED_TYPE_JOGBALL = 0,
#endif //Barkly~
	MT65XX_LED_TYPE_KEYBOARD,
	MT65XX_LED_TYPE_BUTTON,	
	MT65XX_LED_TYPE_LCD,
	MT65XX_LED_TYPE_TOTAL,
};



#endif

