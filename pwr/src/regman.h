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

	extern "C" void I2C1_IRQHandler();

	struct BuckController {
		BuckController();

		std::optional<TickType_t> tick();

		struct LimitConfig {
			uint32_t expected_vin;
			uint32_t max_iin;
		};

		void change_config(LimitConfig new_cfg);
		bool is_enabled() const;

		void set_enabled(bool on);
	private:
		struct RSeq {
			uint8_t cmd;
			uint8_t length;
			bool is_read;
			enum Mode : uint8_t {
				Raw,
				OffsetPtr,
			} dat_mode;
			union {
				uint32_t raw_little_endian;
				uint8_t raw[4];

				uint8_t  BuckController::* offset;
				uint16_t BuckController::* offset_word;
			};
		};

		const RSeq *current_seq {};
		enum RSeqState : uint8_t  {
			SeqStart,
			SeqCmd,
			SeqWrite,
			SeqRead,
			SeqStop,
		} seq_async_op = SeqStart;
		uint8_t seq_async_subop{};
		bool tick_pending = false;

		enum PollAction : uint8_t {
			PollIdle,
			PollStatus,
			PollStatusWide,
			PollTempDisabling,
			PollUpdatingOperation,
			PollWritingConfig
		} poll_action = PollIdle;

		std::optional<LimitConfig> pending, now;

		uint16_t last_vin{};
		uint16_t last_temperature{};
		uint16_t last_vout{};
		uint16_t last_iout{};
		uint16_t last_iin{};
		uint16_t last_status{};

		TickType_t next_status_tick{};

		bool desired_enabled = false, enabled_now = false;

		bool i2c_isr();

		bool start_sequence(const RSeq *seq, bool tick=false);

		void activate_next_seq();

		uint8_t & current_payload(uint8_t idx);

		void start_write_cmd(uint8_t cmd, uint8_t length);
		void start_read_cmd(uint8_t cmd);
		void continue_read_cmd(uint8_t length);

		friend void I2C1_IRQHandler();
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
		bool request_tick_from_isr();

		struct Req {
			enum {
				SwitchOutputMux,
				Tick,
			} kind;

			union {
				MuxController::Mode mux_mode;
			};
		};

		StaticQueue_t reqs_priv;
		uint8_t reqs_priv_dat[sizeof(Req) * 32];
		QueueHandle_t reqs = xQueueCreateStatic(32, sizeof(Req), reqs_priv_dat, &reqs_priv);

		Tickables<
			MuxController,
			BuckController
		> subtasks;

		friend BuckController;
		friend void I2C1_IRQHandler();
	};

	extern util::manual_box<regman::RegulatorManager> man;
}
