/* main assembly file for the power chip */

.syntax unified
.cpu cortex-m0plus
.thumb

.global g_pfnVectors
.global Default_Handler
.global Reset_Handler

.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

.section .text,"ax",%progbits
.type Reset_Handler,%function

Reset_Handler:
	ldr r0, =_estack
	mov sp, r0

	/* copy the sidata around */
	ldr r3, =_sidata /* r3 == pointer to copy from */
	ldr r0, =_sdata
	ldr r1, =_edata

copy_data_loop:
	ldr r2, [r3]
	adds r3, r3, #4
	str r2, [r0]
	adds r0, r0, #4
	cmp r0, r1
	bcc copy_data_loop

	/* zero out bss */
	movs r0, #0
	ldr r1, =_sbss
	ldr r2, =_ebss

zero_bss_loop:
	
	str r0, [r1]
	adds r1, r1, #4
	cmp r1, r2
	bcc zero_bss_loop

	/* call main */
	bl main

return_loop:
	b return_loop

.size Reset_Handler, .-Reset_Handler

Default_Handler:
default_loop:
	b default_loop

.size Default_Handler, .-Default_Handler

/* vector table */

.section  .isr_vector,"a",%progbits
.type  g_pfnVectors, %object

g_pfnVectors:
  .word _estack

  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler

  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word 0
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler

/* External Interrupts */

  .word WWDG_IRQHandler                   /* Window WatchDog              */
  .word PVD_IRQHandler                    /* PVD through EXTI Line detect */
  .word RTC_TAMP_IRQHandler               /* RTC through the EXTI line    */
  .word FLASH_IRQHandler                  /* FLASH                        */
  .word RCC_IRQHandler                    /* RCC                          */
  .word EXTI0_1_IRQHandler                /* EXTI Line 0 and 1            */
  .word EXTI2_3_IRQHandler                /* EXTI Line 2 and 3            */
  .word EXTI4_15_IRQHandler               /* EXTI Line 4 to 15            */
  .word UCPD1_2_IRQHandler                /* UCPD1, UCPD2                 */
  .word DMA1_Channel1_IRQHandler          /* DMA1 Channel 1               */
  .word DMA1_Channel2_3_IRQHandler        /* DMA1 Channel 2 and Channel 3 */
  .word DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler /* DMA1 Channel 4 to Channel 7, DMAMUX1 overrun */
  .word ADC1_COMP_IRQHandler              /* ADC1, COMP1 and COMP2         */
  .word TIM1_BRK_UP_TRG_COM_IRQHandler    /* TIM1 Break, Update, Trigger and Commutation */
  .word TIM1_CC_IRQHandler                /* TIM1 Capture Compare         */
  .word TIM2_IRQHandler                   /* TIM2                         */
  .word TIM3_IRQHandler                   /* TIM3                         */
  .word TIM6_DAC_LPTIM1_IRQHandler        /* TIM6, DAC and LPTIM1         */
  .word TIM7_LPTIM2_IRQHandler            /* TIM7 and LPTIM2              */
  .word TIM14_IRQHandler                  /* TIM14                        */
  .word TIM15_IRQHandler                  /* TIM15                        */
  .word TIM16_IRQHandler                  /* TIM16                        */
  .word TIM17_IRQHandler                  /* TIM17                        */
  .word I2C1_IRQHandler                   /* I2C1                         */
  .word I2C2_IRQHandler                   /* I2C2                         */
  .word SPI1_IRQHandler                   /* SPI1                         */
  .word SPI2_IRQHandler                   /* SPI2                         */
  .word USART1_IRQHandler                 /* USART1                       */
  .word USART2_IRQHandler                 /* USART2                       */
  .word USART3_4_LPUART1_IRQHandler       /* USART3, USART4 and LPUART1   */
  .word CEC_IRQHandler                    /* CEC                          */

/*******************************************************************************
 *
 * Provide weak aliases for each Exception handler to the Default_Handler.
 * As they are weak aliases, any function with the same name will override
 * this definition.
 * 
 *******************************************************************************/

  .weak      NMI_Handler
  .thumb_set NMI_Handler,Default_Handler

  .weak      HardFault_Handler
  .thumb_set HardFault_Handler,Default_Handler

  .weak      SVC_Handler
  .thumb_set SVC_Handler,Default_Handler

  .weak      PendSV_Handler
  .thumb_set PendSV_Handler,Default_Handler

  .weak      SysTick_Handler
  .thumb_set SysTick_Handler,Default_Handler

  .weak      WWDG_IRQHandler
  .thumb_set WWDG_IRQHandler,Default_Handler

  .weak      PVD_IRQHandler
  .thumb_set PVD_IRQHandler,Default_Handler

  .weak      RTC_TAMP_IRQHandler
  .thumb_set RTC_TAMP_IRQHandler,Default_Handler

  .weak      FLASH_IRQHandler
  .thumb_set FLASH_IRQHandler,Default_Handler

  .weak      RCC_IRQHandler
  .thumb_set RCC_IRQHandler,Default_Handler

  .weak      EXTI0_1_IRQHandler
  .thumb_set EXTI0_1_IRQHandler,Default_Handler

  .weak      EXTI2_3_IRQHandler
  .thumb_set EXTI2_3_IRQHandler,Default_Handler

  .weak      EXTI4_15_IRQHandler
  .thumb_set EXTI4_15_IRQHandler,Default_Handler

  .weak      UCPD1_2_IRQHandler
  .thumb_set UCPD1_2_IRQHandler,Default_Handler

  .weak      DMA1_Channel1_IRQHandler
  .thumb_set DMA1_Channel1_IRQHandler,Default_Handler

  .weak      DMA1_Channel2_3_IRQHandler
  .thumb_set DMA1_Channel2_3_IRQHandler,Default_Handler

  .weak      DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler
  .thumb_set DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler,Default_Handler

  .weak      ADC1_COMP_IRQHandler
  .thumb_set ADC1_COMP_IRQHandler,Default_Handler

  .weak      TIM1_BRK_UP_TRG_COM_IRQHandler
  .thumb_set TIM1_BRK_UP_TRG_COM_IRQHandler,Default_Handler

  .weak      TIM1_CC_IRQHandler
  .thumb_set TIM1_CC_IRQHandler,Default_Handler

  .weak      TIM2_IRQHandler
  .thumb_set TIM2_IRQHandler,Default_Handler

  .weak      TIM3_IRQHandler
  .thumb_set TIM3_IRQHandler,Default_Handler

  .weak      TIM6_DAC_LPTIM1_IRQHandler
  .thumb_set TIM6_DAC_LPTIM1_IRQHandler,Default_Handler

  .weak      TIM7_LPTIM2_IRQHandler
  .thumb_set TIM7_LPTIM2_IRQHandler,Default_Handler

  .weak      TIM14_IRQHandler
  .thumb_set TIM14_IRQHandler,Default_Handler

  .weak      TIM15_IRQHandler
  .thumb_set TIM15_IRQHandler,Default_Handler

  .weak      TIM16_IRQHandler
  .thumb_set TIM16_IRQHandler,Default_Handler

  .weak      TIM17_IRQHandler
  .thumb_set TIM17_IRQHandler,Default_Handler

  .weak      I2C1_IRQHandler
  .thumb_set I2C1_IRQHandler,Default_Handler

  .weak      I2C2_IRQHandler
  .thumb_set I2C2_IRQHandler,Default_Handler

  .weak      SPI1_IRQHandler
  .thumb_set SPI1_IRQHandler,Default_Handler

  .weak      SPI2_IRQHandler
  .thumb_set SPI2_IRQHandler,Default_Handler

  .weak      USART1_IRQHandler
  .thumb_set USART1_IRQHandler,Default_Handler

  .weak      USART2_IRQHandler
  .thumb_set USART2_IRQHandler,Default_Handler

  .weak      USART3_4_LPUART1_IRQHandler
  .thumb_set USART3_4_LPUART1_IRQHandler,Default_Handler

  .weak      CEC_IRQHandler
  .thumb_set CEC_IRQHandler,Default_Handler

.size  g_pfnVectors, .-g_pfnVectors
