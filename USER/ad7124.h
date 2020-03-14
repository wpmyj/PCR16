#ifndef __AD7124_H__
#define __AD7124_H__

#include "bsp_ad7124.h"

enum _ad7124_status {
	AD7124_IDLE,
//	AD7124_MEASURE_AVDD,
//	AD7124_MEASURE_AVDD_OK,
//	AD7124_DETECT_SENSOR,
//	AD7124_MEASURE_SENSOR,
	AD7124_MEASURE_TEMP,
};
#define	AVALID_CH		6

typedef struct  _ad7124
{
	ad7124_dev_t *pdev;
	u8 channel_last;
	u8 channel;
	u8 status;
	u8 busy;
	u16 vol[AVALID_CH];
}_ad7124_t;

extern _ad7124_t ad7124;
void AD7124Init(void);
u8 StartADDataCollect(void);
u16 GetADCVol(u8 ch);
#endif

