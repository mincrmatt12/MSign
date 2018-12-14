#include "serial.h"
#include "esp_common.h"
#include "uart.h"

extern "C" void sign_serial_task(void * argument) {
	serial::interface.run();
}

void serial::SerialInterface::run() {
	// Inititalize things
	
	{
		UART_ConfigTypeDef uart_config;
		uart_config.baud_rate        = BIT_RATE_115200;
		uart_config.data_bits        = UART_WordLength_8b;
		uart_config.flow_ctrl        = USART_HardwareFlowControl_None;
		uart_config.stop_bits        = USART_StopBits_1;
		uart_config.parity           = USART_Parity_Even;
		uart_config.UART_InverseMask = UART_None_Inverse;
		
		UART_ParamConfig(UART0, &uart_config);

		UART_IntrConfTypeDef uart_interrupt_config;
		uart_interrupt_config.UART_IntrEnMask = UART_RXFIFO_TOUT_INT_ENA | UART_RXFIFO_OVF_INT_ENA;
		uart_interrupt_config.UART_RX_FifoFullIntrThresh = 3;

		UART_IntrConfig(UART0, &uart_interrupt_config);
	}
}
