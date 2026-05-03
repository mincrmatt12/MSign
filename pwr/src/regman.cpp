#include "regman.h"
#include "ucpd.h"

#include <algorithm>

#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_gpio.h>

namespace regman {
	template<Tickable ...Ts>
	TickType_t Tickables<Ts...>::tick_all_and_get_deadline() {
		std::optional<TickType_t> deadline = std::nullopt;
		auto now = xTaskGetTickCount();

		((deadline = std::get<Ts>(storage).tick().transform([&deadline](TickType_t d){
			return std::min(deadline.value_or(portMAX_DELAY), d);
		}).or_else([deadline](){return deadline;})), ...);

		return deadline.transform([now](TickType_t before){return before - now;}).value_or(portMAX_DELAY);
	}

	// Mux:
	// 
	// PC13 -> ^EN_5VREG (active _low_)
	// PD0  ->  EN_VBUS (active high)

	MuxController::MuxController() {
		LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC | LL_IOP_GRP1_PERIPH_GPIOD);
		LL_GPIO_InitTypeDef init = {
			.Pin = LL_GPIO_PIN_13,
			.Mode = LL_GPIO_MODE_OUTPUT,
			.Speed = LL_GPIO_SPEED_FREQ_LOW,
			.OutputType = LL_GPIO_OUTPUT_PUSHPULL,
			.Pull = LL_GPIO_PULL_NO,
		};

		LL_GPIO_Init(GPIOC, &init);
		init.Pin = LL_GPIO_PIN_0;
		LL_GPIO_Init(GPIOD, &init);

		sync(Off);
	}

	void MuxController::sync(Mode m) {
		switch (m) {
			case Off:
				LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_13);
				LL_GPIO_ResetOutputPin(GPIOD, LL_GPIO_PIN_0);
				break;
			case Regulated:
				LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_13);
				LL_GPIO_ResetOutputPin(GPIOD, LL_GPIO_PIN_0);
				break;
			case Fused:
				LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_13);
				LL_GPIO_SetOutputPin(GPIOD, LL_GPIO_PIN_0);
				break;
		}
	}

	void MuxController::switch_to(Mode m) {
		if (is_switching() || current != Off) {
			if (!is_switching()) {
				cooldown = xTaskGetTickCount() + pdMS_TO_TICKS(10);
			}

			desired = m;
		}
		else {
			current = desired = m;
			sync(m);
		}
	}

	std::optional<TickType_t> MuxController::tick() {
		if (!is_switching())
			return std::nullopt;

		if (xTaskGetTickCount() > cooldown) {
			switch_to(desired);
			current = desired;
			return std::nullopt;
		}
		else {
			return cooldown;
		}
	}

	bool RegulatorManager::switch_output_mux(MuxController::Mode m, TickType_t queue_wait) {
		Req r {
			.kind = Req::SwitchOutputMux,
			.mux_mode = m
		};
		return xQueueSendToBack(reqs, &r, queue_wait) == pdTRUE;
	}

	void RegulatorManager::run() {
		// start next tasks after setting up ucpd
		ucpd::strobe_preinit();

		Req pr;

		while (true) {
			TickType_t delay = subtasks.tick_all_and_get_deadline();
			if (!xQueueReceive(reqs, &pr, delay))
				continue;

			switch (pr.kind) {
				case Req::SwitchOutputMux:
					subtasks.get<MuxController>().switch_to(pr.mux_mode);
					break;
			}
		}
	}

	util::manual_box<RegulatorManager> man;
}
