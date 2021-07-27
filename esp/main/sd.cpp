#include "sd.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <esp8266/gpio_struct.h>
#include <gpio.h>
#include <esp_log.h>

#include <FreeRTOS.h>
#include <portmacro.h>

#include <type_traits>
#include <task.h>

#include <ff.h>
#include <diskio.h>

#define nop asm volatile ("nop\n\t")

static const char* TAG = "diskio_sd";

namespace sd {
	FATFS fs;

	static struct Card {
		enum Type {
			CardTypeSDVer1,
			CardTypeSDVer2OrLater,
			CardTypeSDHC,
			CardTypeUndefined
		} type = CardTypeUndefined;

		uint64_t length = 0;
		TickType_t timeout = 25;

		bool write_protect;
		bool perm_write_protect;
	} card;

	// Helper class for bit definitions inside a struct
	// You should use this class by placing it in a union with a member of size Total bits
	template<std::size_t Total, std::size_t Index, std::size_t LocalSize, typename AccessType=std::conditional_t<LocalSize == 1, bool, uint32_t>>
	struct __attribute__((packed)) reg_bit {
		typedef AccessType access_type;
		static_assert(Total % 8 == 0, "Total size must align to byte");
		static_assert(LocalSize < sizeof(AccessType) * 8, "Local size must fit inside the access type");
		static_assert(LocalSize < 32, "Local size must be less than 32 bits");
		static_assert(Index < Total, "Index must be inside struct");

		inline operator access_type() const {
			return static_cast<access_type>((*(reinterpret_cast<const uint32_t *>(data + (Index / 8))) >> (Index % 8)) & ((1U << LocalSize) - 1));
		}

		inline reg_bit& operator=(const access_type& other) {
			// I have no idea why gcc thinks "argument" is unitialized given that nothing named that exists here, but uh ok.
			// I think it's complaining about me using this somewhere else in the code and getting confused from inlining but uh no idea.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
			*(reinterpret_cast<uint32_t *>(data + (Index / 8))) &= ~(((1U << LocalSize) - 1) << (Index % 8));
			*(reinterpret_cast<uint32_t *>(data + (Index / 8))) |= (static_cast<uint32_t>(other) << (Index % 8));
#pragma GCC diagnostic pop 
			return *this;
		}
		
	private:
		uint8_t data[Total / 8];
	};

	template<std::size_t Index, std::size_t LocalSize>
	using u8_bit = reg_bit<8, Index, LocalSize>;

	template<std::size_t Index, std::size_t LocalSize>
	using u16_bit = reg_bit<16, Index, LocalSize>;

	template<std::size_t Index, std::size_t LocalSize>
	using u32_bit = reg_bit<32, Index, LocalSize>;

	template<std::size_t Index, std::size_t LocalSize>
	using u128_bit = reg_bit<128, Index, LocalSize>;

	struct no_response {};

	struct __attribute__((packed)) status_r1 {
		union __attribute__((packed)) {
			uint8_t raw_data;

			u8_bit<0, 1> idle;
			u8_bit<1, 1> erase_reset;
			u8_bit<2, 1> illegal_command;
			u8_bit<3, 1> com_crc_error;
			u8_bit<4, 1> erase_seq_error;
			u8_bit<5, 1> address_error;
			u8_bit<6, 1> parameter_error;
		};
	};

	struct status_r2 {
		union {
			uint8_t raw_data[2];

			u16_bit<0, 1> card_is_locked;
			u16_bit<1, 1> lock_unlock_failed;
			u16_bit<2, 1> general_error;
			u16_bit<3, 1> cc_error;
			u16_bit<4, 1> card_ecc_failed;
			u16_bit<5, 1> wp_violation;
			u16_bit<6, 1> erase_param_error;
			u16_bit<7, 1> out_of_range;
			u16_bit<8, 1> idle;
			u16_bit<9, 1> erase_reset;
			u16_bit<10, 1> illegal_command;
			u16_bit<11, 1> com_crc_error;
			u16_bit<12, 1> erase_seq_error;
			u16_bit<13, 1> address_error;
			u16_bit<14, 1> parameter_error;
		};
	};

	struct __attribute__((packed)) status_r7 {
		union __attribute__((packed)) argument {
			uint32_t raw_data;

			u32_bit<0, 8> check_pattern;
			u32_bit<8, 4> voltage_accepted;
			u32_bit<28, 4> command_version;
		} response;

		status_r1 status;
	};

	struct send_op_cond_argument {
		union {
			uint32_t raw_data = 0;

			u32_bit<30, 1> hcs;
		};
	};

	struct scr_register {
		union {
			uint64_t raw_data;

			// 0 - CMD20
			// 1 - CMD23
			// 2 - CMD48/49
			// 3 - CMD58/59
			reg_bit<64, 32, 4> cmd_support;
			reg_bit<64, 38, 4> sd_specx;
			reg_bit<64, 42, 1> sd_spec4;
			reg_bit<64, 43, 4> ex_security;
			reg_bit<64, 47, 1> sd_spec3;
			reg_bit<64, 48, 4> sd_bus_widths;
			reg_bit<64, 52, 3> sd_security;
			reg_bit<64, 55, 1> data_stat_after_erase;
			reg_bit<64, 46, 4> sd_spec;
			reg_bit<64, 60, 4> scr_structure_ver;
		};
	};

	struct __attribute__((packed)) status_r3 {
		union __attribute__((packed)) {
			uint32_t raw_data;
			
			u32_bit<0, 24> vdd_voltage;
			u32_bit<24, 1> switch_v18_acc;
			u32_bit<29, 1> uhsII_card_status;
			u32_bit<30, 1> card_capacity_status;
			u32_bit<31, 1> busy;
		} response;

		status_r1 status;
	};

	struct csd_register {
		enum CsdStructureVersion {
			CsdVersion1SC = 0,
			CsdVersion2HCXC = 1
		};

		union {
			uint32_t data[4];

			reg_bit<128, 126, 2, CsdStructureVersion> csd_structure_ver;

			// Common parameters

			u128_bit<112, 8> taac;
			u128_bit<104, 8> nsac;
			u128_bit<96, 8> transfer_speed;
			u128_bit<84, 12> card_command_classes;
			u128_bit<80, 4> read_bl_len;
			u128_bit<79, 1> read_bl_partial;
			u128_bit<78, 1> write_blk_misalign;
			u128_bit<77, 1> read_blk_misalign;
			u128_bit<76, 1> dsr_implemented;

			u128_bit<46, 3> erase_blk_en;
			u128_bit<39, 7> erase_sector_size;

			u128_bit<32, 7> wp_grp_size;
			u128_bit<31, 1> wp_grp_enable;

			u128_bit<26, 3> write_speed_factor;
			u128_bit<22, 4> write_bl_len;
			u128_bit<21, 1> write_bl_partial;

			u128_bit<15, 1> file_format_grp;
			u128_bit<14, 1> copy;
			u128_bit<13, 1> perm_write_protect;
			u128_bit<12, 1> tmp_write_protect;
			u128_bit<10, 2> file_format;

			u128_bit<1, 7> crc7;

			union {
				u128_bit<62, 12> card_size;
				u128_bit<59, 3> vdd_r_curr_min;
				u128_bit<56, 3> vdd_r_curr_max;
				u128_bit<53, 3> vdd_w_curr_min;
				u128_bit<50, 3> vdd_w_curr_max;
				u128_bit<47, 3> card_size_mult;
			} v1;

			union {
				u128_bit<48, 22> card_size;
			} v2;
		};
	};

	namespace spi {
		template<int num>
		inline void clbit() {
			GPIO.out_w1tc = (1 << num);
		}

		template<int num>
		inline void wrbit() {
			GPIO.out_w1ts = (1 << num);
		}

		template<int num>
		inline void sbit(bool bit) {
			if (bit)
				wrbit<num>();
			else
				clbit<num>();
		}

		template<int num>
		inline auto rbit() {
			return (GPIO.in >> num) & 0x1;
		}

		inline void select() { clbit<CONFIG_SDCARD_CS_IO>(); }
		inline void deselect() { wrbit<CONFIG_SDCARD_CS_IO>(); }

		void init() {
			gpio_config_t cfg;
			cfg.intr_type = GPIO_INTR_DISABLE;
			cfg.mode = GPIO_MODE_OUTPUT;
			cfg.pull_up_en = GPIO_PULLUP_DISABLE;
			cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
			cfg.pin_bit_mask = (1 << CONFIG_SDCARD_CS_IO) | (1 << CONFIG_SDCARD_CLK_IO) | (1 << CONFIG_SDCARD_MOSI_IO);
			gpio_config(&cfg);
			cfg.mode = GPIO_MODE_INPUT;
			cfg.pull_up_en = GPIO_PULLUP_ENABLE;
			cfg.pin_bit_mask = (1 << CONFIG_SDCARD_MISO_IO);
			gpio_config(&cfg);

			wrbit<CONFIG_SDCARD_MOSI_IO>();
			clbit<CONFIG_SDCARD_CLK_IO>();
			deselect();

			ESP_LOGI("sdspi", "Initialized pins");
		}

		// Data goes MSB first

		inline __attribute__((always_inline)) void rx_bit(uint8_t &to) {
			// TIMING NOPS
			nop;
			nop;
			nop;
			// WRITE CLOCK HIGH
			wrbit<CONFIG_SDCARD_CLK_IO>();
			// SAMPLE BIT
			to <<= 1;
			to |= rbit<CONFIG_SDCARD_MISO_IO>();
			// WRITE CLOCK LOW
			clbit<CONFIG_SDCARD_CLK_IO>();
		}

		inline __attribute__((always_inline)) void tx_bit(uint8_t &from) {
			// Data is sampled 

			// WRITE BIT
			sbit<CONFIG_SDCARD_MOSI_IO>(from & (1 << 7));
			from <<= 1;
			// WRITE CLOCK HIGH
			wrbit<CONFIG_SDCARD_CLK_IO>();
			// TIMING NOPS
			nop;
			nop;
			nop;
			// WRITE CLOCK LOW
			clbit<CONFIG_SDCARD_CLK_IO>();
		}

		IRAM_ATTR void rx_byte(uint8_t& to) {
			to = 0;
			rx_bit(to);
			rx_bit(to);
			rx_bit(to);
			rx_bit(to);
			rx_bit(to);
			rx_bit(to);
			rx_bit(to);
			rx_bit(to);
		}

		IRAM_ATTR void tx_byte(uint8_t  from) {
			tx_bit(from);
			tx_bit(from);
			tx_bit(from);
			tx_bit(from);
			tx_bit(from);
			tx_bit(from);
			tx_bit(from);
			tx_bit(from);
		}

		inline void reselect() {
			portENTER_CRITICAL();
			spi::deselect();
			spi::tx_byte(0xff);
			spi::select();
			portEXIT_CRITICAL();
		}
	}

	enum struct command_status {
		Ok,
		TimeoutError
	};

	uint8_t crc7(const uint8_t* data, uint8_t n) {
		uint8_t crc = 0;
		for (uint8_t i = 0; i < n; i++) {
			uint8_t d = data[i];
			for (uint8_t j = 0; j < 8; j++) {
				crc <<= 1;
				if ((d & 0x80) ^ (crc & 0x80)) {
					crc ^= 0x09;
				}
				d <<= 1;
			}
		}
		return (crc << 1) | 1;
	}

	const uint16_t crc_ccitt_tab[] = {
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
		0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
		0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
		0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
		0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
		0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
		0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
		0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
		0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
		0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
		0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
		0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
		0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
		0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
		0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
		0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
		0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
		0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
		0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
		0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
		0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
		0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
		0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
		0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
		0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
		0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
		0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
	};

	uint16_t crc_ccitt(const uint8_t *data, size_t n) {
		uint16_t crc = 0;
		for (size_t i = 0; i < n; ++i) {
			crc = crc_ccitt_tab[(crc >> 8 ^ data[i]) & 0xFF] ^ (crc << 8);
		}
		return crc;
	}

	// Send a command to the SD.
	//
	template<bool B, typename Argument, typename Response>
	command_status send_command(Argument argument, uint32_t index, Response& response, TickType_t timeout=2) {
		uint8_t command_out[6];

		// Setup start bit + trans bit + index
		command_out[0] = 0x40U | index;

		if constexpr (!std::is_empty<Argument>::value) {
			static_assert(sizeof(Argument) == 4, "argument must be a 32-bit value");

			uint32_t argument_u32 = *reinterpret_cast<uint32_t *>(&argument);

			command_out[1] = argument_u32 >> 24;
			command_out[2] = argument_u32 >> 16;
			command_out[3] = argument_u32 >> 8;
			command_out[4] = argument_u32;
		}

		command_out[5] = crc7(command_out, 5);

		// Start a critical section to send/rx the command

		portENTER_CRITICAL();

		for (int i = 0; i < 6; ++i) spi::tx_byte(command_out[i]);

		portEXIT_CRITICAL();

		uint8_t *fillresp = reinterpret_cast<uint8_t *>(&response) + (sizeof(Response) - 1);
		// Discard first byte (not MMC)
		spi::rx_byte(*fillresp);

		TickType_t t0 = xTaskGetTickCount();

		*fillresp = 0xff;

		// Wait for 1-8 fill bytes
		while (*fillresp & 0x80) {
			if (xTaskGetTickCount() > (t0 + timeout)) return command_status::TimeoutError;
			portENTER_CRITICAL();
			spi::rx_byte(*fillresp);
			portEXIT_CRITICAL();
		}

		if (*fillresp & 0x80) {
			// Timeout error
			return command_status::TimeoutError;
		}

		// Receive remaining bytes of response
		for (int i = 1; i < sizeof(Response); ++i) {
			spi::rx_byte(fillresp[-i]);
		}

		if constexpr (B) {
			uint8_t busy = 0;
			TickType_t t0 = xTaskGetTickCount();

			while (!busy && xTaskGetTickCount() < (t0 + timeout)) {
				portENTER_CRITICAL();
				spi::rx_byte(busy);
				portEXIT_CRITICAL();
			}

			if (!busy) return command_status::TimeoutError;
		}

		return command_status::Ok;
	}

	template<bool B=false, typename Argument=uint32_t>
	inline command_status send_command(Argument argument, uint32_t index, TickType_t timeout=2) {
		status_r1 x;
		return send_command<B>(argument, index, x, timeout);
	}

	enum struct read_status {
		Ok,
		Timeout,
		CRCError,
		ProtocolError,
		OutOfBounds
	};

	// Do a single read transaction, writing the received data to buf
	read_status single_read(uint8_t *buf, size_t length, bool inverted=false) {
		// Wait for start block token

		TickType_t t0 = xTaskGetTickCount();
		uint8_t token = 0xff;

		while (token == 0xff && xTaskGetTickCount() < (t0 + card.timeout)) {
			portENTER_CRITICAL();
			spi::rx_byte(token);
			portEXIT_CRITICAL();
		}

		if (token == 0xFF) return read_status::Timeout;

		// DATA_START_TOKEN
		if (token != 0xFE) {
			return read_status::ProtocolError;
		}

		uint16_t crc = 0;

		// Receive the actual data into the provided buf
		portENTER_CRITICAL();
		for (int i = 0; i < length; ++i) {
			spi::rx_byte(buf[i]);
		}
		spi::rx_byte(token);
		crc = token;
		crc <<= 8;
		spi::rx_byte(token);
		crc |= token;
		portEXIT_CRITICAL();

		if (crc != crc_ccitt(buf, length)) {
			return read_status::CRCError;
		}

		if (inverted) std::reverse(buf, buf+length);

		return read_status::Ok;
	}

	// Do a single write transaction
	enum struct write_status {
		Ok,
		Timeout,
		CRCError,
		ProtocolError,
		WriteError,
		OutOfBounds
	};

	write_status single_write(const uint8_t *buf, size_t length, uint8_t token) {
		// First compute the CRC
		uint16_t crc = crc_ccitt(buf, length);

		portENTER_CRITICAL();
		spi::tx_byte(token);
		for (size_t i = 0; i < length; ++i) {
			spi::tx_byte(buf[i]);
		}
		spi::tx_byte(crc >> 8);
		spi::tx_byte(crc & 0xff);
		spi::wrbit<CONFIG_SDCARD_MOSI_IO>();
		spi::rx_byte(token);
		portEXIT_CRITICAL();

		switch ((token & 0b1110) >> 1) {
			case 0b010:
				return write_status::Ok;
			case 0b101:
				return write_status::CRCError;
			case 0b110:
				return write_status::WriteError;
			default:
				ESP_LOGE(TAG, "Protocol error during single_write %02x", token);
				return write_status::ProtocolError;
		}
	}

	read_status read(uint8_t *buf, uint32_t lba, size_t length_in_sectors) {
		if (card.type != Card::CardTypeSDHC) lba <<= 9; // old cards use address not sector
		status_r1 x;
		if (length_in_sectors == 1) {
			// Single read
			if (send_command<false>(lba, 17, x, card.timeout) != command_status::Ok) {
				// Failed to send
				return read_status::Timeout;
			}
			// Check for errors
			if (x.raw_data) {
				if (x.address_error) return read_status::OutOfBounds;
				else return read_status::ProtocolError;
			}
			// Proceed with the read
			return single_read(buf, 512);
		}
		else {
			// Otherwise, use multi-read
			if (send_command<false>(lba, 18, x, card.timeout) != command_status::Ok) {
				// Failed to send
				return read_status::Timeout;
			}
			// Check for errors
			if (x.raw_data) {
				if (x.address_error) return read_status::OutOfBounds;
				else return read_status::ProtocolError;
			}
			read_status s=read_status::OutOfBounds;

			for (size_t i = 0; i < length_in_sectors; ++i, buf += 512) {
				 s = single_read(buf, 512);
				 if (s != read_status::Ok) break;
			}
			
			// Stop the transmission
			if (send_command<true>(0, 12, x, card.timeout) != command_status::Ok) {
				return read_status::Timeout;
			}

			// Return the status
			return s;
		}
	}

	bool wait_for_not_busy() {
		uint8_t x = 0;
		TickType_t t0 = xTaskGetTickCount();
		while (x != 0xff && xTaskGetTickCount() < (t0 + card.timeout)) {
			portENTER_CRITICAL();
			spi::rx_byte(x);
			portEXIT_CRITICAL();
		}
		return x;
	}

	write_status write(const uint8_t *buf, uint32_t lba, size_t length_in_sectors) {
		if (card.type != Card::CardTypeSDHC) lba <<= 9; // old cards use address not sector
		status_r1 x;
		if (length_in_sectors == 1) {
			// Send a single write
			if (send_command<false>(lba, 24, x, card.timeout) != command_status::Ok) {
				ESP_LOGE(TAG, "Timeout during CMD24");
				return write_status::Timeout;
			}
			if (x.raw_data) {
				if (x.address_error) return write_status::OutOfBounds;
				else return write_status::ProtocolError;
			}
			// Send out the block data
			auto s = single_write(buf, 512, 0xFE);
			// Wait for not busy
			if (!wait_for_not_busy()) return write_status::Timeout;
			return s;
		}
		else {
			// Send a multi write
			if (send_command<false>(lba, 25, x, card.timeout) != command_status::Ok) {
				ESP_LOGE(TAG, "Timeout during CMD25");
				return write_status::Timeout;
			}
			if (x.raw_data) {
				if (x.address_error) return write_status::OutOfBounds;
				else {
					ESP_LOGE(TAG, "Protocol error during setup write %02x", x.raw_data);
					return write_status::ProtocolError;
				}
			}
			// Send out the block data
			write_status s=write_status::ProtocolError;
			
			for (size_t i = 0; i < length_in_sectors; ++i, buf += 512) {
				s = single_write(buf, 512, 0xFC);
				// Wait for not busy
				if (!wait_for_not_busy()) {
					ESP_LOGE(TAG, "Timeout during notbusy");
					return write_status::Timeout;
				}
				if (s != write_status::Ok) break;
			}
			
			// If we encountered an error, stop using CMD12
			if (s != write_status::Ok) {
				if (send_command<true>(0, 12, x, card.timeout) != command_status::Ok) {
					ESP_LOGE(TAG, "Timeout during cancel (%d)", (int)s);
					return write_status::Timeout;
				}

				return s;
			}
			// Otherwise, send an end token
			portENTER_CRITICAL();
			uint8_t token = 0xFD;
			spi::tx_byte(token);
			portEXIT_CRITICAL();

			if (!wait_for_not_busy()) {
				ESP_LOGE(TAG, "Timeout during last onbusy (%d)", (int)s);
				return write_status::Timeout;
			}
			return s;
		}
	}

	FIL logtarget{};
	char logbuf[300];
	int logptr = 0;
	putchar_like_t oldlogputchar;
	bool is_masking_code = false;

	int log_putc(int c) {
		oldlogputchar(c);
		if (c == 0x1b) {
			is_masking_code = true;
		}
		if (is_masking_code) {
			if (c == 'm') is_masking_code = false;
			return 1;
		}
		logbuf[logptr++] = c;
		if (logptr == sizeof(logbuf) || (c == '\n' && logptr > 50)) {
			flush_logs();
		}
		return 1;
	}

	void flush_logs() {
		if (logptr) {
			UINT bw = 0;
			FRESULT res = f_write(&logtarget, logbuf, logptr, &bw);
			f_sync(&logtarget);

			if (res == FR_OK) {
				// all is ok
				logptr = 0;
			}
			else {
				memmove(logbuf, logbuf + bw, logptr - bw);
				logptr -= bw;
			}

			if (logptr >= sizeof(logbuf)) {
				// dump logs
				logptr = 0;
			}
		}
	}

	void install_log() {
		f_mkdir("/log");
		{
			// Try and rotate log files, storing up to 2 previous entries
			DIR logdir; FILINFO fno;
			int maxn = 0;
			f_opendir(&logdir, "/log");
			while (f_readdir(&logdir, &fno) == FR_OK && fno.fname[0] != 0) {
				int num;
				if (sscanf(fno.fname, "log.%d", &num) != 1) continue;
				if (num > maxn) maxn = num;
			}
			f_closedir(&logdir);
			// rename files
			for (int i = maxn; i >= 0; --i) {
				char oldname[32], newname[32];
				snprintf(oldname, 32, "/log/log.%d", i);
				snprintf(newname, 32, "/log/log.%d", i+1);
				if (i > 3) {
					ESP_LOGW("sdlog", "deleting old log %s", oldname);
					f_unlink(oldname);
				}
				else {
					ESP_LOGI("sdlog", "moving old log %s -> %s", oldname, newname);
					f_rename(oldname, newname);
				}
			}
		}
		// Open `log.0` for writing + truncation
		if (f_open(&logtarget, "/log/log.0", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
			oldlogputchar = esp_log_set_putchar(log_putc);
			ESP_LOGI("sdlog", "started sd logging");
		}
		else {
			ESP_LOGW("sdlog", "unable to open log file");
		}
	}

	InitStatus init() {
		ESP_LOGI(TAG, "Starting SDFAT layer");

		// Start by turning on the SPI pins
		spi::init();

		// Send a few clock cycles to get the chip warmed up (spec says min 74, we send 80)
		portENTER_CRITICAL();
		for (int i = 0; i < 10; ++i) {
			spi::tx_byte(0xff);
		}
		portEXIT_CRITICAL();

		// Select the chip
		spi::select();
		
		ESP_LOGI(TAG, "Initializing card...");

		status_r1 x;

		// Send CMD0 up to 300 times
		{
			int tries = 30;
			while (true) {
				if (send_command<false>(0, 0, x) != command_status::Ok) {
					--tries;
					if (tries < 0) {
						ESP_LOGE(TAG, "Couldn't get a response from card");
						spi::deselect();
						return InitStatus::NoCard;
					}
				}
				else break;
			}
		}

		ESP_LOGI(TAG, "Got CMD0 response");

		// Finish multi-block write if necessary
		if (!(x.idle)) {
			ESP_LOGI(TAG, "Stopping stale write");
			uint8_t _;
			spi::tx_byte(0xFD); // STOP_TRAN_TOKEN
			// Receive a bunch of garbage
			for (int i = 0; i < 520; ++i) spi::rx_byte(_);
		}

		// Enable CRC checking
		if (send_command<false>(1, 59, x) != command_status::Ok || !(x.idle)) {
			ESP_LOGE(TAG, "Failed to set crc checking");
		}

		// Begin card type check flow sequence with CMD8
		{
			status_r7 response;
			status_r7::argument argument;

			argument.check_pattern = 0xAA;
			argument.voltage_accepted = 1;
			
			if (send_command<false>(argument, 8, response) == command_status::Ok) {
				// Is this a legal command?
				if (response.status.illegal_command) {
					// Version 1 or not SD
					ESP_LOGI(TAG, "Ver1/invalid card");

					card.type = Card::CardTypeSDVer1;
				}
				else {
					// Verify response data
					if (response.response.check_pattern != 0xAA || response.response.voltage_accepted != 1) {
						ESP_LOGE(TAG, "Invalid response to CMD8");
						return InitStatus::UnusableCard;
					}
					// Otherwise, ver2 or later
					ESP_LOGI(TAG, "Ver2/later card");

					card.type = Card::CardTypeSDVer2OrLater;
				}
			}
			else {
				ESP_LOGE(TAG, "Card didn't respond to CMD8");
				// Unusable card
				return InitStatus::UnusableCard;
			}
		}

		// Send ACMD41
		{
			status_r3 response;
			send_op_cond_argument arg;
			arg.raw_data = 0;
			arg.hcs = card.type == Card::CardTypeSDVer2OrLater ? 1 : 0; // If card supports it, ask for SDHC

			int tries = 4095;

			while (true) {
				spi::reselect();
				// Send ACMD41
				if (send_command<false>(0, 55, x) != command_status::Ok) {
					ESP_LOGE(TAG, "Card didn't respond to CMD55");
					return InitStatus::UnusableCard;
				}

				spi::reselect();
				if (send_command<false>(arg, 41, x) != command_status::Ok || x.idle) {
					--tries;
					if (tries < 0) {
						ESP_LOGE(TAG, "Timed out waiting for ACMD41");
						return InitStatus::UnusableCard;
					}
					continue;
				}

				break;
			}

			tries = 0;

			// Read OCR
			if (card.type == Card::CardTypeSDVer2OrLater) {
retry:
				spi::reselect();
				if (send_command<false>(0, 58, response) != command_status::Ok) {
					ESP_LOGE(TAG, "Failed to get OCR");
					return InitStatus::UnusableCard;
				}
				
				if (!response.response.busy) {
					// Card is busy
					++tries;
					if (tries > 3000) {
						ESP_LOGE(TAG, "Card not responding to CCS request");
						return InitStatus::UnusableCard;
					}

					goto retry;
				}

				// Otherwise, check the CCS
				if (response.response.card_capacity_status) {
					ESP_LOGI(TAG, "Card is SDHC");
					card.type = Card::CardTypeSDHC;
				}
			}
		}

		spi::reselect();

		// Read the CSD
		{
			csd_register csd;

			if (send_command<false>(0, 9 /* SEND_CSD */, x) != command_status::Ok || x.raw_data) {
				ESP_LOGE(TAG, "Card refused CSD");
				return InitStatus::UnusableCard;
			}

			read_status status = single_read(reinterpret_cast<uint8_t *>(&csd), 16, true);
			if (status != read_status::Ok) {
				ESP_LOGE(TAG, "Failed to do block read (%d)", (int)status);
			}

			if (csd.perm_write_protect) {
				ESP_LOGW(TAG, "Card is permanently write protected");
				card.perm_write_protect = true;
				card.write_protect = true;
			}
			if (csd.tmp_write_protect) {
				ESP_LOGW(TAG, "Card is temporarily write protected");
				card.write_protect = true;
			}

			// TODO: read TAAC/NSAC
		
			if (csd.csd_structure_ver == csd_register::CsdVersion1SC) {
				// Check card size from C_SIZE and C_SIZE_MULT and READ_BL_LEN
				card.length = (uint64_t(csd.v1.card_size + 1) * (1 << (csd.v1.card_size_mult + 2))) * (1 << csd.read_bl_len);
			}
			else {
				// Check card size using a much more sane method
				card.length = (uint64_t(csd.v2.card_size + 1) * 512 * 1024);
			}

			ESP_LOGI(TAG, "Card is %d MiB long", (int)(card.length / (1024 * 1024)));
		}

		// Deselect card
		spi::deselect();
		spi::tx_byte(0xff);

		// Mount volume
		switch (f_mount(&sd::fs, "", 1)) {
			case FR_OK:
				break;
			case FR_NO_FILESYSTEM:
				ESP_LOGE(TAG, "No FAT detected");
				return InitStatus::FormatError;
			default:
				ESP_LOGE(TAG, "Unknown FatFS error");
				return InitStatus::UnkErr;
		}

		ESP_LOGI(TAG, "Initialized FAT on sd card");

		// Initialize SD card logging
		return InitStatus::Ok;
	}
}

extern "C" DSTATUS disk_initialize(BYTE) {
	if (sd::card.type != sd::Card::CardTypeUndefined) {
		return 0;
	}
	else return STA_NOINIT;
}

extern "C" DSTATUS disk_status(BYTE) {
	if (sd::card.type != sd::Card::CardTypeUndefined) {
		if (sd::card.write_protect) return STA_PROTECT;
		else return 0;
	}
	else return STA_NOINIT;
}

extern "C" DRESULT disk_read(BYTE, BYTE* buff, LBA_t sector, UINT count) {
	if (sd::card.type == sd::Card::CardTypeUndefined) return RES_NOTRDY;
	int tries = 0;
	portENTER_CRITICAL();
	sd::spi::select();
	sd::spi::tx_byte(0xff);
	portEXIT_CRITICAL();
retry:
	auto x = sd::read(buff, sector, count);
	ESP_LOGV(TAG, "read (%d/3) %d %d --> %d", tries, sector, count, (int)x);
	if (x != sd::read_status::Ok) {
		++tries;
		if (tries < 3) goto retry;
	}
	portENTER_CRITICAL();
	sd::spi::deselect();
	sd::spi::tx_byte(0xff);
	portEXIT_CRITICAL();
	switch (x) {
		case sd::read_status::Ok:
			return RES_OK;
		case sd::read_status::OutOfBounds:
			return RES_PARERR;
		default:
			return RES_ERROR;
	}
}

extern "C" DRESULT disk_write(BYTE, const BYTE* buff, LBA_t sector, UINT count) {
	if (sd::card.type == sd::Card::CardTypeUndefined) return RES_NOTRDY;
	if (sd::card.write_protect) return RES_WRPRT;
	// retry behavior, up to 3 attempts
	int tries = 0;
	portENTER_CRITICAL();
	sd::spi::select();
	sd::spi::tx_byte(0xff);
	portEXIT_CRITICAL();
retry:
	auto x = sd::write(buff, sector, count);
	ESP_LOGV(TAG, "write (%d/3) %d %d --> %d", tries, sector, count, (int)x);
	if (x != sd::write_status::Ok) {
		++tries;
		if (tries < 3) goto retry;
	}
	portENTER_CRITICAL();
	sd::spi::deselect();
	sd::spi::tx_byte(0xff);
	portEXIT_CRITICAL();
	switch (x) {
		case sd::write_status::Ok:
			return RES_OK;
		case sd::write_status::OutOfBounds:
			return RES_PARERR;
		default:
			return RES_ERROR;
	}
}

extern "C" DRESULT disk_ioctl(BYTE, BYTE cmd, void *buff) {
	if (sd::card.type == sd::Card::CardTypeUndefined) return RES_NOTRDY;
	switch (cmd) {
		case CTRL_SYNC:
			return RES_OK;
		case GET_SECTOR_COUNT:
			*((DWORD *) buff) = sd::card.length / 512;
			return RES_OK;
		case GET_SECTOR_SIZE:
			*((DWORD *) buff) = 512;
			return RES_OK;
		case GET_BLOCK_SIZE:
			return RES_ERROR;
		default:
			return RES_PARERR;
	}
}
