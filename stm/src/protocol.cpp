#include "protocol.h"
#include "stm32f2xx.h"
#include "stm32f2xx_ll_system.h"
#include "stm32f2xx_ll_bus.h"
#include "stm32f2xx_ll_usart.h"
#include "stm32f2xx_ll_gpio.h"
#include "stm32f2xx_ll_dma.h"
#include "pins.h"
#include "tasks/timekeeper.h"

extern tasks::Timekeeper timekeeper;

void srv::ProtocolImpl::dma_finish(bool incoming) {
	if (incoming) {
		// check for error
		if (NVIC_SRV_RXE_ACTV(UART_DMA)) {
			NVIC_SRV_RXE_CLRF(UART_DMA);
			LL_DMA_DisableStream(UART_DMA, UART_DMA_RX_Stream);

			// this is a deceptive command, clearing flags is done by reading SR and DR, which this function does.
			// it also "reads" a byte off of the serial port but eh.
			LL_USART_ClearFlag_ORE(ESP_USART);

			start_recv(state);
			return;
		}

		LL_DMA_DisableStream(UART_DMA, UART_DMA_RX_Stream);

		// first, check if we need to handle the handshake command
		if (state == ProtocolState::HANDSHAKE_RECV) {
			if (dma_buffer[0] == 0xa6 && dma_buffer[1] == 0x00 && dma_buffer[2] == 0x11) {
				state = ProtocolState::HANDSHAKE_SENT;
				return;
			}
			else if (dma_buffer[0] == 0xa6 && dma_buffer[1] == 0x00 && dma_buffer[2] == 0x13) {
				state = ProtocolState::UPDATE_STARTING;
				return;
			}
			else {
				NVIC_SystemReset();
			}
		}
		else if (state == ProtocolState::DMA_WAIT_SIZE && dma_buffer[1] != 0x00) {
			if (dma_buffer[0] != 0xa6) {
				int offset = 0;
				while (dma_buffer[1] == 0xa6 || dma_buffer[2] == 0xa6) {
					dma_buffer[0] = dma_buffer[1];
					dma_buffer[1] = dma_buffer[2];
					dma_buffer[2] = 0; 
					++offset;
				}
				if (offset) {
					offset = 3 - offset;
				}
				start_recv(ProtocolState::DMA_WAIT_SIZE, offset);
				return;
			}
			
			recv_full();
			last_comm = timekeeper.current_time;
		}
		else {
			process_command();
			start_recv();
			last_comm = timekeeper.current_time;
			sent_ping = false;
		}
	}
	else {
		LL_DMA_DisableStream(UART_DMA, UART_DMA_TX_Stream);

		while (!LL_USART_IsActiveFlag_TC(ESP_USART)) {
			;
		}

		is_sending = false;

		if (notify_on_send_done) {
			xTaskNotifyFromISR(notify_on_send_done, 1, eSetValueWithOverwrite, nullptr);
			notify_on_send_done = nullptr;
		}
	}
}

void srv::ProtocolImpl::switch_fast() {
	// TODO: ensure no transmissions are occuring
	LL_USART_Disable(ESP_USART);
	
	LL_USART_InitTypeDef usart_init = {0};
	usart_init.BaudRate = 230400;
	usart_init.OverSampling = LL_USART_OVERSAMPLING_8;
	usart_init.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	usart_init.DataWidth = LL_USART_DATAWIDTH_9B;
	usart_init.Parity = LL_USART_PARITY_EVEN;
	usart_init.StopBits = LL_USART_STOPBITS_1;
	usart_init.TransferDirection = LL_USART_DIRECTION_TX_RX;
	
	LL_USART_Init(ESP_USART, &usart_init);
	LL_USART_Enable(ESP_USART);
}

void srv::ProtocolImpl::setup_uart_dma() {
	// Enable clocks

	LL_AHB1_GRP1_EnableClock(UART_DMA_PERIPH | UART_GPIO_PERIPH);
	UART_ENABLE_CALL(UART_UART_PERIPH);

	// Setup USART
	
	LL_USART_InitTypeDef usart_init = {0};
	usart_init.BaudRate = 115200;
	usart_init.OverSampling = LL_USART_OVERSAMPLING_16;
	usart_init.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	usart_init.DataWidth = LL_USART_DATAWIDTH_9B;
	usart_init.Parity = LL_USART_PARITY_EVEN;
	usart_init.StopBits = LL_USART_STOPBITS_1;
	usart_init.TransferDirection = LL_USART_DIRECTION_TX_RX;
	
	LL_USART_Init(ESP_USART, &usart_init);

	LL_USART_EnableDMAReq_RX(ESP_USART);
	LL_USART_EnableDMAReq_TX(ESP_USART);

	LL_USART_ConfigAsyncMode(ESP_USART);
	LL_USART_Enable(ESP_USART);

	LL_USART_EnableDMAReq_RX(ESP_USART);
	LL_USART_EnableDMAReq_TX(ESP_USART);

	LL_USART_EnableIT_ERROR(ESP_USART);

	// Setup DMA channels

	LL_DMA_SetChannelSelection(UART_DMA, UART_DMA_RX_Stream, UART_DMA_Channel); // RX
	LL_DMA_SetChannelSelection(UART_DMA, UART_DMA_TX_Stream, UART_DMA_Channel); // TX

	// Config addresses doesn't actually set this.. it just reads it.

	LL_DMA_SetDataTransferDirection(UART_DMA, UART_DMA_RX_Stream, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
	LL_DMA_SetDataTransferDirection(UART_DMA, UART_DMA_TX_Stream, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
	
	LL_DMA_SetStreamPriorityLevel(UART_DMA, UART_DMA_RX_Stream, LL_DMA_PRIORITY_HIGH);
	LL_DMA_SetStreamPriorityLevel(UART_DMA, UART_DMA_TX_Stream, LL_DMA_PRIORITY_MEDIUM);

	LL_DMA_SetMode(UART_DMA, UART_DMA_RX_Stream, LL_DMA_MODE_NORMAL);
	LL_DMA_SetMode(UART_DMA, UART_DMA_TX_Stream, LL_DMA_MODE_NORMAL);

	LL_DMA_SetPeriphIncMode(UART_DMA, UART_DMA_RX_Stream, LL_DMA_PERIPH_NOINCREMENT);
	LL_DMA_SetMemoryIncMode(UART_DMA, UART_DMA_RX_Stream, LL_DMA_MEMORY_INCREMENT);
	LL_DMA_SetPeriphIncMode(UART_DMA, UART_DMA_TX_Stream, LL_DMA_PERIPH_NOINCREMENT);
	LL_DMA_SetMemoryIncMode(UART_DMA, UART_DMA_TX_Stream, LL_DMA_MEMORY_INCREMENT);

	LL_DMA_SetPeriphSize(UART_DMA, UART_DMA_RX_Stream, LL_DMA_PDATAALIGN_BYTE);
	LL_DMA_SetMemorySize(UART_DMA, UART_DMA_RX_Stream, LL_DMA_MDATAALIGN_BYTE);
	LL_DMA_SetPeriphSize(UART_DMA, UART_DMA_TX_Stream, LL_DMA_PDATAALIGN_BYTE);
	LL_DMA_SetMemorySize(UART_DMA, UART_DMA_TX_Stream, LL_DMA_MDATAALIGN_BYTE);

	LL_DMA_DisableFifoMode(UART_DMA, UART_DMA_RX_Stream);
	LL_DMA_DisableFifoMode(UART_DMA, UART_DMA_TX_Stream);

	// Setup GPIO
	
	LL_GPIO_InitTypeDef gpio_init = {0};

	gpio_init.Alternate = UART_Af;
	gpio_init.Pin = ESP_USART_TX | ESP_USART_RX;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	gpio_init.Pull = LL_GPIO_PULL_UP;

	LL_GPIO_Init(ESP_USART_Port, &gpio_init);
}

void srv::ProtocolImpl::send() {
	// Send whatever is in the dma_out_buffer
	if (is_sending) return;
	is_sending = true;

	LL_DMA_ConfigAddresses(UART_DMA, UART_DMA_TX_Stream, 
			(uint32_t)(dma_out_buffer),
			LL_USART_DMA_GetRegAddr(ESP_USART),
			LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
	LL_DMA_SetDataLength(UART_DMA, UART_DMA_TX_Stream, dma_out_buffer[1] + 3);

	LL_DMA_DisableIT_HT(UART_DMA, UART_DMA_TX_Stream);
	LL_DMA_EnableIT_TC(UART_DMA, UART_DMA_TX_Stream);
	
	LL_USART_Enable(ESP_USART);
	LL_USART_ClearFlag_TC(ESP_USART);
	LL_DMA_EnableStream(UART_DMA, UART_DMA_TX_Stream);

	LL_USART_EnableDMAReq_TX(ESP_USART);
}

void srv::ProtocolImpl::start_recv(srv::ProtocolState target_state, int offset) {
	state = target_state;
	// setup a receieve of 3 bytes

	LL_DMA_ConfigAddresses(UART_DMA, UART_DMA_RX_Stream, 
			LL_USART_DMA_GetRegAddr(ESP_USART),
			(uint32_t)(this->dma_buffer + offset),
			LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

	LL_DMA_SetDataLength(UART_DMA, UART_DMA_RX_Stream, 3 - offset);

	LL_DMA_EnableIT_TC(UART_DMA, UART_DMA_RX_Stream);
	LL_DMA_EnableIT_TE(UART_DMA, UART_DMA_RX_Stream);

	NVIC_SRV_RXE_CLRF(UART_DMA);
	NVIC_SRV_RX_CLRF(UART_DMA);

	LL_USART_ClearFlag_RXNE(ESP_USART);
	LL_USART_ClearFlag_LBD(ESP_USART);
	LL_USART_EnableDMAReq_RX(ESP_USART);
	LL_DMA_EnableStream(UART_DMA, UART_DMA_RX_Stream);
}

void srv::ProtocolImpl::recv_full() {
	state = ProtocolState::DMA_GOING;
	// setup a recieve similar to out_buffer

	LL_DMA_ConfigAddresses(UART_DMA, UART_DMA_RX_Stream, 
			LL_USART_DMA_GetRegAddr(ESP_USART),
			(uint32_t)(&this->dma_buffer[3]),
			LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

	LL_DMA_SetDataLength(UART_DMA, UART_DMA_RX_Stream, dma_buffer[1]);

	LL_DMA_EnableIT_TC(UART_DMA, UART_DMA_RX_Stream);
	LL_DMA_EnableIT_TE(UART_DMA, UART_DMA_RX_Stream);

	LL_USART_ClearFlag_RXNE(ESP_USART);
	LL_USART_EnableDMAReq_RX(ESP_USART);
	LL_DMA_EnableStream(UART_DMA, UART_DMA_RX_Stream);
}
