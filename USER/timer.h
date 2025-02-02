#ifndef _TIMER_H
#define _TIMER_H

#include "includes.h"

typedef struct _softtimer
{
	TIM_HandleTypeDef *TIM;
	u32 cnt;
	u32 period;
  	u8 state;
	void (*pCallBack)();
}_softtimer_t;

extern _softtimer_t SoftTimer1;
extern _softtimer_t SoftTimer2;

void timer_update(TIM_HandleTypeDef *tmr,u32 val);
void timer_start(TIM_HandleTypeDef *tmr)  ;
void timer_stop(TIM_HandleTypeDef *tmr);
void SoftTimerInit(void);
u8 SoftTimerStart(_softtimer_t *psofttimer, u32 value, u8 restart_flag);
u8 GetSoftTimerState(_softtimer_t *psofttimer);
void SoftTimerStop(_softtimer_t *psofttimer, u8 stop_flag);
void SoftTimerCallback(void);
void UpdatePWM(TIM_HandleTypeDef *pPWM, u8 ch, INT16U val);
void StopPWM(TIM_HandleTypeDef *pPWM, u8 ch);
void StartPWM(TIM_HandleTypeDef *pPWM, u8 ch, u16 val);
#endif
