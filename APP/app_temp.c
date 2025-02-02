#include "app_temp.h"
#include "ad7124.h"
#include "timer.h"
#include "app_spiflash.h"
#include "app_motor.h"
//堆栈
__align(4) OS_STK  TASK_TEMP_STK[STK_SIZE_TEMP]; //任务堆栈声?

_app_temp_t app_temp;
temp_ctrl_t TempCtrl[TEMPCTRL_NUM];
#define	HOLE_TECPWM_PLUSE		400
#define	COVER_TECPWM_PLUSE		800
#define	HOLE_TECPWM_MAX		62//TEC pwm占空比最大值
#define	COVER_TECPWM_MAX		100//TEC pwm占空比最大值
static  message_pkt_t    msg_pkt_temp;
static void AppTempTask (void *parg);

void AppTempInit (void)
{
	OSTaskCreate(AppTempTask,  (void * )0, (OS_STK *)&TASK_TEMP_STK[STK_SIZE_TEMP-1], TASK_PRIO_TEMP);
}

static void TempDatInit(void)
{
	TempCtrl[HOLE_TEMP].PIDid = PID_ID1;
	TempCtrl[HOLE_TEMP].pTECPWM = &htim8;
	TempCtrl[HOLE_TEMP].TimCH = TIM_CHANNEL_1;
	TempCtrl[HOLE_TEMP].TimPluse = HOLE_TECPWM_PLUSE;
	TempCtrl[HOLE_TEMP].DutyMax = HOLE_TECPWM_MAX;
	SetPIDOutputLimits(PID_ID1, -HOLE_TECPWM_MAX, HOLE_TECPWM_MAX);
	
	TempCtrl[COVER_TEMP].PIDid = PID_ID2;
	TempCtrl[COVER_TEMP].pTECPWM = &htim2;
	TempCtrl[COVER_TEMP].TimCH = TIM_CHANNEL_4;
	TempCtrl[COVER_TEMP].TimPluse = COVER_TECPWM_PLUSE;
	TempCtrl[COVER_TEMP].DutyMax = COVER_TECPWM_MAX;
	SetPIDOutputLimits(PID_ID2, -COVER_TECPWM_MAX, COVER_TECPWM_MAX);
}
//TEC pwm控制
void StartTECPWM(temp_ctrl_t *pTempCtrl, u8 duty)
{
	u16 temp;
	
	temp = (pTempCtrl->TimPluse*duty)/100;
	if(duty>=100)	{
		temp = 101;
	}
	StartPWM(pTempCtrl->pTECPWM, pTempCtrl->TimCH, temp);
}

//停止温度控制
static void StopTempCtrl(temp_ctrl_t *pTempPid)
{
	StopPWM(pTempPid->pTECPWM, pTempPid->TimCH);
//	pTempPid->PIDParam = 0.0;
}
u16 setval;
//pid调节半导体片温度 采样增量法计算 pwm占空比不能超过50%
static void TempControl(temp_ctrl_t *pTempPid, u16 cur_t)
{
	s16 dat;
	float temp;
//	u16 setval;
	
	temp = PID_control(pTempPid->PIDid, cur_t);
	dat = (s16)temp;
	if(dat<0)	{//当前温度高于目标温度 将TEC切换到制冷模式 快速降温
		TEC_DIR_COLD();
	}
	else	{//当前温度低于目标温度 将TEC切换到制热模式 快速升温
		TEC_DIR_HOT();
	}
	setval = abs(dat);
	if(setval > pTempPid->DutyMax)
		setval = pTempPid->DutyMax;
//	temp = GetPIDIncrement(pTempPid->PIDid);//获取上次增量值
//	temp += PID_control(pTempPid->PIDid, cur_t);//
//	dat = (s16)floatToInt(temp);
//	if(dat<0)	{//当前温度高于目标温度 将TEC切换到制冷模式 快速降温
//		if(dat < -pTempPid->DutyMax)
//			setval = pTempPid->DutyMax;
//		else
//			setval = -dat;
//		TEC_DIR_COLD();
//	}
//	else {//当前温度低于目标温度 将TEC切换到制热模式 快速升温
//		if(dat > pTempPid->DutyMax)
//			setval = pTempPid->DutyMax;
//		else
//			setval = dat;
//		TEC_DIR_HOT();
//	}
	StartTECPWM(pTempPid, setval);
//	SYS_PRINTF("D:%d,T:%d ",dat,cur_t);
}
//
u8 StartAPPTempCtrl(void)
{
	msg_pkt_temp.Src = MSG_WriteLabTemplate;//保存实验模板, 路径./lab/Temp.json; ./lab/Lab.json
	OSQPost(spiflash.MSG_Q, &msg_pkt_temp);	
	OSTimeDly(500);
	ClearPIDDiff(TempCtrl[HOLE_TEMP].PIDid);	
	SetPIDVal(PID_ID1, 0.65, 0.00025, 5.8);
	return 1;
}

void StopAPPTempCtrl(void)
{	
	SoftTimerStop(&SoftTimer1, DEF_True);
	SoftTimerStop(&SoftTimer2, DEF_False);
}
u8 hengwenflag;
//恒温时间达到 调用该函数
static void ConstantTempArrivedCallback(void)
{
	u8 m;
	
	m = temp_data.CurStage;
	temp_data.stage[m].CurStep++;
	if(temp_data.stage[m].CurStep>=temp_data.stage[m].StepNum)	{//达到当前阶段的最后一步
		temp_data.stage[m].CurRepeat ++;
		if(temp_data.stage[m].CurRepeat>=temp_data.stage[m].RepeatNum)	{//达到当前阶段的最后一个循环 进入下阶段
			temp_data.CurStage++;
			if(temp_data.CurStage>=temp_data.StageNum)	{//达到最后一个阶段 停止控温
				Sys.devstate = DevState_IDLE;
			}else	{
				m = temp_data.CurStage;
				temp_data.stage[m].CurStep=0;
			}
		}else	{//未达到当前阶段的最后一个循环 继续该阶段			
			temp_data.stage[m].CurStep = 0;
		}
	}
	SoftTimerStop(&SoftTimer1, DEF_True);
}

static void PD_DataCollectCallback(void)
{
	msg_pkt_temp.Src = MSG_CollectHolePD_EVENT;//启动电机 开始采集孔PD值
	OSMboxPost(tMotor[MOTOR_ID1].Mbox, &msg_pkt_temp);
	SoftTimerStop(&SoftTimer2, DEF_False);
}

//按照设置好的温度程序巡视 设置的温度曲线控温
void TempProgramLookOver(s16 c_temp)
{
	u8 m,n;
	s16 target;
	static u8 ConstantTempCnt;
	
	if(Sys.devstate != DevState_Running)
		return;
	m = temp_data.CurStage;
	n = temp_data.stage[m].CurStep;
	target = temp_data.stage[m].step[n].temp;
	if(abs(c_temp-target)>100)	{//温度差大于1度 当前处于升降温阶段		
		ConstantTempCnt = 0;
		SetPIDTarget(PID_ID1, target);
		hengwenflag = 0;
	}
	else {//到达目标温度 当前处于恒温阶段
		ConstantTempCnt++;
		if(ConstantTempCnt>=5)	{//持续500ms 温度差小于1度 判断温度控制已稳定
			ConstantTempCnt = 0;
			if(GetSoftTimerState(&SoftTimer1)==DEF_Stop)	{//100ms 为单位
				SoftTimerStart(&SoftTimer1, temp_data.stage[m].step[n].tim*10, DEF_True); //设置恒温时间定时
				SoftTimer1.pCallBack = &ConstantTempArrivedCallback;
				SoftTimerStart(&SoftTimer2, (temp_data.stage[m].step[n].tim-8)*10, DEF_False);//设置PD数据采集时间定时
				SoftTimer2.pCallBack = &PD_DataCollectCallback;
			}
			hengwenflag = target;
		}
	}
}
//风扇控制：flag 开关，duty pwm占空比
void StartCoolFan(u8 flag, u8 duty)
{
	u32 temp;
	
	if(flag==DEF_ON)	{
		temp = (80*duty)/100;
		StartPWM(&htim4, TIM_CHANNEL_3, temp);
	}
	else if(flag==DEF_OFF)
		StopPWM(&htim4, TIM_CHANNEL_3);
}

#define	TEMPCTRL_ACCURACY		10//温控精度0.1
#define	TEMPCOLLECT_ACCURACY		5//温度采集精度 0.05
static void AppTempTask (void *parg)
{
	s32 cur_temp;
	
	PIDParamInit();
	TempDatInit();
	StopTempCtrl(&TempCtrl[HOLE_TEMP]);
	OSTimeDly(1000);
//	EquipFAN_ON();//打开设备风扇
	SetPIDVal(PID_ID1, 0.65, 0.00025, 5.8);
//	SetPIDVal(PID_ID1, 0.0, 0.0, 0.0);
	StartCoolFan(DEF_ON, 100-70);//打开制冷片风扇 默认50%占空比
	EquipFAN_OFF();
	for(;;)
    {
		if(Sys.devstate == DevState_Running||Sys.devsubstate == DevSubState_DebugTemp)	
		{
			if(CalcTemperature(GetADCVol(TEMP_ID1), &cur_temp)==0)	{//计算模块温度
				TempProgramLookOver(cur_temp);
				app_temp.current_t[TEMP_ID1] = cur_temp;//0.01
//				SetPIDTarget(PID_ID1, TempPid[HOLE_TEMP].target_t);//设置控制目标
				TempControl(&TempCtrl[HOLE_TEMP], cur_temp);//pid调节 增量法计算
				SysError.Y1.bits.b3 = DEF_Active;
			}else	{//温度传感器脱落
				SysError.Y1.bits.b3 = DEF_Inactive;//温度传感器异常
			}
			if(CalcTemperature(GetADCVol(TEMP_ID2), (s32 *)&cur_temp)==0)	{
				app_temp.current_t[TEMP_ID2] = cur_temp;
				SysError.Y1.bits.b4 = DEF_Active;				
			}else	{
				SysError.Y1.bits.b4 = DEF_Inactive;
			}
			if(CalcTemperature(GetADCVol(TEMP_ID3), (s32 *)&cur_temp)==0)	{
				app_temp.current_t[TEMP_ID3] = cur_temp;
//				SetPIDTarget(PID_ID2, TempPid[COVER_TEMP].target_t);//设置控制目标
//				TempCtrl(&TempPid[COVER_TEMP], cur_temp);//热盖pid调节 增量法计算
				SysError.Y1.bits.b5 = DEF_Active;
			}else	{
				SysError.Y1.bits.b5 = DEF_Inactive;
			}
//			if(CalcTemperature(GetADCVol(TEMP_ID4), (s32 *)&cur_temp)==0)	{//散热器 预留
//				app_temp.current_t[TEMP_ID4] = cur_temp;				
//			}else	{
//			
//			}
		}
		else	
		{
			ClearPIDDiff(TempCtrl[HOLE_TEMP].PIDid);
			StopTempCtrl(&TempCtrl[HOLE_TEMP]);
			ClearPIDDiff(TempCtrl[COVER_TEMP].PIDid);
			StopTempCtrl(&TempCtrl[COVER_TEMP]);
		}
		OSTimeDly(80);
	}
}

s16 GetCoverTemperature(void)
{
	return app_temp.current_t[TEMP_ID3];
}

s16 GetHoleTemperature(void)
{
	return app_temp.current_t[TEMP_ID1];
}

