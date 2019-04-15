#include "stm32f0xx_ll_rcc.h"
#include "stm32f0xx_ll_system.h"
#include "stm32f0xx_ll_bus.h"
#include "stm32f0xx_ll_gpio.h"
#include "stm32f0xx_ll_exti.h"
#include "stm32f0xx_ll_utils.h"
#include "stm32f0xx_ll_cortex.h"

#define SYSCLK 48000000
#define MY_FREQ 100
#define DEBOUNCE 50

/**
  * System Clock Configuration
  * The system Clock is configured as follow :
  *    System Clock source            = PLL (HSI/2)
  *    SYSCLK(Hz)                     = 48000000
  *    HCLK(Hz)                       = 48000000
  *    AHB Prescaler                  = 1
  *    APB1 Prescaler                 = 1
  *    HSI Frequency(Hz)              = 8000000
  *    PLLMUL                         = 12
  *    Flash Latency(WS)              = 1
  */

static void rcc_config(void)
{
    /* Set FLASH latency */
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);

    /* Enable HSI and wait for activation*/
    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1);

    /* Main PLL configuration and activation */
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI_DIV_2,
                                LL_RCC_PLL_MUL_12);

    LL_RCC_PLL_Enable();
    while (LL_RCC_PLL_IsReady() != 1);

    /* Sysclk activation on the main PLL */
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);

    /* Set APB1 prescaler */
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);

    /* Update CMSIS variable (which can be updated also
     * through SystemCoreClockUpdate function) */
    SystemCoreClock = 48000000;
}

static void gpio_config(void)
{
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_8, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_9, LL_GPIO_MODE_OUTPUT);

	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_8, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_0, LL_GPIO_PULL_DOWN);
	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_1, LL_GPIO_PULL_DOWN); // what does it mean??
	return;
}

/*__attribute__((naked)) static void delay(void)
{
    asm ("push {r7, lr}");
    asm ("ldr r6, [pc, #8]");
    asm ("sub r6, #1");
    asm ("cmp r6, #0");
    asm ("bne delay+0x4");
    asm ("pop {r7, pc}");
    asm (".word 0x927c00"); //9600000
}*/


static void exti_config(void)
{
	/*Clocking to the system_config registers*/
	LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);

	LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE0);
	LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE1);
	LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_0);
	LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_1);
	LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_0);
	LL_EXTI_EnableFallingTrig_0_31(LL_EXTI_LINE_0);
	LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_1);
        LL_EXTI_EnableFallingTrig_0_31(LL_EXTI_LINE_1);

	/*Configure NVIC interrupt module*/
	NVIC_EnableIRQ(EXTI0_1_IRQn);
	NVIC_SetPriority(EXTI0_1_IRQn, 0);

}

static int counter_top = 1000;

void EXTI0_1_IRQHandler(void) // why not static??
{
	static int8_t states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
	uint8_t encoder_current_state = 0;
	static uint8_t encoder_two_last_states = 0;
	static int8_t encoder_sum_direction = 0x00;
	encoder_current_state = 0x0003 & LL_GPIO_ReadInputPort(GPIOA);
	encoder_two_last_states = ((0x03 & encoder_two_last_states) << 2) | encoder_current_state;
	encoder_sum_direction += states[encoder_two_last_states];
	if(encoder_sum_direction == -4)
	{
		if(counter_top > 10)
			counter_top -= counter_top / 10;
		//LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_8);
		encoder_sum_direction = 0;
	}
	if(encoder_sum_direction == 4)
        {
		if(counter_top < 3000)
			counter_top += counter_top / 10;
                //LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_9);
		encoder_sum_direction = 0;
        }

		
	LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);
	LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_1);
	return;
/*	int ms = milliseconds;
	static int ms_old = 0;
	if(ms - ms_old > DEBOUNCE)
	{
	LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_8);
	LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);
	ms_old = ms;
	}*/
}

static void systick_config(void)
{
	LL_InitTick(SYSCLK, MY_FREQ);
	LL_SYSTICK_EnableIT();
	NVIC_SetPriority(SysTick_IRQn, 0);
	return; //should we write return??
}

void SysTick_Handler(void)
{
	static int counter = 0;
	counter = (counter + 1) % counter_top;
	if(counter == 0)
		LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_8);
}


int main(void)
{
	rcc_config();
	gpio_config();
	exti_config();
	systick_config();
	while(1)
	{
	}
	return 0;
}	
