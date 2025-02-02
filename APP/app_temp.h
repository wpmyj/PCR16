#ifndef __APP_TEMP_H__
#define __APP_TEMP_H__

#include "includes.h"
#include "TempCalc.h"
#include "PID.h"

#define	HOLE_TEMP		0
#define	COVER_TEMP		1

#define	TEMPCTRL_NUM		2//需要pid控制数量


typedef struct _temp_ctrl	{
	u8 PIDid;
	TIM_HandleTypeDef *pTECPWM;	
	u8 TimCH;
	u16 TimPluse;
	u8 DutyMax;
//	s16 target_t;//目标温度 0.1
//	float PIDParam;//tec pwm占空比
}temp_ctrl_t;

typedef struct _app_temp	{
	OS_EVENT           *MSG_Q;
	OS_EVENT           *lock;
	
	s32 current_t[TEMP_ID_NUMS];//当前温度 0.01
//	pid_ctrl_t *pPid;
}_app_temp_t;

void AppTempInit (void);
u8 StartAPPTempCtrl(void);
void StopAPPTempCtrl(void);
s16 GetCoverTemperature(void);
s16 GetHoleTemperature(void);
#endif

