#include "protocol.h"
#include <sys/ioctl.h>
#include <termios.h>
#include <iostream>
#include "srv.h"

#include <FreeRTOS.h>
#include <task.h>

uint32_t faux_dma_counter = 0;
uint8_t * faux_dma_ptr = 0;

#include "tasks/timekeeper.h"
#include "stm32f2xx_ll_system.h"

extern tasks::Timekeeper timekeeper;
extern srv::Servicer servicer;

void pump_faux_dma_task(void*) {
	while (true) {
		// handle fauxdma:tm:
		if (!(faux_dma_ptr == 0 || faux_dma_counter == 0)) {
			portENTER_CRITICAL();
			int bytes_waiting;
			ioctl(0, FIONREAD, &bytes_waiting);
			while (bytes_waiting--) {
				if (faux_dma_ptr == 0) break;
				*faux_dma_ptr++ = getchar();
				if (!--faux_dma_counter) {
					faux_dma_ptr = 0;
					servicer.dma_finish(true);
				}
			}
			portEXIT_CRITICAL();
		}

		vTaskDelay(pdMS_TO_TICKS(5));
	}
}

void srv::ProtocolImpl::dma_finish(bool incoming) {
	if (incoming) {
		// check for error
		last_comm = timekeeper.current_time;
		sent_ping = false;
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
				start_recv();
				return;
			}
			
			recv_full();
		}
		else {
			process_command();
			start_recv();
		}
	}
	else {
		is_sending = false;
	}
}

void srv::ProtocolImpl::setup_uart_dma() {
	// Setup fake UART with pipes
	termios term;
    tcgetattr(0, &term);
    term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
    tcsetattr(0, TCSANOW, &term);
}

void srv::ProtocolImpl::send() {
	// Send whatever is in the dma_out_buffer
	std::cerr.write((char *)dma_out_buffer, dma_out_buffer[1] + 3);
	is_sending = false;
}

void srv::ProtocolImpl::start_recv(srv::ProtocolState s) {
	state = s;
	
	faux_dma_ptr = dma_buffer;
	faux_dma_counter = 3;
}

void srv::ProtocolImpl::recv_full() {
	state = ProtocolState::DMA_GOING;

	faux_dma_ptr = dma_buffer + 3;
	faux_dma_counter = dma_buffer[1];
}
