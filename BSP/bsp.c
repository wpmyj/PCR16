#include "bsp.h"
//#include "tim.h"
#include "bsp_spi.h"
#include "bsp_w25qxx.h"
#include "motor.h"
#include "sys_data.h"
#include "bsp_max5401.h"

////////////////////////////////////
//IO配置函数
void	bsp_init(void)
{
	bsp_spi_init();
	BSP_W25Qx_Init();
	SoftTimerInit();
//	BspAD7091Init();
//	MAX5401_init();
}
//io配置
void ioconfig(const struct _io_map *pio, cpu_bool_t sw)
{
    GPIO_InitTypeDef  GPIO_InitStructure={0};

    GPIO_InitStructure.Pin   = pio->pin;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStructure.Mode  = (sw == DEF_OUT)? GPIO_MODE_OUTPUT_PP: GPIO_MODE_INPUT;
		GPIO_InitStructure.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(pio->port, &GPIO_InitStructure);
}

void FluoLED_OnOff(u8 led_t, u8 onoff)
{
//	Led_FluoGreen_Off();
//	Led_FluoBlue_Off();
	if(led_t&LED_BLUE)	{
		if(onoff==DEF_ON)
			Led_FluoBlue_On();
		else if(onoff==DEF_OFF)
			Led_FluoBlue_Off();
	}
	if(led_t&LED_GREEN)	{
		if(onoff==DEF_ON)
			Led_FluoGreen_On();
		else if(onoff==DEF_OFF)
			Led_FluoGreen_Off();	
	}
}
////修改串口波特率
//void UartBaudrateSet(UART_HandleTypeDef *phuart, u32 baudrate)
//{
//	__HAL_UART_DISABLE(phuart);
//	 phuart->Init.BaudRate = baudrate;
//	UART_SetConfig(phuart);
//	__HAL_UART_ENABLE(phuart);
//}
////使用time6进行us定时
//void delay_us(u16 us)
//{
//#if OS_CRITICAL_METHOD == 3
//	OS_CPU_SR   cpu_sr = 0;
//#endif
//  uint16_t value;
//	
//	OS_ENTER_CRITICAL();
//	value = 0xffff-us;
//	__HAL_TIM_SET_COUNTER(&htim6,value);
//	HAL_TIM_Base_Start(&htim6);
//	while(__HAL_TIM_GET_COUNTER(&htim6)<=0xfffe);
//	HAL_TIM_Base_Stop(&htim6);
//	OS_EXIT_CRITICAL();
//}

////亮度调低一般
//void SysStandbyEnter(void)
//{
//	sys.state |= SYSSTATE_STANDBYMODE;
//}

//void SysStandbyQuit(void)
//{
//	sys.StandbyFlag = 0;
//	sys.state &= ~SYSSTATE_STANDBYMODE;	
//}

////系统关机
//void SysShutDown(void)
//{
//	OSTimeDly(500);
//	SlaveboardPowerDisable();
//	DisplayPowerDisable();
//	UsbPowerDisable();
//	SysPowerDisable();
//}
//系统复位
void SoftReset(void)
{
	__set_FAULTMASK(1);      // 关闭所有中端
	NVIC_SystemReset();// 复位
}
//固件升级 复位
#define 	RunIAPKeyword               0xA5A55A5A
#define		ApplicationUpgradeKeywordAddr		0x0800E800 //一个page
void FWUpdate_reboot(void)
{
	FLASH_EraseInitTypeDef f;
	f.TypeErase = FLASH_TYPEERASE_PAGES;
	f.Page = 29;
	f.NbPages = 1;
	f.Banks = FLASH_BANK_1;
	//设置PageError
	uint32_t PageError = 0;
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(&f, &PageError);	//调用擦除函数	
	if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,ApplicationUpgradeKeywordAddr, RunIAPKeyword) == HAL_OK) {
		HAL_FLASH_Lock();
		SYS_PRINTF("SYS Reboot");
		OSTimeDly(1000);
		SoftReset();//RUN IAP
	}
	HAL_FLASH_Lock();
}
