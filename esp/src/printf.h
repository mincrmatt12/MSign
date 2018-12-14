#ifndef PRINTF_HACK_H
#define PRINTF_HACK_H

#include "esp_common.h"
#include "uart.h"

void init_printf() {
	UART_ConfigTypeDef uart_config;
	uart_config.baud_rate        = BIT_RATE_115200;
	uart_config.data_bits        = UART_WordLength_8b;
	uart_config.flow_ctrl        = USART_HardwareFlowControl_None;
	uart_config.stop_bits        = USART_StopBits_1;
	uart_config.parity           = USART_Parity_Even;
	uart_config.UART_InverseMask = UART_None_Inverse;
	
	UART_ParamConfig(UART1, &uart_config);
			
	UART_SetPrintPort(UART1);
}

#endif /* ifndef PRINTF_HACK_H */
