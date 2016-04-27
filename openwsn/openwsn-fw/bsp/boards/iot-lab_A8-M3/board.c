/**
\brief iot-lab_A8-M3 definition of the "board" bsp module.

\author Alaeddine Weslati <alaeddine.weslati@inria.fr>, January 2014.
*/
#include "stm32f10x_lib.h"
#include "board.h"
// bsp modules
#include "leds.h"
#include "uart.h"
#include "spi.h"
#include "bsp_timer.h"
#include "radio.h"
#include "radiotimer.h"
#include "rcc.h"
#include "nvic.h"
#include "debugpins.h"
#include "opentimers.h"
#include "gpio.h"

//=========================== main ============================================

extern int mote_main(void);

int main(void)
{
 return mote_main();
}

//=========================== public ==========================================

void board_init()
{
  RCC_Configuration();//Configure rcc
  NVIC_Configuration();//configure NVIC and Vector Table
  
  //configure ALL GPIO to AIN to get lowest power
  GPIO_Config_ALL_AIN();
  //configuration GPIO to measure the time from sleep to 72MHz
  GPIO_Configuration();
 
  GPIO_InitTypeDef  GPIO_InitStructure;

  //enable GPIOC and GPIOA, Clock
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC , ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA , ENABLE);
  
  //Configure PA.02 as SLP_TR pin of RF
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  //Configure PC.01 as RST pin of RF
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  
  //set /RST pin high(never reset)
  GPIO_SetBits(GPIOC, GPIO_Pin_1);
  
  // Configure PC.04 as input floating (EXTI Line4)
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource4);//Connect EXTI Line4 to PC.4
  EXTI_ClearITPendingBit(EXTI_Line4);

  //Configures EXTI line 4 to generate an interrupt on rising edge
  EXTI_InitTypeDef  EXTI_InitStructure; 
  EXTI_InitStructure.EXTI_Line    = EXTI_Line4;
  EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt; 
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE; 
  EXTI_Init(&EXTI_InitStructure);
  
  // initialize board
  leds_init();
  uart_init();
  spi_init();
  bsp_timer_init();
  radio_init();
  radiotimer_init();
  debugpins_init();
  //enable nvic for the radio
  NVIC_radio();
}

void board_sleep()
{
#if 0
  uint16_t sleepTime = radiotimer_getPeriod() - radiotimer_getCapturedTime();
  DBGMCU_Config(DBGMCU_STOP, ENABLE);
  
  // Enable PWR and BKP clock
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
  // Desable the SRAM and FLITF clock in Stop mode
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_SRAM | RCC_AHBPeriph_FLITF, DISABLE);

  PWR_EnterSTOPMode(PWR_Regulator_ON,PWR_STOPEntry_WFI);
  
  if(sleepTime > 0)
  opentimers_sleepTimeCompesation(sleepTime*2);
#endif 
}

void board_reset()
{
}

