// mock various ll_usart calls

#include "pins.h"

#ifndef MOCK_USART_H
#define MOCK_USART_H

void LL_USART_ClearFlag_ORE(void *) {};
void LL_USART_ClearFlag_PE(void *) {};

#define ESP_USART nullptr

#endif
