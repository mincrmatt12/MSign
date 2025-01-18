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

		void arc(int x0, int y0, int x1, int y1, int xc, int yc) {
			constexpr static class OctantMap {
				// Octants go clockwise from north of the center. The fields
				// represent the transform from coordinates in octant 0 _to_ the given octant.
				// Transforming from octant 1 to octant 0 is performed by flipping x/y, so transitively
				// going to octant 1 is performed by flipping swap_xy.
				struct {
					bool swap_xy, neg_y, neg_x;
				} xforms[8] = {
					/* [0] = */ {false, false,  false},
					/* [1] = */ {true,  false,  false},
					/* [2] = */ {true,  true,   false},
					/* [3] = */ {false, true,   false},
					/* [4] = */ {false, true,   true},
					/* [5] = */ {true,  true,   true},
					/* [6] = */ {true,  false,  true},
					/* [7] = */ {false, false,  true},
				};

				int octants[8];

				// Octant 0 has coordinates with negative y, positive x, and where abs(dx) <= abs(dy)
				// (i.e. where the effective slope is less than one). Thus, referencing the table
				// we have that neg_x==true corresponds to dx < 0, neg_y = true corresponds to dy > 0
				// and swap_xy==true corresponds to abs(dx) > abs(dy)
			public:
				constexpr OctantMap() {
					for (int i = 0; i < 8; ++i) {
						const auto& xform = xforms[i];
						bool dx_gt_dy = xform.swap_xy;
						bool dx_neg   = xform.neg_x;
						bool dy_pos   = xform.neg_y;
						octants[
							dx_gt_dy * 4 +
							dx_neg   * 2 +
							dy_pos   * 1
						] = i;
					}
				};

				inline std::pair<int, int> to_octant(int dx, int dy, int octant, bool src_is_octant_1) const {
					auto &xform = xforms[octant];
					int px = (src_is_octant_1 ^ xform.swap_xy) ? -dy : dx;
					int py = (src_is_octant_1 ^ xform.swap_xy) ? -dx : dy;
					if (xform.neg_x) px = -px;
					if (xform.neg_y) py = -py;
					return {px, py};
				}

				inline int as_octant(int dx, int dy) const {
					return octants[
						(abs(dx) > abs(dy)) * 4 +
						(dx < 0) * 2 +
						(dy > 0) * 1
					];
				}
			} OCTANTS{};

			// The idea behind this is similar to a typical Bresenham/midpoint circle implementation, where
			// we iteratively step along the edge of the circle and pick pixels which minimize the total radius
			// error. 
			//
			// We treat the (xy0 -- xyc) line as a radius, while the (xy1 -- xyc) line is just used to define the end angle
			// (so if they have different radii the algorithm will still terminate but not actually reach xy1 (just a point
			// collinear with it).
			//
			// We avoid directly computing the radius (as that introduces a sqrt), instead always working with an implicit
			// radius squared term. This means we cannot compute the normal starting point (0, -r). Instead we assume xy0
			// is on the circle (even if there isn't an actual integer radius that would place a circle at that exact pixel.)
			//
			// Normally, the algorithm can assume its current dx/dy are in some known octant (usually 0) and mirror the remaining
			// 7 octants to fill a circle. We instead notice that of the 8 octants, each half is rotationally symmetric (so octants
			// 0, 2, 4, 6 are all identical modulo some rotation, and the same for 1, 3, 5, 7), and so we can mirror the input point
			// into one of octants 0 or 1 without changing the relative angle from the start of the octant (going clockwise) to that
			// new point. This means that when we re-mirror back into the original octant, points clockwise of the internal dx,dy point
			// are still clockwise of xy0 and so are on the arc.
			//
			// This lets us draw clockwise from xy0 until the edge of its containing octant. We can similarly run the algorithm in reverse
			// to draw the rest of xy0's octant - filling an entire octant and letting us fill in the entire circle. Since we want an arc,
			// we limit the filled octants to the ones between xy0 and xy1 - the ones not containing xy0 and xy1 can be completely filled,
			// the one containing xy0 is filled unconditionally in the forward clockwise pass and the one containing xy1 is filled until
			// reaching xy1 -- the handedness check is done with a cross product -- x cross y is positive where y is counterclockwise of x.
			
			// Determine if we need to use the octant 0 (lo) or 1 (hi) implementation.

			int dx0 = x0 - xc;
			int dy0 = y0 - yc;

			int dx1 = x1 - xc;
			int dy1 = y1 - yc;

			// Escape hatch: if xc/yc are _really_ far away from the endpoints, the thing is basically just
			// a line (and integer overflow will break in that case)
			if (std::max(abs(dx0), abs(dy0)) > 1e5) {
				return line(x0, y0, x1, y1);
			}

			int o0 = OCTANTS.as_octant(dx0, dy0);
			int o1 = OCTANTS.as_octant(dx1, dy1);

			// Calculate number of octants to draw.
			int oc = o1 - o0 + 1;
			if (oc <= 0) oc += 8;
			else if (oc == 1) {
				// If both points are in the same octant, we either:
				// 	- fill a single arc section, if xy0 is before xy1
				// 	- fill the entire circle, otherwise
				if (dx0 * dy1 - dy0 * dx1 <= 0) // allow = 0 for full circle
					// Set octant count to 9 to indicate full circle, as we need
					// to process the backwards pass into the first octant in this
					// special case.
					oc = 9;
			}

			// Move dy/dx into quadrant 0 (leave the octant to the IMPL block)
			dy0 = -abs(dy0);
			dx0 =  abs(dx0);

			auto IMPL = [=, this] <bool Hi> () mutable {
				if ((Hi && dx0 < -dy0) || (!Hi && dx0 > -dy0)) {
					std::swap(dx0, dy0);
					dx0 = -dx0;
					dy0 = -dy0;
				}

				// Clockwise block
				{
					int dx = dx0;
					int dy = dy0;
					int oc0 = oc == 9 ? 8 : oc;
					int err = 1 + (Hi ? (dx + 2*dy) : (2*dx + dy));

					goto pixel; // Draw first pixel at (x0,y0) unconditionally

					while ((Hi && 0 > dy) || // in octant 1, CW until at x-axis
						   (!Hi && dx < -dy) // in octant 0, CW until at x=y line
					) {
						if constexpr (Hi) {
							if (err > 0) {
								err += 2*dy + 3;
							}
							else {
								err += 2*(dx + dy) + 5;
								++dx;
							}
							++dy;
						}
						else {
							if (err < 0) {
								err += 2*dx + 3;
							}
							else {
								err += 2*(dx + dy) + 5;
								++dy;
							}
							++dx;
						}
pixel:
						for (int i = 0, oct = o0; i < oc0;
							   ++i,     oct = (oct + 1) % 8)
						{
							auto [px, py] = OCTANTS.to_octant(dx, dy, oct, Hi);
							// notice that for the full circle case oc0 == 8 so i < oc - 1 == i < 8 is always true -
							// this ensures that we fill even CW of the endpoint because of the "inverted" nature
							// of the arc. when oc == 1, we _do_ want to do the CCW check because the first octant
							// genuinely is the last octant.
							if (i < oc - 1 ||  // ... if not the last octant, always fill
								px * dy1 - py * dx1 > 0) // ... or if we are ccw of the end point
								plot(xc + px, yc + py);
						}
					}
				}

				--oc;
				if (oc <= 0) // if only filling a single octant, we don't need the backwards pass
					return;

				// Counterclockwise block 
				{
					int dx = dx0;
					int dy = dy0;
					int err = -(Hi ? dx + 2*dy : 2*dx + dy) + 1;

					// do not plot into the first octant
					++o0;
					o0 %= 8;

					while ((Hi && dx > -dy) || // in octant 1, CCW until at x=y line
						   (!Hi && 0 < dx)     // in octant 0, CCW until at y-axis
					) {
						if constexpr (Hi) {
							if (err < 0) {
								err -= 2*dy - 3;
							}
							else {
								err -= 2*(dx + dy) - 5;
								--dx;
							}
							--dy;
						}
						else {
							if (err > 0) {
								err -= 2*dx - 3;
							}
							else {
								err -= 2*(dx + dy) - 5;
								--dy;
							}
							--dx;
						}

						for (int i = 0, oct = o0; i < oc;
							   ++i,     oct = (oct + 1) % 8) {
							auto [px, py] = OCTANTS.to_octant(dx, dy, oct, Hi);
							if (oct != o1 ||             // ... if not the last octant, always fill
								px * dy1 - py * dx1 > 0) // ... or if we are ccw of the end point
								plot(xc + px, yc + py);
							// Unlike the forward block, we check for oct == o1 explicitly here:
							// for one, it's faster than doing the arithmetic on oc, but also
							// because unlike the forward pass we want to do the CCW check
							// for o1 even if o1 == o0 (unlike in the forward pass where we
							// explicitly _don't_ want that iff oc == 9)
						}
					}
				}
			};

			if (o0 % 2 == 0)
				IMPL.operator()<false>();
			else
				IMPL.operator()<true>();
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
		case GCODE_SCAN_COMMAND_TYPE_G_ARC_CW:
		case GCODE_SCAN_COMMAND_TYPE_G_ARC_CCW:
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

				bool did_extrude = (last.x != gstate->pos.x || last.y != gstate->pos.y || last.z != gstate->pos.z) && last.e < gstate->pos.e && state->c.wipe_nesting == 0;
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
					switch (state->c.command_type) {
						case GCODE_SCAN_COMMAND_TYPE_G_MOVE:
							gstate->line(
								gstate->draw_info.scale_x(last.x),
								gstate->draw_info.scale_y(last.y),
								gstate->draw_info.scale_x(gstate->pos.x),
								gstate->draw_info.scale_y(gstate->pos.y)
							);
							break;
						case GCODE_SCAN_COMMAND_TYPE_G_ARC_CW:
							gstate->arc(
								gstate->draw_info.scale_x(last.x),
								gstate->draw_info.scale_y(last.y),
								gstate->draw_info.scale_x(gstate->pos.x),
								gstate->draw_info.scale_y(gstate->pos.y),
								gstate->draw_info.scale_x(last.x + fixed_t(state->c.i_int, state->c.i_decimal)),
								gstate->draw_info.scale_y(last.y - fixed_t(state->c.j_int, state->c.j_decimal))
							);
							break;
						// CCW is equivalent to swapping endpoints since we're just plotting
						case GCODE_SCAN_COMMAND_TYPE_G_ARC_CCW:
							gstate->arc(
								gstate->draw_info.scale_x(gstate->pos.x),
								gstate->draw_info.scale_y(gstate->pos.y),
								gstate->draw_info.scale_x(last.x),
								gstate->draw_info.scale_y(last.y),
								gstate->draw_info.scale_x(last.x + fixed_t(state->c.i_int, state->c.i_decimal)),
								gstate->draw_info.scale_y(last.y - fixed_t(state->c.j_int, state->c.j_decimal))
							);
							break;
						default: break;
					}
				}
				else {
					gstate->layer_info.append_point(last);
					gstate->layer_info.append_point(gstate->pos);
					// todo: arc bbox
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
