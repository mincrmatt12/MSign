#include "octoprintbitmap.h"
#include <math.h>
#include <ff.h>
#include <memory>
#include <stdint.h>
#include <string.h>
#include <esp_system.h>
#include "octoprint.cfg.h"
#include "../common/slots.h"
#include "../serial.h"
#include <esp_task_wdt.h>

static const char * TAG = "octoprintbitmap";

#include <esp_log.h>
#include <gcode_scan.h>

namespace octoprint {
	// Basic fixed-point wrapper, only supporting a few operations.
	struct fixed_t {
		const static inline int Fac = 65536;

		fixed_t() : value{} {}
		fixed_t(int raw, std::nullptr_t) : value(raw) {}
		fixed_t(int nonraw) : value(nonraw * Fac) {}
		fixed_t(float flt) : value(flt * Fac) {}
		fixed_t(intmax_t real, int8_t decimal)
		{
			switch (decimal) {
				// if (Fac/10**decimal) > 1, multiply directly.
				// the extra additions are for the fractional part (.6, .36, .536, .5536)
				case 0:
					value = int(real) * Fac;
					return;
				case 1:
					value = int(real) * (Fac / 10);
					return;
				case 2:
					value = int(real) * (Fac / 100) + int(real >> 2);
					return;
				case 3:
					value = int(real) * (Fac / 1000) + int(real >> 1) + int(real >> 5);
					return;
				case 4:
					value = int(real) * (Fac / 10000) + int(real >> 1) + int(real >> 5) + int(real >> 6);
					return;
				// if between 5-12, use a table of powers of 10 and work in intmax_t precision
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
				case 10:
				case 11:
					{
						const static intmax_t factors[6] = {
							100000LL,
							1000000LL,
							10000000LL,
							100000000LL,
							1000000000LL,
							10000000000LL
						};
						real *= Fac;
						real /= factors[decimal - 5];
						value = int(real);
					}
					return;
				default:
					{
						real *= Fac;
						real /= 100000000000LL;
						// if greater than 12, do repeated division for the remaining places
						decimal -= 12;
						while (decimal--) real /= 10;
						value = int(real);
					}
					return;
			}
		}

#define rel_op(x) inline bool operator x (fixed_t other) const { return value x other.value; } \
			      inline bool operator x (int other)            const { return value x other * Fac; } \
			      inline bool operator x (float other)          const { return value x other; } \

		rel_op(<);
		rel_op(<=);
		rel_op(==);
		rel_op(!=);
		rel_op(>);
		rel_op(>=);

#undef rel_op

#define arith_op(x) inline fixed_t  operator x (fixed_t other) const { return {value x other.value, nullptr}; } \
					inline fixed_t &operator x ## = (fixed_t other)  { value x ## = other.value; return *this; }

		arith_op(+);
		arith_op(-);

#undef arith_op

#define int_op(x) inline fixed_t  operator x      (int other) const { return {value x other, nullptr}; } \
				  inline fixed_t &operator x ## = (int other)  { value x ## = other; return *this; } \
                  inline fixed_t  operator x      (float other) const { return {int(value x other), nullptr}; } \
				  inline fixed_t &operator x ## = (float other)  { value x ## = other; return *this; }

		int_op(*);
		int_op(/);

#undef int_op

		inline fixed_t operator-() const { return {-value, nullptr}; }
		inline fixed_t abs() const { return {std::abs(value), nullptr}; }

		fixed_t operator*(fixed_t other) const {
			intmax_t temp = value;
			temp *= other.value;
			return {static_cast<int>(temp / Fac), nullptr};
		}
		fixed_t operator/(fixed_t other) const {
			intmax_t temp = value;
			temp *= Fac;
			temp /= other.value;
			return {static_cast<int>(temp), nullptr};
		}

		explicit inline operator float() const { return (float)value / Fac; }
		explicit operator int() const { 
			return (value + (Fac / 2)) / Fac;
		}

	private:
		int value;
	};

	struct GcodeMachineState {
		constexpr inline static size_t BITMAP_SIZE = 4096;
		constexpr inline static size_t BITMAP_BITS = BITMAP_SIZE * 8;
		constexpr inline static int MIN_AXIS = 48;
		constexpr inline static int MAX_AXIS = 350;
		constexpr inline static int MAX_OUTER_MARGIN = 8;

		struct Pos {
			fixed_t x{}, y{}, z{}, e{};
		} pos;

		bool relative_e{}, relative_g{};
		uint16_t layer{};

		fixed_t layerheight{};
		bool had_sd_error{};

		FIL modelinfo{}, auxinfo{}; // auxinfo is bitmaps while drawing and index map while indexing
		std::unique_ptr<uint8_t[]> bitmap;

		GcodeMachineState() {}
		~GcodeMachineState() {
			f_close(&modelinfo);
			f_close(&auxinfo);
		}

		void plot(int x, int y) {
			int idx = draw_info.bitindex(x, y);
			if (idx < 0) return;
			bitmap[idx / 8] |= 1 << (idx % 8);
		}

		void clear_bitmap() {
			memset(bitmap.get(), 0, BITMAP_SIZE);
		}

		bool reset(bool mode) {
			layerheight = 0;
			layer = 0;

			drawing = mode;
			relative_e = relative_g = false;

			if (mode) {
				auto *mem = new (std::nothrow) uint8_t[BITMAP_SIZE];
				if (mem == nullptr) {
					ESP_LOGE(TAG, "failed to allocate bitmap");
					return false;
				}
				bitmap.reset(mem);
				draw_info = DrawInfo{};

				f_close(&auxinfo);
				if (auto code = f_open(&auxinfo, "/cache/printer/bitmaps.bin", FA_CREATE_ALWAYS | FA_WRITE); code != FR_OK) {
					ESP_LOGE(TAG, "failed to open bitmaps.bin for writing, code %d", code);
					return false;
				}
				f_rewind(&modelinfo);
			}
			else {
				if (auto code = f_open(&modelinfo, "/cache/printer/layers.bin", FA_WRITE | FA_CREATE_ALWAYS | FA_READ); code != FR_OK) {
					ESP_LOGE(TAG, "failed to open modelinfo for writing, code %d", code);
					return false;
				}
				if (auto code = f_open(&auxinfo,   "/cache/printer/boffs.bin",  FA_WRITE | FA_CREATE_ALWAYS); code != FR_OK) {
					ESP_LOGE(TAG, "failed to open boffs.bin for writing, code %d", code);
					return false;
				}
				UINT bw;
				uint32_t filpos{};
				f_write(&auxinfo, &filpos, 4, &bw);
				layer_info = LayerInfo{};
			}

			return true;
		}

		template<bool Hi>
		void line_impl(int x0, int y0, int x1, int y1) {
			int dx = x1 - x0;
			int dy = y1 - y0;
			int inc = 1;

			if ((Hi ? dx : dy) < 0) {
				inc = -1;
				(Hi ? dx : dy) = -(Hi ? dx : dy);
			}

			int D = 2*(Hi ? dx : dy) - (Hi ? dy : dx);
			int a = (Hi ? x0 : y0);

			for (int b = (Hi ? y0 : x0); b <= (Hi ? y1 : x1); ++b) {
				if constexpr (Hi)
					plot(a, b);
				else
					plot(b, a);

				if (D > 0) {
					a += inc;
					D -= 2*(Hi ? dy : dx);
				}
				D += 2*(Hi ? dx : dy);
			}
		}

		void line(int x0, int y0, int x1, int y1) {
			// If line is out of range, ignore it
			if (x0 < -MAX_OUTER_MARGIN || y0 < -MAX_OUTER_MARGIN || x0 >= draw_info.xdim + MAX_OUTER_MARGIN || y0 >= draw_info.ydim + MAX_OUTER_MARGIN) {
				ESP_LOGW(TAG, "line is out of range: %d %d -> %d %d", x0, y0, x1, y1);
				return;
			}
			if (x1 < -MAX_OUTER_MARGIN || y1 < -MAX_OUTER_MARGIN || x1 >= draw_info.xdim + MAX_OUTER_MARGIN || y1 >= draw_info.ydim + MAX_OUTER_MARGIN) {
				ESP_LOGW(TAG, "line is out of range: %d %d -> %d %d", x0, y0, x1, y1);
				return;
			}

			if (abs(y1 - y0) < abs(x1 - x0)) {
				if (x0 > x1)
					line_impl<false>(x1, y1, x0, y0);
				else
					line_impl<false>(x0, y0, x1, y1);
			}
			else
				if (y0 > y1)
					line_impl<true>(x1, y1, x0, y0);
				else
					line_impl<true>(x0, y0, x1, y1);
		}

		struct LayerInfo {
			fixed_t minx{}, miny{}, maxx{}, maxy{};

			LayerInfo() = default;
			LayerInfo(const Pos& pos) {
				minx = maxx = pos.x;
				miny = maxy = pos.y;
			}

			void append_point(const Pos& pos) {
				if (pos.x < minx)
					minx = pos.x;
				if (pos.x > maxx)
					maxx = pos.x;
				if (pos.y < miny)
					miny = pos.y;
				if (pos.y > maxy)
					maxy = pos.y;
			}
		};
		struct DrawInfo {
			fixed_t minx{-1}, miny{-1}, sizex{0}, sizey{0};
			int16_t xdim{}, ydim{};

			DrawInfo() = default;
			DrawInfo(const LayerInfo& recorded) {
				sizex = (recorded.maxx - recorded.minx);
				sizey = (recorded.maxy - recorded.miny);

				float A = float(sizex) / float(sizey);
				int y_size = sqrtf(BITMAP_BITS / A);
				int x_size = A * y_size;

				if (x_size < MIN_AXIS) {
					x_size = MIN_AXIS;
					if (y_size < MIN_AXIS) {
						y_size = MIN_AXIS;
					}
					else {
						y_size = std::min<int>(MAX_AXIS, MIN_AXIS / A);
					}
				}
				else if (y_size < MIN_AXIS) {
					y_size = MIN_AXIS;
					x_size = std::min<int>(MAX_AXIS, MIN_AXIS * A);
				}

				float A_real = (float)x_size / (float)y_size;
				fixed_t ry = float(sizex) / A_real;
				fixed_t rx = float(sizey) * A_real;

				if (A_real != A) {
					if (rx < sizex) {
						sizey = ry;
					}
					else {
						sizex = rx;
					}
				}
				else {
					sizex = rx;
					sizey = ry;
				}

				minx = (recorded.minx + recorded.maxx - sizex) / 2;
				miny = (recorded.miny + recorded.maxy - sizey) / 2;
				xdim = x_size;
				ydim = y_size;
			}

			int scale_x(fixed_t x) const {
				return int(((x - minx) / sizex) * (xdim - 1));
			}
			int scale_y(fixed_t y) const {
				return int(((y - miny) / sizey) * (ydim - 1));
			}
			int bitindex(int x, int y) const {
				if (x < 0 || x >= xdim) return -1;
				if (y < 0 || y >= ydim) return -1;
				return x + y * xdim;
			}
		};

		union {
			LayerInfo layer_info{};
			DrawInfo draw_info;
		};

		bool drawing = false;
	};

	void common_finished_layer(GcodeMachineState *gstate, gcode_scan_state_t *sstate, bool is_last=false) {
		UINT bw = 0, total_bw = 0;
		if (gstate->drawing) {
			int tries = 0;
			while (total_bw < GcodeMachineState::BITMAP_SIZE && tries < 5) {
				auto code = f_write(&gstate->auxinfo, gstate->bitmap.get() + total_bw, GcodeMachineState::BITMAP_SIZE - total_bw, &bw);
				total_bw += bw;
				if (code != FR_OK) {
					vTaskDelay(pdMS_TO_TICKS(15));
					++tries;
				}
			}
			if (tries >= 5) {
				ESP_LOGE(TAG, "failed to write bitmaps.bin");
				gstate->had_sd_error = true;
			}
		}
		else {
			GcodeMachineState::DrawInfo new_dinfo(gstate->layer_info);

			if (f_write(&gstate->modelinfo, &new_dinfo, sizeof(new_dinfo), &bw) != FR_OK) {
				ESP_LOGE(TAG, "failed to write modelinfo");
				gstate->had_sd_error = true;
			}
			else if (f_write(&gstate->auxinfo, &gstate->layerheight, 4, &bw) != FR_OK) {
				ESP_LOGE(TAG, "failed to write layerheight");
				gstate->had_sd_error = true;
			}
			else if (!is_last && f_write(&gstate->auxinfo, &sstate->c.file_pos, 4, &bw) != FR_OK)
			{
				ESP_LOGE(TAG, "failed to write layeroffset");
				gstate->had_sd_error = true;
			}
		}
	}

	bool process_gcode(GcodeProvider& provider, uint16_t &layer_count_out) {
		uint64_t gcode_size{};
		uint8_t buf[FF_MIN_SS];
		
		GcodeParseProgressTracker pt;
		pt.set_download_phase_flag(provider.has_download_phase());

		if (!provider.restart()) {
			pt.fail();
			return false;
		}

		gcode_size = provider.gcode_size();
		bool use_25_progress_base = provider.has_download_phase();

		GcodeMachineState machine;
		gcode_scan_state_t scanner;
		scanner.userptr = &machine;

		ESP_LOGD(TAG, "processing new gcode");
		if (!machine.reset(false)) {
			pt.fail();
			return false;
		}

		auto update_progress = [&]{
			int8_t current_progress = ((uint64_t)scanner.c.file_pos * (!use_25_progress_base || machine.drawing ? 50 : 25)) / gcode_size;
			if (machine.drawing)
				current_progress += 50;
			else if (use_25_progress_base)
				current_progress += 25;
			pt.update(current_progress);
		};

		auto phase = [&]{
			gcode_scan_start(&scanner);
			UINT bytes_since_last_yield = 0;

			// do the first pass of processing
			while (!provider.is_done()) {
				int br = provider.read(buf, sizeof buf);

				if (br <= 0) {
					pt.fail();
					return false;
				}

				bytes_since_last_yield += br;
				if (bytes_since_last_yield >= 32768) {
					bytes_since_last_yield = 0;
					vTaskDelay(pdMS_TO_TICKS(10));
				}

				switch (gcode_scan_feed(buf, buf + br, &scanner)) {
					case GCODE_SCAN_OK:
					case GCODE_SCAN_DONE:
						update_progress();
						break;
					case GCODE_SCAN_FAIL:
						ESP_LOGE(TAG, "Failed to parse ./job.gcode");
						pt.fail();
						return false;
				}
				if (machine.had_sd_error) {
					ESP_LOGE(TAG, "machine parser indicated error, exiting");
					pt.fail();
					return false;
				}
			}
			common_finished_layer(&machine, &scanner, true);
			return true;
		};

		// 0 - 25% is used by the downloader to report the download percent;
		// 25% - 50% is used by the first phase, 50 - 100% is used by the rest
		pt.update(use_25_progress_base ? 25 : 0);
		if (!phase())
		{
			pt.fail();
			return false;
		}
		ESP_LOGD(TAG, "parsed layers ok");
		if (!machine.reset(true)) {
			pt.fail();
			return false;
		}

		vTaskDelay(pdMS_TO_TICKS(25));
		provider.restart();

		if (!phase())
		{
			pt.fail();
			return false;
		}
		ESP_LOGD(TAG, "drew bitmaps ok");

		layer_count_out = machine.layer;
		return true;
	}

	void send_bitmap_for(uint32_t filepos) {
		int layeridx = -1;
		fixed_t layerheight;
		UINT br;

		{
			FIL boffs{};
			if (f_open(&boffs, "/cache/printer/boffs.bin", FA_READ) != FR_OK) {
				ESP_LOGE(TAG, "failed to open boffs.bin");
				return;
			}

			struct BoffEntry {
				uint32_t fpos;
				fixed_t layerheight;
			};

			if (filepos == RANDOM_FILEPOS) {
				layeridx = esp_random() % (f_size(&boffs) / sizeof(BoffEntry));
				f_lseek(&boffs, layeridx * sizeof(BoffEntry));
				BoffEntry be;
				f_read(&boffs, &be, sizeof(BoffEntry), &br);
				layerheight = be.layerheight;
			}
			else while (!f_eof(&boffs)) {
				BoffEntry be;

				f_read(&boffs, &be, sizeof(BoffEntry), &br);

				if (be.fpos > filepos) break;
				else {
					++layeridx;
					layerheight = be.layerheight;
				}
			}

			f_close(&boffs);
		}
		
		if (layeridx < 0)
		{
			ESP_LOGE(TAG, "invalid boff offset");
			return;
		}

		size_t max_used_bytes = 0;

		{
			GcodeMachineState::DrawInfo di;
			FIL layerinfos;
			
			if (f_open(&layerinfos, "/cache/printer/layers.bin", FA_READ) != FR_OK)
			{
				ESP_LOGE(TAG, "failed to open layers.bin");
				return;
			}

			if (f_lseek(&layerinfos, sizeof(di) * layeridx) != FR_OK)
			{
				ESP_LOGE(TAG, "out of range");
				f_close(&layerinfos);
				return;
			}
			f_read(&layerinfos, &di, sizeof(di), &br);

			slots::PrinterBitmapInfo inf;
			inf.file_process_percent = inf.PROCESSED_OK;
			inf.bitmap_width = di.xdim;
			inf.bitmap_height = di.ydim;
			inf.current_layer_height = int(layerheight * 100);
			inf.current_layer_number = layeridx + 1;

			serial::interface.update_slot(slots::PRINTER_BITMAP_INFO, inf);

			ESP_LOGD(TAG, "sending layer %d", layeridx + 1);

			max_used_bytes = di.xdim * di.ydim / 8;
			max_used_bytes += 1;
			if (max_used_bytes >= 4096) max_used_bytes = 4096;

			f_close(&layerinfos);
		}

		{
			uint8_t buf[256];
			UINT offset = 0;

			serial::interface.allocate_slot_size(slots::PRINTER_BITMAP, max_used_bytes);

			FIL bitmaps;
			if (f_open(&bitmaps, "/cache/printer/bitmaps.bin", FA_READ) != FR_OK)
			{
				ESP_LOGE(TAG, "failed to open bitmaps.bin");
				serial::interface.delete_slot(slots::PRINTER_BITMAP);
				return;
			}

			f_lseek(&bitmaps, GcodeMachineState::BITMAP_SIZE * layeridx);

			while (offset < max_used_bytes) {
				UINT toread = sizeof(buf);
				UINT br;
				if (offset + toread > max_used_bytes)
					toread = max_used_bytes - offset;
				f_read(&bitmaps, buf, toread, &br);
				if (br == 0) {
					ESP_LOGE(TAG, "failed to read bitmaps.bin");
					f_close(&bitmaps);
					serial::interface.delete_slot(slots::PRINTER_BITMAP);
					return;
				}
				serial::interface.update_slot_range(slots::PRINTER_BITMAP, buf, offset, br, true, false);
				offset += br;
			}
			serial::interface.trigger_slot_update(slots::PRINTER_BITMAP);

			f_close(&bitmaps);
		}
	}
}

extern "C" void gcode_scan_got_command_hook(gcode_scan_state_t *state, uint8_t inval) {
	using namespace octoprint;
	auto *gstate = (GcodeMachineState *)state->userptr;

	switch (state->c.command_type) {
		case GCODE_SCAN_COMMAND_TYPE_G_MOVE:
			{
				GcodeMachineState::Pos last = gstate->pos;
				if (gstate->relative_g)
				{
					if (state->c.x_decimal >= 0) gstate->pos.x += fixed_t(state->c.x_int, state->c.x_decimal);
					if (state->c.y_decimal >= 0) gstate->pos.y -= fixed_t(state->c.y_int, state->c.y_decimal);
					if (state->c.z_decimal >= 0) gstate->pos.z += fixed_t(state->c.z_int, state->c.z_decimal);
				}
				else
				{
					if (state->c.x_decimal >= 0) gstate->pos.x = fixed_t(state->c.x_int, state->c.x_decimal);
					if (state->c.y_decimal >= 0) gstate->pos.y = -fixed_t(state->c.y_int, state->c.y_decimal);
					if (state->c.z_decimal >= 0) gstate->pos.z = fixed_t(state->c.z_int, state->c.z_decimal);
				}
				if (gstate->relative_e)
				{
					if (state->c.e_decimal >= 0) gstate->pos.e += fixed_t(state->c.e_int, state->c.e_decimal);
				}
				else
				{
					if (state->c.e_decimal >= 0) gstate->pos.e = fixed_t(state->c.e_int, state->c.e_decimal);
				}

				bool did_extrude = (last.x != gstate->pos.x || last.y != gstate->pos.y || last.z != gstate->pos.z) && last.e < gstate->pos.e;
				if (!did_extrude)
					return;

				if (gstate->pos.z != gstate->layerheight && (gstate->pos.z > gstate->layerheight || (gstate->layerheight - gstate->pos.z).abs() > max_layer_height))
				{
					if (gstate->layer)
						common_finished_layer(gstate, state);
					if (gstate->drawing)
					{
						gstate->clear_bitmap();
						UINT br;
						int tries = 0;
						while (++tries <= 3) {
							if (f_read(&gstate->modelinfo, &gstate->draw_info, sizeof(GcodeMachineState::DrawInfo), &br) != FR_OK) {
								vTaskDelay(pdMS_TO_TICKS(20));
								if (tries > 1)
									ESP_LOGD(TAG, "failed to read modelinfo, trying again");
							}
							else break;
						}
						if (tries > 3) {
							gstate->had_sd_error = true;
							ESP_LOGE(TAG, "failed to read modelinfo");
						}
					}
					else {
						gstate->layer_info = GcodeMachineState::LayerInfo{gstate->pos};
					}
					gstate->layer += 1;
					gstate->layerheight = gstate->pos.z;
				}

				if (gstate->drawing) {
					gstate->line(
						gstate->draw_info.scale_x(last.x),
						gstate->draw_info.scale_y(last.y),
						gstate->draw_info.scale_x(gstate->pos.x),
						gstate->draw_info.scale_y(gstate->pos.y)
					);
				}
				else {
					gstate->layer_info.append_point(last);
					gstate->layer_info.append_point(gstate->pos);
				}
			}
			break;
		case GCODE_SCAN_COMMAND_TYPE_E_RELATIVE_ON:
		case GCODE_SCAN_COMMAND_TYPE_E_RELATIVE_OFF:
			gstate->relative_e = state->c.command_type == GCODE_SCAN_COMMAND_TYPE_E_RELATIVE_ON;
			break;
		case GCODE_SCAN_COMMAND_TYPE_G_RELATIVE_ON:
		case GCODE_SCAN_COMMAND_TYPE_G_RELATIVE_OFF:
			gstate->relative_g = state->c.command_type == GCODE_SCAN_COMMAND_TYPE_G_RELATIVE_ON;
			if (g_relative_is_e)
				gstate->relative_e = gstate->relative_g;
			break;
		case GCODE_SCAN_COMMAND_TYPE_G_RESET_E:
			gstate->pos.e = 0;
			break;
	}
}
