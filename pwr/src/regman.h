#pragma once

#include "manual_box.h"
#include <FreeRTOS.h>
#include <optional>
#include <tuple>
#include <task.h>
#include <queue.h>

namespace regman {
	template<typename T>
	concept Tickable = requires(T& t) {
		{ t.tick() } -> std::same_as<std::optional<TickType_t>>;
	};

	struct MuxController {
		enum Mode {
			Off,
			Regulated,
			Fused
		};

		MuxController();

		std::optional<TickType_t> tick();

		Mode mode() const { return current; }
		Mode desired_mode() const { return desired; }

		bool is_switching() const { return current != desired; }
		void switch_to(Mode m);
	private:
		void sync(Mode m);

		TickType_t cooldown = 0;
		Mode current = Off, desired = Off;
	};

	template<Tickable ...Ts>
	struct Tickables final {
		std::tuple<Ts...> storage;

		TickType_t tick_all_and_get_deadline();

		template<Tickable T>
		decltype(auto) get(this auto&& self) {
			return std::get<T>(self.storage);
		}
	};

	struct RegulatorManager {
		const MuxController& mux() const { return subtasks.get<MuxController>(); }

		bool switch_output_mux(MuxController::Mode m, TickType_t queue_wait=portMAX_DELAY);

		void run();
	private:
		struct Req {
			enum {
				SwitchOutputMux
			} kind;

			union {
				MuxController::Mode mux_mode;
			};
		};

		StaticQueue_t reqs_priv;
		uint8_t reqs_priv_dat[sizeof(Req) * 32];
		QueueHandle_t reqs;

		Tickables<
			MuxController
		> subtasks;
	};

	extern util::manual_box<regman::RegulatorManager> man;
}
