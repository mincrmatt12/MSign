#include "regman.h"
#include "portmacro.h"
#include "tskmem.h"
#include "ucpd.h"

#include <algorithm>

#include <optional>
#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_gpio.h>
#include <stm32g0xx_ll_i2c.h>

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
				sync(Off);
				current = Off;
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

	bool RegulatorManager::request_tick_from_isr() {
		Req r {
			.kind = Req::Tick,
		};
		BaseType_t higher;
		xQueueSendToBackFromISR(reqs, &r, &higher);
		return higher == pdTRUE;
	}

	constexpr inline static uint8_t PMBUS_OPERATION = 0x01;
	constexpr inline static uint8_t PMBUS_ON_OFF_CONFIGURATION = 0x02;
	constexpr inline static uint8_t PMBUS_CLEAR_FAULTS = 0x03;
	constexpr inline static uint8_t PMBUS_SMBALERT_MASK = 0x1B;
	constexpr inline static uint8_t PMBUS_VOUT_COMMAND = 0x21;
	constexpr inline static uint8_t PMBUS_VOUT_TRIM = 0x22;
	constexpr inline static uint8_t PMBUS_VOUT_MAX = 0x24;
	constexpr inline static uint8_t PMBUS_VOUT_MARGIN_HIGH = 0x25;
	constexpr inline static uint8_t PMBUS_VOUT_MARGIN_LOW = 0x26;
	constexpr inline static uint8_t PMBUS_VOUT_SCALE_LOOP = 0x29;
	constexpr inline static uint8_t PMBUS_VIN_ON = 0x35;
	constexpr inline static uint8_t PMBUS_VIN_OFF = 0x36;
	constexpr inline static uint8_t PMBUS_VOUT_OV_FAULT_LIMIT = 0x40;
	constexpr inline static uint8_t PMBUS_VOUT_OV_WARN_LIMIT = 0x42;
	constexpr inline static uint8_t PMBUS_VOUT_UV_WARN_LIMIT = 0x43;
	constexpr inline static uint8_t PMBUS_VOUT_UV_FAULT_LIMIT = 0x44;
	constexpr inline static uint8_t PMBUS_IOUT_OC_FAULT_LIMIT = 0x46;
	constexpr inline static uint8_t PMBUS_IOUT_OC_WARN_LIMIT = 0x4A;
	constexpr inline static uint8_t PMBUS_VIN_OV_FAULT_LIMIT = 0x55;
	constexpr inline static uint8_t PMBUS_VIN_UV_WARN_LIMIT = 0x58;
	constexpr inline static uint8_t PMBUS_IIN_OC_WARN_LIMIT = 0x5D;
	constexpr inline static uint8_t PMBUS_POWER_GOOD_ON = 0x5E;
	constexpr inline static uint8_t PMBUS_POWER_GOOD_OFF = 0x5F;
	constexpr inline static uint8_t PMBUS_STATUS_BYTE = 0x78;
	constexpr inline static uint8_t PMBUS_STATUS_WORD = 0x79;
	constexpr inline static uint8_t PMBUS_STATUS_VOUT = 0x7A;
	constexpr inline static uint8_t PMBUS_STATUS_IOUT = 0x7B;
	constexpr inline static uint8_t PMBUS_STATUS_INPUT = 0x7C;
	constexpr inline static uint8_t PMBUS_STATUS_TEMPERATURE = 0x7D;
	constexpr inline static uint8_t PMBUS_READ_VIN = 0x88;
	constexpr inline static uint8_t PMBUS_READ_IIN = 0x89;
	constexpr inline static uint8_t PMBUS_READ_VOUT = 0x8B;
	constexpr inline static uint8_t PMBUS_READ_IOUT = 0x8C;
	constexpr inline static uint8_t PMBUS_READ_TEMPERATURE = 0x8D;
	constexpr inline static uint8_t PMBUS_READ_DUTY_CYCLE = 0x94;
	constexpr inline static uint8_t PMBUS_READ_POUT = 0x96;
	constexpr inline static uint8_t PMBUS_READ_PIN = 0x97;

	constexpr inline static uint8_t SIC_ADDR = 0x1D << 1;

	// Buck:
	// 
	// PA1 -> ALERT
    // PB7 -> SDA
    // PB8 -> SCL

	consteval inline static uint16_t operator ""_ul16(unsigned long long millivolts) {
		return (millivolts << 9) / 1000;
	}

	BuckController::BuckController() {
		LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA | LL_IOP_GRP1_PERIPH_GPIOB);
		{
			LL_GPIO_InitTypeDef init = {
				.Pin = LL_GPIO_PIN_7 | LL_GPIO_PIN_8,
				.Mode = LL_GPIO_MODE_ALTERNATE,
				.Speed = LL_GPIO_SPEED_FREQ_LOW,
				.OutputType = LL_GPIO_OUTPUT_OPENDRAIN,
				.Pull = LL_GPIO_PULL_UP,
				.Alternate = LL_GPIO_AF_6
			};

			LL_GPIO_Init(GPIOB, &init);
		}

		LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
		{
			LL_I2C_InitTypeDef init = {
				.PeripheralMode = LL_I2C_MODE_SMBUS_HOST,
				.Timing = __LL_I2C_CONVERT_TIMINGS(0x1, 0x3, 0x2, 0x3, 0x9), // from datasheet
				.AnalogFilter = LL_I2C_ANALOGFILTER_ENABLE,
				.DigitalFilter = 0,
				.OwnAddress1 = 0,
				.TypeAcknowledge = LL_I2C_ACK,
				.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT,
			};

			LL_I2C_EnableSMBusPEC(I2C1);
			LL_I2C_Init(I2C1, &init);

			I2C1->ICR = ~0u;
			LL_I2C_EnableIT_STOP(I2C1);
			LL_I2C_EnableIT_NACK(I2C1);
			LL_I2C_EnableIT_RX(I2C1);
			LL_I2C_EnableIT_TX(I2C1);
			LL_I2C_EnableIT_TC(I2C1);
			LL_I2C_EnableIT_ERR(I2C1);
		}

		NVIC_SetPriority(I2C1_IRQn, 1);
		NVIC_EnableIRQ(I2C1_IRQn);

		// perform a bulk register write to setup the thing for 5v out, ignoring EN signal; do not
		// set it up with vin limits since those will come from the ucpd
		
		using enum RSeq::Mode;

		const static RSeq init_ops[] = {
			// 0x88 is default, 0x08 turns off ON flag
			{ .cmd = PMBUS_OPERATION, .length = 1, .is_read = false, .dat_mode = Raw, .raw_little_endian = 0x08 },
			// clear EN and set CMD - require OPERATION to turn on the regulator
			{ .cmd = PMBUS_ON_OFF_CONFIGURATION, .length = 1, .is_read = false, .dat_mode = Raw, .raw_little_endian = 0b00011011 },
			{ .cmd = PMBUS_VOUT_MAX, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 6000_ul16 },
			{ .cmd = PMBUS_VOUT_COMMAND, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 5000_ul16 },
			{ .cmd = PMBUS_VOUT_MARGIN_LOW, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 4750_ul16 },
			{ .cmd = PMBUS_VOUT_MARGIN_HIGH, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 5250_ul16 },
			// from datasheet, 0.25
			{ .cmd = PMBUS_VOUT_SCALE_LOOP, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 0xE802 },
			{ .cmd = PMBUS_VOUT_OV_FAULT_LIMIT, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 5750_ul16 },
			{ .cmd = PMBUS_VOUT_OV_WARN_LIMIT, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 5500_ul16 },
			{ .cmd = PMBUS_VOUT_UV_WARN_LIMIT, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 4500_ul16 },
			{ .cmd = PMBUS_VOUT_UV_FAULT_LIMIT, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 4000_ul16 },
			{ .cmd = PMBUS_POWER_GOOD_ON, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 4500_ul16 },
			{ .cmd = PMBUS_POWER_GOOD_OFF, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = 4250_ul16 },

			// temp
			{ .cmd = PMBUS_VIN_OFF, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = ((0b11111) << 11) | (2 * 13) },
			{ .cmd = PMBUS_VIN_ON, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = ((0b11111) << 11) | (2 * 14) },

			{ .cmd = PMBUS_IOUT_OC_FAULT_LIMIT, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = ((0b11111) << 11) | (2 * 10) },
			{ .cmd = PMBUS_IOUT_OC_WARN_LIMIT, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = ((0b11111) << 11) | (1 + 2 * 8) },
			{ .cmd = PMBUS_VIN_UV_WARN_LIMIT, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = ((0b11111) << 11) | (2 * 13) },
			{ .cmd = PMBUS_VIN_OV_FAULT_LIMIT, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = ((0b11111) << 11) | (2 * 17) },
			{ .cmd = PMBUS_IIN_OC_WARN_LIMIT, .length = 2, .is_read = false, .dat_mode = Raw, .raw_little_endian = ((0b11111) << 11) | (2 * 3) },

			// turn on
			{ .cmd = PMBUS_OPERATION, .length = 1, .is_read = false, .dat_mode = Raw, .raw_little_endian = 0x88 },
			// terminator
			{},
		};

		start_sequence(init_ops);

		// perform initialization synchronously
		while (current_seq) 
			vTaskDelay(1);
	}

	void BuckController::start_write_cmd(uint8_t cmd, uint8_t length) {
		LL_I2C_HandleTransfer(
			I2C1,
			SIC_ADDR,
			LL_I2C_ADDRSLAVE_7BIT,
			2 + length,
			LL_I2C_MODE_SMBUS_AUTOEND_WITH_PEC,
			LL_I2C_GENERATE_START_WRITE);
	}

	void BuckController::start_read_cmd(uint8_t cmd) {
		LL_I2C_HandleTransfer(
			I2C1,
			SIC_ADDR,
			LL_I2C_ADDRSLAVE_7BIT,
			1,
			LL_I2C_MODE_SMBUS_SOFTEND_NO_PEC,
			LL_I2C_GENERATE_START_WRITE);
	}

	void BuckController::continue_read_cmd(uint8_t length) {
		LL_I2C_HandleTransfer(
			I2C1,
			SIC_ADDR,
			LL_I2C_ADDRSLAVE_7BIT,
			length + 1,
			LL_I2C_MODE_SMBUS_AUTOEND_WITH_PEC,
			LL_I2C_GENERATE_RESTART_7BIT_READ
		);
	}

	bool BuckController::i2c_isr() {
		auto flags = I2C1->ISR;
		I2C1->ICR = flags & (I2C_ICR_ADDRCF | I2C_ICR_ALERTCF | I2C_ICR_ARLOCF | I2C_ICR_PECCF | I2C_ICR_BERRCF | I2C_ICR_NACKCF | I2C_ICR_OVRCF | I2C_ICR_STOPCF | I2C_ICR_TIMOUTCF);

		bool higher_prio_woken = false;

		if (!current_seq)
			return false;

		if (flags & I2C_ISR_NACKF) {
			// TODO: NACK received; explode?
		}

		if (flags & I2C_ISR_PECERR) {
			// TODO: PEC is wrong; explode?
		}

		if (flags & I2C_ISR_TXIS) {
			// something just finished transmitting
			switch (seq_async_op) {
				case SeqStart:
					// just finished sending address, write cmd
					LL_I2C_TransmitData8(I2C1, current_seq->cmd);
					seq_async_op = SeqCmd;
					break;
				case SeqCmd:
					if (current_seq->is_read) {
						// todo: panic; this should not happen; expecting TC instead
						break;
					}
					seq_async_op = SeqWrite;
					seq_async_subop = 0;
					[[fallthrough]];
				case SeqWrite:
					LL_I2C_TransmitData8(I2C1, current_payload(seq_async_subop++));
					if (seq_async_subop == current_seq->length) {
						seq_async_op = SeqStop;
					}
					break;
				default:
					// not expecting in other states
					break;
			}
		}

		if (flags & I2C_ISR_RXNE) {
			switch (seq_async_op) {
				case SeqRead:
					if (seq_async_subop >= current_seq->length) {
						seq_async_op = SeqStop;
						break;
					}
					current_payload(seq_async_subop++) = LL_I2C_ReceiveData8(I2C1);
					break;
				case SeqStop:
					(void)LL_I2C_ReceiveData8(I2C1);
					break;
				default:
					// not expecting in other states
					break;
			}
		}

		if (flags & I2C_ISR_STOPF) {
			switch (seq_async_op) {
				case SeqStop:
					++current_seq;
					if (current_seq->cmd == 0) {
						current_seq = nullptr;
						if (tick_pending) {
							higher_prio_woken |= man->request_tick_from_isr();
							tick_pending = false;
						}
					}
					else {
						// transfer done; go to next one
						activate_next_seq();
					}
					break;
				case SeqCmd:
					break; // waiting for TC instead
				default:
					// not expecting in other states
					break;
			}
		}

		if (flags & I2C_ISR_TC) {
			switch (seq_async_op) {
				case SeqCmd:
					continue_read_cmd(current_seq->length);
					seq_async_op = SeqRead;
					seq_async_subop = 0;
					break;
				default:
					break;
			}
		}

		return higher_prio_woken;
	}

	extern "C" void I2C1_IRQHandler() {
		portYIELD_FROM_ISR(man->subtasks.get<BuckController>().i2c_isr());
	}

	void BuckController::activate_next_seq() {
		seq_async_op = SeqStart;

		if (!current_seq->is_read) {
			start_write_cmd(current_seq->cmd, current_seq->length);
		}
		else {
			start_read_cmd(current_seq->cmd);
		}
	}

	std::optional<TickType_t> BuckController::tick() {
		return std::nullopt;
	}

	uint8_t& BuckController::current_payload(uint8_t offset) {
		switch (current_seq->dat_mode) {
			case RSeq::Raw:
				return const_cast<uint8_t &>(current_seq->raw[offset]);
			case RSeq::OffsetPtr:
				return (&(this->*current_seq->offset))[offset];
			default:
				__builtin_unreachable();
		}
	}

	bool BuckController::start_sequence(const RSeq *seq, bool tick) {
		if (current_seq)
			return false;

		current_seq = seq;
		tick_pending = tick;

		activate_next_seq();
		return true;
	}

	void RegulatorManager::run() {
		// start next tasks after setting up ucpd
		ucpd::strobe_preinit();

		tskmem::ucpd.create(ucpd::port, "ucpd", 2);

		Req pr;
		while (true) {
			TickType_t delay = subtasks.tick_all_and_get_deadline();
			if (!xQueueReceive(reqs, &pr, delay))
				continue;

			switch (pr.kind) {
				case Req::SwitchOutputMux:
					subtasks.get<MuxController>().switch_to(pr.mux_mode);
					break;
				case Req::Tick:
					break;
			}
		}
	}

	util::manual_box<RegulatorManager> man;
}
