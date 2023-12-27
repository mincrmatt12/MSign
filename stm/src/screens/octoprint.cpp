#include "octoprint.h"
#include "../srv.h"
#include "../draw.h"
#include "../tasks/screen.h"
#include "../tasks/timekeeper.h"

#include "../fonts/lcdpixel_6.h"
#include "../fonts/tahoma_9.h"
#include "../fonts/latob_15.h"
#include "../common/slots.h"
#include "threed/fixed.h"

extern srv::Servicer servicer;
extern matrix_type matrix;
extern uint64_t rtc_time;
extern tasks::Timekeeper timekeeper;
extern tasks::DispMan dispman;

screen::OctoprintScreen::OctoprintScreen() {
	servicer.set_temperature_all<
		slots::PRINTER_INFO,
		slots::PRINTER_STATUS,
		slots::PRINTER_FILENAME,
		slots::PRINTER_BITMAP,
		slots::PRINTER_BITMAP_INFO>(bheap::Block::TemperatureHot);
}

screen::OctoprintScreen::~OctoprintScreen() {
	servicer.set_temperature_all<
		slots::PRINTER_INFO,
		slots::PRINTER_STATUS,
		slots::PRINTER_FILENAME,
		slots::PRINTER_BITMAP,
		slots::PRINTER_BITMAP_INFO>(bheap::Block::TemperatureCold);
}

void screen::OctoprintScreen::prepare(bool) {
	servicer.set_temperature_all<
		slots::PRINTER_INFO,
		slots::PRINTER_STATUS,
		slots::PRINTER_FILENAME,
		slots::PRINTER_BITMAP,
		slots::PRINTER_BITMAP_INFO>(bheap::Block::TemperatureWarm);
}

void screen::OctoprintScreen::draw() {
	srv::ServicerLockGuard g(servicer);

	const auto& pinfo = servicer.slot<slots::PrinterInfo>(slots::PRINTER_INFO);

	if (!pinfo) {
		return draw_disabled_background("loading");
	}

	switch (pinfo->status_code) {
		case slots::PrinterInfo::DISCONNECTED:
			return draw_disabled_background("printer off");
		case slots::PrinterInfo::READY:
		case slots::PrinterInfo::PRINTING:
			break;
		case slots::PrinterInfo::UNKNOWN:
			return;
	}

	draw_background();

	const auto& status = servicer[slots::PRINTER_STATUS];
	const auto& filename = servicer[slots::PRINTER_FILENAME];

	if (!status)
		return;

	// Draw printer state. TODO: possibly a fun icon?
	int16_t y = 8;

	draw::outline_multi_text(matrix.get_inactive_buffer(), font::tahoma_9::info, 1, y, *status, pinfo->status_code == slots::PrinterInfo::PRINTING ? 0x22ff22_cc : 0x77_c);
	y += 10;
	if (filename) {
		draw::outline_multi_text(matrix.get_inactive_buffer(), font::tahoma_9::info, 1, y, *filename, 0x2222ff_cc);
		y += 8;
	}

	if (pinfo->status_code == pinfo->PRINTING) {
		const auto &binfo = servicer.slot<slots::PrinterBitmapInfo>(slots::PRINTER_BITMAP_INFO);
		char buf[32];
		if (binfo && binfo->file_process_percent == binfo->PROCESSED_OK) {
			snprintf(buf, sizeof buf, "%02d%%", pinfo->percent_done);
			auto x = draw::outline_multi_text(matrix.get_inactive_buffer(), font::lcdpixel_6::info, 1, y, buf, 0x22ff22_cc, "done; ly ", 0x77_c);
			snprintf(buf, sizeof buf, "%d/%d (%d.%02d mm)", binfo->current_layer_number, pinfo->total_layer_count, binfo->current_layer_height / 100, binfo->current_layer_height % 100);
			draw::outline_text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y, 0x2222ff_cc);
			y += 7;
		}
		else {
			snprintf(buf, sizeof buf, "%02d%%", pinfo->percent_done);
			auto x = draw::outline_multi_text(matrix.get_inactive_buffer(), font::lcdpixel_6::info, 1, y, buf, 0x22ff22_cc, "done", 0x77_c);
			y += 7;
		}
		if (pinfo->estimated_print_done > rtc_time) {
			draw::format_relative_date(buf, sizeof buf, pinfo->estimated_print_done);
			draw::outline_multi_text(matrix.get_inactive_buffer(), font::lcdpixel_6::info, 1, y, "est. ", 0x77_c, buf, 0x22ff22_cc);
		}
	}
}

void screen::OctoprintScreen::refresh() {
	servicer.refresh_grabber(slots::protocol::GrabberID::PRINTER);
}

void screen::OctoprintScreen::draw_disabled_background(const char *text) {
	matrix.get_inactive_buffer().clear();
	draw::text(matrix.get_inactive_buffer(), text, font::lato_bold_15::info, 64 - draw::text_size(text, font::lato_bold_15::info) / 2, 40, 0x3333ff_cc);
}

void screen::OctoprintScreen::draw_gcode_progressbar(int8_t progress) {
	char text[32];
	led::color_t bar_color;
	int16_t bar_x_pos = 0;

	if (progress == slots::PrinterBitmapInfo::PROCESSED_OK) {
		bar_color = 0x3333ff_cc;
		snprintf(text, sizeof text, "loading bitmap");
		bar_x_pos = 128;
	}
	else if (progress == slots::PrinterBitmapInfo::PROCESSED_FAIL) {
		bar_color = 0xff3333_cc;
		snprintf(text, sizeof text, "gcode parse fail");
		bar_x_pos = 128;
	}
	else {
		bar_x_pos = progress;
		bar_x_pos *= 128;
		bar_x_pos /= 100;
		bar_color = 0x33ff33_cc;
		if (progress < 25)
			snprintf(text, sizeof text, "dl gcode (%02d%%)", (int)progress);
		else if (progress < 50)
			snprintf(text, sizeof text, "scan gcode (%02d%%)", (int)progress);
		else
			snprintf(text, sizeof text, "draw gcode (%02d%%)", (int)progress);
	}

	int16_t text_pos = 64 - draw::text_size(text, font::lcdpixel_6::info) / 2;

	matrix.get_inactive_buffer().clear();
	draw::rect(matrix.get_inactive_buffer(), 0, 57, bar_x_pos, 64, bar_color);
	draw::rect(matrix.get_inactive_buffer(), bar_x_pos, 57, 128, 64, 0);
	draw::outline_text(matrix.get_inactive_buffer(), text, font::lcdpixel_6::info, text_pos, 63, 0xff_c);
}

void screen::OctoprintScreen::draw_background() {
	const auto &bii_blk = servicer.slot<slots::PrinterBitmapInfo>(slots::PRINTER_BITMAP_INFO);
	const auto &bit_blk = servicer[slots::PRINTER_BITMAP];

	if (!bii_blk)
	{
		matrix.get_inactive_buffer().clear();
		return;
	}

	if (bii_blk->file_process_percent != slots::PrinterBitmapInfo::PROCESSED_OK) {
		draw_gcode_progressbar(bii_blk->file_process_percent);
		return;
	}

	if (!bit_blk || bit_blk.datasize < ((bii_blk->bitmap_width * bii_blk->bitmap_height) / 8)) {
		draw_gcode_progressbar(bii_blk->file_process_percent);
		return;
	}

	if (servicer.slot_dirty(slots::PRINTER_BITMAP)) {
		has_valid_bitmap_drawn = false;
	}

	fm::fixed_t aspect_ratio = fm::fixed_t(bii_blk->bitmap_width) / bii_blk->bitmap_height;

	// Determine how the bitmap should be presented: 
	// 
	// There are 2 modes: scrolling and fixed.
	//
	// We can also decide to rotate the model 90 degrees, which we do if:
	// 	it is taller than it is wide, and
	// 	it is not relatively square
	
	bool rotate_90 = aspect_ratio < fm::fixed_t(9, 10);
	
	fm::fixed_t onscreen_ratio = rotate_90 ? fm::fixed_t(bii_blk->bitmap_height) / bii_blk->bitmap_width : aspect_ratio;
	int x_size = std::min<int>(128, rotate_90 ? bii_blk->bitmap_height : bii_blk->bitmap_width);
	int x_min = 64 - x_size / 2;
	int x_max = 64 + x_size / 2;
	int y_min = (32 - (x_size / onscreen_ratio / 2)).round();	
	int y_max = (32 + (x_size / onscreen_ratio / 2)).round();	

	led::color_t filament_color;
	{
		const auto& pinfo = *servicer.slot<slots::PrinterInfo>(slots::PRINTER_INFO);
		filament_color.r = static_cast<uint16_t>(pinfo.filament_r) << 4; // << 4 is for _cu
		filament_color.g = static_cast<uint16_t>(pinfo.filament_g) << 4;
		filament_color.b = static_cast<uint16_t>(pinfo.filament_b) << 4;
	}

	if (onscreen_ratio > fm::fixed_t(195, 100) && onscreen_ratio < fm::fixed_t(208, 100)) {
		// Do not scroll the bitmap region, just map it directly to the screen centred. Some clipping
		// might occur.
		fill_gcode_area(bit_blk, *bii_blk, x_min, y_min, x_max, y_max, rotate_90, filament_color);
	}
	else {
		// If scrolling is required, scroll up and down.
		if (y_min < 0) {
			int offset = 0; 
			if (y_min < -16)
				offset = draw::fastsin(timekeeper.current_time, 1500, 2 + -y_min);
			else
				offset = draw::fastsin(timekeeper.current_time, 2500, 2 + -y_min);
			y_min += offset;
			y_max += offset;
		}
		fill_gcode_area(bit_blk, *bii_blk, x_min, y_min, x_max, y_max, rotate_90, filament_color);
	}
}

template<bool Rotate90>
void screen::OctoprintScreen::fill_gcode_area_impl(const bheap::TypedBlock<uint8_t *>& bitmap, const slots::PrinterBitmapInfo& binfo, 
							 int x0, int y0, int x1, int y1, led::color_t filament) {
	// Precompute:
	// 	how far each step in x and y take us in the bitmap.
	//
	// for normal, x goes along x axes, otherwise it goes along y axis (vise versa for y step).
	// these reference different axes in the bitmap w.r.t Rotate90 -- x_step is the distance travelled along one motion in the
	// screen x axis, it is a vector in either the bitmap's x/y direction depending on Rotate90.
	fm::fixed_t x_step = (Rotate90 ? binfo.bitmap_height - 1 : binfo.bitmap_width - 1) / fm::fixed_t(x1 - x0);
	fm::fixed_t y_step = (Rotate90 ? binfo.bitmap_width  - 1: binfo.bitmap_height - 1) / fm::fixed_t(y1 - y0);

	// these are referenced to the bitmap -- x is the actual x coord (after rotation), sim. for y.
	// _tail_bpos is the opposite end of a rectangle containing all pixels to sample.
	fm::fixed_t x_bpos = Rotate90 ? binfo.bitmap_width - y_step : 0, y_bpos = 0;
	fm::fixed_t x_tail_bpos = Rotate90 ? binfo.bitmap_width : x_step, y_tail_bpos = Rotate90 ? x_step : y_step;

	auto& fb = matrix.get_inactive_buffer();
	const auto& last_fb = matrix.get_active_buffer();

	// determine if cache can be used
	if (last_x1 - last_x0 != x1 - x0 || last_y1 - last_y0 != y1 - y0)
		has_valid_bitmap_drawn = false;

	// Declared outside of loop for speed
	int cache_x_offset = 0, cache_y_offset = 0;
	bool cache_valid = false;
	led::color_t cache_color{};

	if (has_valid_bitmap_drawn) {
		cache_x_offset = last_x0 - x0;
		cache_y_offset = last_y0 - y0;
	}

	// Used as upper bits of red channel for valid cached data.
	const static uint16_t cache_code = 0b1010;

	// Fill the edges of the screen with black (we don't manually clear)
	if (x0 > 0) {
		draw::rect(fb, 0, 0, x0, 64, 0);
	}
	if (x1 < 128) {
		draw::rect(fb, x0, 0, 128, 64, 0);
	}
	if (y0 > 0) {
		draw::rect(fb, std::max(0, x0), 0, std::min(128, x1), y0, 0);
	}
	if (y1 < 64) {
		draw::rect(fb, std::max(0, x0), y1, std::min(128, x1), 64, 0);
	}

	for (int y = y0; y < y1; ++y) {
		for (int x = x0; x < x1; ++x) {
			if (fb.on_screen(x, y)) {
				if (has_valid_bitmap_drawn) {
					int cache_x = x + cache_x_offset;
					int cache_y = y + cache_y_offset;
					if (last_fb.on_screen(cache_x, cache_y)) {
						const auto& screen_color = last_fb.at(cache_x, cache_y);
						if (screen_color.r >> 12 == cache_code) {
							cache_color = screen_color;
							cache_valid = true;
						}
						else cache_valid = false;
					}
					else cache_valid = false;
				}
				if (!cache_valid) {
					int region_count = 0, hit_count = 0;
					for (int by = std::min<int>(binfo.bitmap_height - 1, y_bpos.round()); 
							by <= std::min<int>(binfo.bitmap_height - 1, y_tail_bpos.round()); ++by) {
						for (int bx = std::min<int>(binfo.bitmap_width - 1, x_bpos.round()); 
							bx <= std::min<int>(binfo.bitmap_width - 1, x_tail_bpos.round()); ++bx) {

							int bit_idx = by * binfo.bitmap_width + bx;
							if (bitmap[bit_idx / 8] & (1 << (bit_idx % 8))) ++hit_count;
							++region_count;
						}
					}
					cache_color = draw::cvt(
						(0_ccu).mix(filament, (hit_count*255)/region_count)
					);
					cache_color.r |= cache_code << 12;
				}
				fb.at_unsafe(x, y) = cache_color;
			}

			if constexpr (Rotate90) {
				y_bpos += x_step;
				y_tail_bpos += x_step;
			} 
			else {
				x_bpos += x_step;
				x_tail_bpos += x_step;
			}
		}
		if constexpr (Rotate90) {
			x_bpos -= y_step;
			x_tail_bpos -= y_step;
			y_bpos = 0;
			y_tail_bpos = x_step;
		}
		else {
			y_bpos += y_step;
			y_tail_bpos += y_step;
			x_bpos = 0;
			x_tail_bpos = x_step;
		}
	}

	has_valid_bitmap_drawn = true;
	last_x0 = x0;
	last_x1 = x1;
	last_y0 = y0;
	last_y1 = y1;
}
