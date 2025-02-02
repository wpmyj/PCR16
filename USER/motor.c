#include "motor.h"

message_pkt_t    msg_pkt_motor;
struct _motor_port{
	struct _io_map en;
	struct _io_map dir;
	struct _io_map step;
};

typedef struct _motor_priv{
	struct _motor_port *pport;
}motor_priv_t;
velprofile_t VelProfile;
#define ARRLEN	40
u16 vel_arr[ARRLEN];
/*
*********************************************************************************************************
*                                       PRIVATE FUNCTION PROTOTYPES
*********************************************************************************************************
*/
static void     UpdateMotorTimer        (MOTOR_ID id, INT16U val);
//static void     CalcSpedingProcedure    (MOTOR_ID id,INT8U if_acc);
static void UpdateMotorPWM(MOTOR_ID id, INT16U val);
static void StartMotorPWM(MOTOR_ID id);
static void StopMotorPWM(MOTOR_ID id);
static void StartMotorAccDec(MOTOR_ID id);
static void StopMotorAccDec(MOTOR_ID id);
/*
*********************************************************************************************************
*                                  PRIVATE GLOBAL CONSTANTS & VARIABLES
*********************************************************************************************************
*/

static struct _motor_port g_motor_port[MOTOR_ID_NUMS]={
	[MOTOR_ID1] = {
		.en ={MOTOR_ID1_EN_PORT ,MOTOR_ID1_EN_PIN},
		.dir={MOTOR_ID1_DIR_PORT,MOTOR_ID1_DIR_PIN},
		.step={MOTOR_ID1_STEP_PORT,MOTOR_ID1_STEP_PIN}
	},
/*	[MOTOR_ID2] = {
		.en ={MOTOR_ID2_EN_PORT ,MOTOR_ID2_EN_PIN},
		.dir={MOTOR_ID2_DIR_PORT,MOTOR_ID2_DIR_PIN},
		.step={MOTOR_ID2_STEP_PORT,MOTOR_ID2_STEP_PIN}
	},*/
};

static motor_priv_t g_motor_priv[MOTOR_ID_NUMS]={
	[MOTOR_ID1] = {
		.pport = &g_motor_port[MOTOR_ID1]
	},
/*	[MOTOR_ID2] = {
		.pport = &g_motor_port[MOTOR_ID2]
	},*/
};
TMotor tMotor[MOTOR_ID_NUMS];

#define ACC_TIME	1//加速时间间隔1ms
static void VelProfile_Init(void)
{
	u8 t;
	u16 vel_profile[ARRLEN];
	
	VelProfile.Vo = 1000;
	VelProfile.Vmax = 2000;
	VelProfile.AccStep = 80;//加速步进
	VelProfile.SaStep = 40;
	for(t=0;;t++)	{
		vel_profile[t] = VelProfile.AccStep*t + VelProfile.Vo;
		if(vel_profile[t]>=VelProfile.Vmax)	{
			VelProfile.MaxIdx = t;//加速次数
			break;
		}
	}
	for(t=0;t<=VelProfile.MaxIdx;t++)	{
		vel_arr[t] = 80000/vel_profile[t];
	}
	VelProfile.pVelBuf = vel_arr;
	tMotor[MOTOR_ID1].pCurve = &VelProfile;
}

void Motor_Init(void)
{
	VelProfile_Init();
}

void enable_motor(TMotor *pdev)
{
  struct _io_map const *m_en = &g_motor_port[pdev->id].en;
  
  SET_L(m_en);
}

void disable_motor(TMotor *pdev)
{
  struct _io_map const *m_en = &g_motor_port[pdev->id].en;
  
  SET_H(m_en);
}
//停止电机
void StopMotor(TMotor *pMotor)
{	
	if(pMotor->status.is_run==MotorState_Run)	{
		StopMotorPWM(pMotor->id);
		OSSemPost(pMotor->Sem);
		pMotor->status.is_run        = MotorState_Stop;
		pMotor->if_acc = DEF_False;
//		StopMotorAccDec(MOTOR_ID1);//停止加减速
	}
}
//设置电机方向
static INT32S SetMotorDir(TMotor *pMotor,INT32S step)
{
    if(step<0){
        step = -step;
        pMotor->Dir = MOTOR_TO_MIN;
    }else{
    	pMotor->Dir = MOTOR_TO_MAX;
    }
    return step;
}

INT32S StepsToLen(TMotor *pdev)
{
    INT32S tmp;

    tmp = (INT32S)(Motor_NumPerStep*pdev->CurSteps*10);
 
    return tmp;
}

//将pc传下来的长度转换为距离，pc传下来的距离是0.1mm为单位
INT32U LenToSteps(TMotor *pdev,INT32S len)
{
    INT32U tmp;

    tmp = (Motor_StepsPerum*len);
    tmp /= 10;
 
    return tmp;
}

//以零点为参考点，向前移动len距离
void CalcAnyPosAtResetSteps(TMotor *pMotor,INT32S step)
{
    INT32S tmp,curSteps;

    pMotor->SysAbsoluteOffsetStep = step;
    if((pMotor->SysAbsoluteOffsetStep > Motor_Move_MAX_STEP))//x y导轨最大行程限制
		pMotor->SysAbsoluteOffsetStep = Motor_Move_MAX_STEP;
	
	curSteps = pMotor->CurSteps;
    tmp = pMotor->SysAbsoluteOffsetStep - curSteps;//目标位置与当前位置的差 大于0前进
    pMotor->MoveTotalSteps = SetMotorDir(pMotor,tmp);
}
//计算加减速点 最后一段ConSteps距离强制匀速
static void CalcSpedingProcedure(TMotor *pMotor,INT8U if_acc)
{
    s32 updn_steps;

    if(!if_acc || pMotor->MoveTotalSteps <= pMotor->ConSteps)	{//强制匀速 或者 总距离小于某值
    	pMotor->AccSteps = -1;//表示匀速运动		
    }
	else	{
		updn_steps = pMotor->MoveTotalSteps - pMotor->ConSteps;//减去后端匀速路程
		pMotor->AccSteps = 0;//起点开始立即加速
		if(updn_steps <= (pMotor->pCurve->SaStep*2))    {//计算减速点
			pMotor->DecSteps = updn_steps/2 + pMotor->AccSteps;
		}
		else        {
			pMotor->DecSteps = (updn_steps - pMotor->pCurve->SaStep) + pMotor->AccSteps;
		}
	}
}

#include "PD_DataProcess.h"
void MotorArrivedCheck(TMotor *pMotor)
{
	if(pMotor->status.is_run != MotorState_Run)	{
		return;		
	}
	if(pMotor->StepCnt >= pMotor->MoveTotalSteps)	{
		StopMotor(pMotor);	
	}
	else if(Sys.state & SysState_CollHolePD)	{//根据电机是否到达孔位置 采集孔PD值
		u8 i = HolePos.idx;
//		for(i=0;i<HOLE_NUM;i++)	
		{
			if(tMotor[MOTOR_ID1].CurSteps > HolePos.pos[i].x1 && tMotor[MOTOR_ID1].CurSteps < HolePos.pos[i].x2)	{//电机到达孔位置范围 启动pd采集
				if(sample_data.hole[i].sample_t[0] != 0||Sys.devstate == DevState_Debug)
					gPD_Data.coll_enable = DEF_True;
			}
			else if(gPD_Data.coll_enable == DEF_True)	{//走出孔位置 
				gPD_Data.coll_enable = DEF_False;
				CalcPDData(HolePos.idx);
				if(gPD_Data.PDVol[HolePos.idx]==0)
					StopMotor(&tMotor[MOTOR_ID1]);
				if(tMotor[MOTOR_ID1].Dir==MOTOR_TO_MAX)	
					HolePos.idx ++;
				else
					HolePos.idx --;
			}
		}
	}
}

//电机加减速 匀加速 1ms
void MotorAccDec(TMotor *pMotor)
{
	u16 arr;
	
	if(pMotor->status.is_run != MotorState_Run||pMotor->if_acc == DEF_False)	{
		return;		
	}

	if(pMotor->StepCnt >= pMotor->DecSteps)	{//到达减速点 减速
		if(pMotor->TableIndex > 0)
			pMotor->TableIndex --;		
	}
	else if(pMotor->TableIndex < pMotor->pCurve->MaxIdx)	{//没到达减速点 加速
		pMotor->TableIndex ++;
	}
	else {//匀速
		return;
	}
	arr = pMotor->pCurve->pVelBuf[pMotor->TableIndex];
	UpdateMotorPWM(pMotor->id,arr);	
}

u8 StartMotor(TMotor *pMotor, INT8U dir, INT32U steps,INT8U if_acc)
{
    INT8U err;
//    INT32U len;

   if(pMotor->status.is_run == MotorState_Run)
	   return 0;
   struct _io_map const *m_dir = &g_motor_port[pMotor->id].dir;
    pMotor->StepCnt       = 0;
	if (steps)
    {
        pMotor->Dir = dir;
        if(pMotor->Dir==MOTOR_TO_MIN) {//后退
            if(!Motor_MinLimit())//在上限开关位置
                goto _end;
            SET_H(m_dir);
        }
        else    {//前进
            if(!Motor_MaxLimit())//在下限开关位置
                goto _end;
            SET_L(m_dir);
        }
        enable_motor(pMotor);
        pMotor->MoveTotalSteps    = steps;
		pMotor->status.is_run = MotorState_Run;
        CalcSpedingProcedure(pMotor,if_acc);
        pMotor->TableIndex    = 0;
//        pMotor->StepsCallback = &MotorPositionCheck;
        StartMotorPWM(pMotor->id);
//		if(if_acc)//是否开启加减速
//			StartMotorAccDec(pMotor->id);
		pMotor->if_acc = if_acc;
		OSSemPend(pMotor->Sem, 0, &err);//等待事件触发
		disable_motor(pMotor);
	}
_end:
	
	return 1;
}

//static void StartMotorAccDec(MOTOR_ID id)
//{
//	__HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
//	__HAL_TIM_ENABLE(&htim6);
//	__HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
//}

//static void StopMotorAccDec(MOTOR_ID id)
//{
//	__HAL_TIM_DISABLE(&htim6);
//}

#define	MOTORPWM_CH		TIM_CHANNEL_3
static void StopMotorPWM(MOTOR_ID id)
{
	HAL_TIM_PWM_Stop(tMotor[id].tmr, MOTORPWM_CH);
}

static void UpdateMotorPWM(MOTOR_ID id, INT16U val)
{
//	__HAL_TIM_SET_COUNTER(tMotor[id].tmr, 0);
    __HAL_TIM_SET_AUTORELOAD(tMotor[id].tmr, val);
    __HAL_TIM_SET_COMPARE(tMotor[id].tmr, MOTORPWM_CH, val/2);
}

static void StartMotorPWM(MOTOR_ID id)
{
	__HAL_TIM_CLEAR_FLAG(tMotor[id].tmr, TIM_FLAG_UPDATE);
	UpdateMotorPWM(id, tMotor[id].pCurve->pVelBuf[tMotor[id].TableIndex]);
	HAL_TIM_PWM_Start(tMotor[id].tmr, MOTORPWM_CH);	
}
