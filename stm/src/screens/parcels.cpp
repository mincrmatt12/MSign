#include "parcels.h"
#include "../srv.h"
#include "../fonts/tahoma_9.h"
#include "../fonts/dejavu_12.h"
#include "../fonts/lcdpixel_6.h"
#include "../tasks/screen.h"

extern srv::Servicer servicer;
extern matrix_type matrix;
extern uint64_t rtc_time;
extern tasks::DispMan dispman;

namespace bitmap::parcels {
	// w=11, h=11, stride=2, color=255, 255, 255
	const uint8_t cancelled[] = {
		0b00000000,0b00000000,
		0b00000100,0b00000000,
		0b00001000,0b10000000,
		0b00010001,0b00000000,
		0b00101110,0b00000000,
		0b01001010,0b01000000,
		0b00001110,0b10000000,
		0b00010001,0b00000000,
		0b00100010,0b00000000,
		0b00000100,0b00000000,
		0b00000000,0b00000000
	};

	// w=11, h=11, stride=2, color=255, 255, 255
	const uint8_t delivered[] = {
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00000001,0b00000000,
		0b00001010,0b00000000,
		0b00000100,0b00000000,
		0b00000000,0b00000000,
		0b00001110,0b00000000,
		0b00001010,0b00000000,
		0b00001110,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000
	};

	// w=11, h=11, stride=2, color=255, 255, 255
	const uint8_t in_transit[] = {
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b10000000,
		0b00111000,0b11000000,
		0b00101011,0b11100000,
		0b00111000,0b11000000,
		0b00000000,0b10000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000
	};

	// w=11, h=11, stride=2, color=255, 255, 255
	const uint8_t out_for_delivery[] = {
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00111111,0b00000000,
		0b00111110,0b10000000,
		0b00111111,0b10000000,
		0b00010001,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000
	};

	// w=11, h=11, stride=2, color=255, 255, 255
	const uint8_t error[] = {
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00001010,0b00000000,
		0b00000100,0b00000000,
		0b00001010,0b00000000,
		0b00000000,0b00000000,
		0b00001110,0b00000000,
		0b00001010,0b00000000,
		0b00001110,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000
	};

	// w=11, h=11, stride=2, color=255, 255, 255
	const uint8_t pre_transit[] = {
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00111111,0b10000000,
		0b00000000,0b00000000,
		0b00111111,0b10000000,
		0b00000000,0b00000000,
		0b00111111,0b10000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000
	};

	// w=11, h=11, stride=2, color=255, 255, 255
	const uint8_t ready_for_pickup[] = {
		0b00000000,0b00000000,
		0b00000000,0b00000000,
		0b00001110,0b00000000,
		0b00001010,0b00000000,
		0b00001110,0b00000000,
		0b00000000,0b10000000,
		0b00111111,0b10000000,
		0b00000000,0b10000000,
		0b00000000,0b10000000,
		0b00000000,0b00000000,
		0b00000000,0b00000000
	};

	// w=11, h=11, stride=2, color=255, 255, 255
	const uint8_t return_to_sender[] = {
		0b00000000,0b00000000,
		0b00001000,0b00000000,
		0b00011000,0b00000000,
		0b00111111,0b00000000,
		0b00011000,0b10000000,
		0b00001001,0b00000000,
		0b00000000,0b00000000,
		0b00001110,0b00000000,
		0b00001010,0b00000000,
		0b00001110,0b00000000,
		0b00000000,0b00000000
	};
}

screen::ParcelScreen::ParcelScreen() {
	servicer.set_temperature_all<
		slots::PARCEL_INFOS,
		slots::PARCEL_NAMES,
		slots::PARCEL_STATUS_SHORT
	>(bheap::Block::TemperatureHot);
}

screen::ParcelScreen::~ParcelScreen() {
	servicer.set_temperature_all<
		slots::PARCEL_INFOS,
		slots::PARCEL_NAMES,
		slots::PARCEL_STATUS_SHORT,
		slots::PARCEL_STATUS_LONG,
		slots::PARCEL_EXTRA_INFOS
	>(bheap::Block::TemperatureCold);
}

void screen::ParcelScreen::prepare(bool) {
	servicer.set_temperature_all<
		slots::PARCEL_INFOS,
		slots::PARCEL_NAMES,
		slots::PARCEL_STATUS_SHORT
	>(bheap::Block::TemperatureWarm);
}

void screen::ParcelScreen::refresh() {
	servicer.refresh_grabber(slots::protocol::GrabberID::PARCELS);
}

void screen::ParcelScreen::draw() {
	// TODO: some sort of header (maybe draw it to the left instead of above for variety?)
	
	srv::ServicerLockGuard g(servicer);

	const auto& parcels = servicer.slot<slots::ParcelInfo *>(slots::PARCEL_INFOS);

	if (!dispman.interacting()) {
		auto y = scroll_tracker.begin();
		
		for (const auto& parcel : parcels) {
			// Draw parcel to screen
			y += draw_short_parcel_entry(y, parcel);
		}
	}
	else {
		switch (subscreen) {
			case SubscreenSelecting:
				{
					auto y = scroll_tracker.begin();

					int i = -1;
					for (const auto& parcel : parcels) {
						++i;
						
						if (i == selected_parcel) {
							int16_t sy = y;
							y.fix(draw_short_parcel_entry(y, parcel));
							int16_t ey = y;
							draw::dashed_outline(matrix.get_inactive_buffer(), 0, sy, 127, ey-1, 0xff_c);
						}
						else {
							y += draw_short_parcel_entry(y, parcel);
						}
					}
				}
				break;
			case SubscreenLongView:
				draw_long_view(parcels[selected_parcel]);
				break;
		}
	}
}

bool screen::ParcelScreen::interact() {
	srv::ServicerLockGuard g(servicer);

	const auto& parcels = servicer.slot<slots::ParcelInfo *>(slots::PARCEL_INFOS);
	size_t n_parcels = parcels.end() - parcels.begin();

	switch (subscreen) {
		case SubscreenSelecting:
			if (ui::buttons[ui::Buttons::PRV]) {
				if (selected_parcel == 0) selected_parcel = n_parcels-1;
				else --selected_parcel;
			}
			else if (ui::buttons[ui::Buttons::NXT]) {
				if (selected_parcel == n_parcels-1) selected_parcel = 0;
				else ++selected_parcel;
			}

			if (ui::buttons[ui::Buttons::SEL]) {
				servicer.set_temperature_all<
					slots::PARCEL_EXTRA_INFOS,
					slots::PARCEL_STATUS_LONG
				>(bheap::Block::TemperatureHot);

				subscreen = SubscreenLongView;
			}

			return ui::buttons[ui::Buttons::POWER];
		case SubscreenLongView:
			if (ui::buttons[ui::Buttons::POWER]) {
				servicer.set_temperature_all<
					slots::PARCEL_EXTRA_INFOS,
					slots::PARCEL_STATUS_LONG
				>(bheap::Block::TemperatureWarm);
				subscreen = SubscreenSelecting;
			}

			if (ui::buttons[ui::Buttons::NXT]) {
				parcel_entries_scroll += 128*8;
			}
			else if (ui::buttons.held(ui::Buttons::NXT, pdMS_TO_TICKS(500))) {
				parcel_entries_scroll += ui::buttons.frame_time() * 8;
			}
			else if (ui::buttons[ui::Buttons::PRV]) {
				parcel_entries_scroll -= 128*8;
			}
			else if (ui::buttons.held(ui::Buttons::PRV, pdMS_TO_TICKS(500))) {
				parcel_entries_scroll -= ui::buttons.frame_time() * 8;
			}

			if (parcel_entries_scroll < 0) parcel_entries_scroll = 0;
			if (parcel_entries_scroll && (18 + parcel_entries_size - parcel_entries_scroll/128 < 64)) {
				parcel_entries_scroll = (18 + parcel_entries_size - 64)*128;
			}
			
			break;
	}
	return false;
}

int16_t screen::ParcelScreen::draw_long_parcel_entry(int16_t y, const slots::ParcelStatusLine& psl, const uint8_t * heap, size_t heap_size, uint64_t updated_time) {
	/* 
	 * 01234567
	 *   *  T
	 *   *  T
	 *   *  T
	 *   *  T
	 *  * * T
	 *   *  T
	 *   *  T
	 *   *  T
	 *   *  T
	 *   *
	 *   *  L
	 *   *  L
	 *   *  L
	 *   *  L
	 *   *  L
     *   *  L
	 */

	int16_t y2 = y;
	int16_t y3 = 0;

	// Draw scrolling text first (so we can mask it)
	if (psl.flags & psl.HAS_STATUS && psl.status_offset < heap_size) {
		if (y3 == 0) y3 = y2 + 10;
		draw::text(matrix.get_inactive_buffer(), heap + psl.status_offset, font::tahoma_9::info, 5 + draw::scroll(rtc_time / 10, draw::text_size(heap + psl.status_offset, font::tahoma_9::info), 123), y2 + 8, 0xff_c);
		y2 += 10;
	}

	bool second_line = false;

	// Draw nonscrolling text
	if (psl.flags & psl.HAS_LOCATION && psl.location_offset < heap_size) {
		if (y3 == 0) y3 = y2 + 6;
		draw::text(matrix.get_inactive_buffer(), heap + psl.location_offset, font::lcdpixel_6::info, 5, y2 + 5, 0x5555ff_cc);
		second_line = true;
	}
	if (psl.flags & psl.HAS_UPDATED_TIME) {
		if (y3 == 0) y3 = y2 + 6;
		char buf[32]; draw::format_relative_date(buf, 32, updated_time);
		auto sz = draw::text_size(buf, font::lcdpixel_6::info);
		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, 127 - sz, y2 + 5, 0xaa_c);
		second_line = true;
	}
	if (second_line) y2 += 9;
	
	// Mask out the scrolling bubbling line
	draw::rect(matrix.get_inactive_buffer(), 0, y, 5, y2, 0);
	draw::line(matrix.get_inactive_buffer(), 2, y, 2, y2, 0x40_c);

	// draw the bulb
	int16_t bulbpos = y + (y3 - y) / 2;
	auto& fb = matrix.get_inactive_buffer();

	fb.at(1, bulbpos) = fb.at(3, bulbpos) = fb.at(2, bulbpos - 1) = fb.at(2, bulbpos + 1) = 0x55ff55_cc;
	fb.at(2, bulbpos) = 0;

	return y2 - y + 1;
}

void screen::ParcelScreen::draw_long_view(const slots::ParcelInfo& parcel) {
	if (!servicer.slot(slots::PARCEL_STATUS_SHORT) || !servicer.slot(slots::PARCEL_STATUS_LONG) || !servicer.slot(slots::PARCEL_EXTRA_INFOS)) return;
	// expects lock to be held
	auto statuses = *servicer[slots::PARCEL_STATUS_SHORT], statuses_long = *servicer[slots::PARCEL_STATUS_LONG];
	const auto& extra_infos = servicer.slot<slots::ExtraParcelInfoEntry *>(slots::PARCEL_EXTRA_INFOS);
	
	// Draw scrollable content first
	parcel_entries_size = (parcel.status.flags & parcel.status.HAS_EST_DEILIVERY) ? 0 : -6;
	int16_t header_height = (parcel.status.flags & parcel.status.HAS_EST_DEILIVERY) ? 18 : 12;

	{
		int16_t y = header_height - parcel_entries_scroll/128;

		// draw initial location
		{
			auto sz = draw_long_parcel_entry(y, parcel.status, statuses, servicer[slots::PARCEL_STATUS_SHORT].datasize, parcel.updated_time);
			y += sz;
			parcel_entries_size += sz;
		}

		for (const auto& ei : extra_infos) {
			if (ei.for_parcel != selected_parcel) continue;
			
			// draw the entry
			auto sz = draw_long_parcel_entry(y, ei.status, statuses_long, servicer[slots::PARCEL_STATUS_LONG].datasize, ei.updated_time);
			y += sz;
			parcel_entries_size += sz;
		}

		y -= 1;
		auto& fb = matrix.get_inactive_buffer();

		// then, draw end stop/arrow
		if (parcel.status.flags & parcel.status.EXTRA_INFO_TRUNCATED) {
			y -= 1;
		}

		fb.at(1, y) = fb.at(3, y) = 0x40_c;
	}
	// Mask out header area
	draw::rect(matrix.get_inactive_buffer(), 0, 0, 127, header_height, 0);

	// Draw header text
	draw_parcel_name(0, parcel);
	// Show estimated delivery if present
	if (parcel.status.flags & parcel.status.HAS_EST_DEILIVERY) {
		char buf[48] = "est. delivery "; draw::format_relative_date(buf+strlen(buf), 48-strlen(buf), parcel.estimated_delivery);
		auto sz = draw::text_size(buf, font::lcdpixel_6::info);
		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, 127 - sz, 17, 0xaa_c);
	}
}

led::color_t screen::ParcelScreen::draw_parcel_name(int16_t y, const slots::ParcelInfo& parcel) {
	auto names = *servicer[slots::PARCEL_NAMES] ;
	if (parcel.name_offset > servicer[slots::PARCEL_NAMES].datasize) return 0;
	led::color_t iconcolorbase = 0x55_cc, iconcolorcontent = 0xff_c;
									
	switch (parcel.status_icon) {
		case slots::ParcelInfo::PRE_TRANSIT:
			iconcolorbase = 0x99_c;
			break;
		case slots::ParcelInfo::IN_TRANSIT:
			iconcolorbase = 0x4579f5_cc;
			break;
		case slots::ParcelInfo::OUT_FOR_DELIVERY:
			iconcolorbase = 0xd8f51d_cc;
			break;
		case slots::ParcelInfo::DELIVERED:
			iconcolorbase = 0x109513_cc;
			break;
		case slots::ParcelInfo::READY_FOR_PICKUP:
			iconcolorbase = 0x0b9665_cc;
			break;
		case slots::ParcelInfo::RETURN_TO_SENDER:
		case slots::ParcelInfo::FAILED_TO_DELIVER:
			iconcolorbase = 0xff4444_c;
			break;
		case slots::ParcelInfo::GENERAL_ERROR:
			iconcolorbase = 0xff7777_c;
			break;

		default:
			break;
	}

        // draw name
	draw::text(matrix.get_inactive_buffer(), names + parcel.name_offset, font::dejavusans_12::info, 12 + draw::scroll(rtc_time / 8, draw::text_size(names + parcel.name_offset, font::dejavusans_12::info), 118), y + 10, 0xff_c);

	// draw icon
	{
		draw::rect(  matrix.get_inactive_buffer(), 0, y, 6, y + 11, 0);              // fill half-circle so scrolling doesn't look weird
		draw::circle(matrix.get_inactive_buffer(), 1, y, 12, y + 11, iconcolorbase);  // circle background
		const uint8_t * icon = nullptr;
		switch (parcel.status_icon) {
			using namespace bitmap::parcels;

			case slots::ParcelInfo::PRE_TRANSIT:
				icon = pre_transit;
				break;
			case slots::ParcelInfo::IN_TRANSIT:
				icon = in_transit;
				break;
			case slots::ParcelInfo::OUT_FOR_DELIVERY:
				icon = out_for_delivery;
				iconcolorcontent = 0x3344ef_c;
				break;
			case slots::ParcelInfo::DELIVERED:
				icon = delivered;
				break;
			case slots::ParcelInfo::READY_FOR_PICKUP:
				icon = ready_for_pickup;
				break;
			case slots::ParcelInfo::RETURN_TO_SENDER:
				icon = return_to_sender;
				break;
			case slots::ParcelInfo::GENERAL_ERROR:
			case slots::ParcelInfo::FAILED_TO_DELIVER:
				icon = error;
				break;
			case slots::ParcelInfo::CANCELLED:
				icon = cancelled;
				break;
			default:
				return iconcolorbase;
		}
		draw::bitmap(matrix.get_inactive_buffer(), icon, 11, 11, 2, 1, y, iconcolorcontent); // draw icon
	}

	return iconcolorbase;
}

int16_t screen::ParcelScreen::draw_short_parcel_entry(int16_t y, const slots::ParcelInfo& parcel) {
	if (!servicer[slots::PARCEL_NAMES] || !servicer[slots::PARCEL_STATUS_SHORT]) return 0; // show loading or something
	// expects lock to be held
	auto statuses = *servicer[slots::PARCEL_STATUS_SHORT];

	int16_t height = 0;

	auto iconcolorbase = draw_parcel_name(y, parcel);

	y += 11; height += 11;

	// draw progress bar
	{
		int16_t bulbpos = 64;

		if (parcel.status.flags & slots::ParcelStatusLine::HAS_EST_DEILIVERY && parcel.status_icon != slots::ParcelInfo::DELIVERED && parcel.estimated_delivery > rtc_time) {
			// If more than 5 days out, place close to start
			int hours = (rtc_time - parcel.estimated_delivery) / 60*60*1000;
			if (hours >= 5*24) {
				bulbpos = 12;
			}
			else {
				if (hours <= 24) bulbpos = 118;
				else bulbpos = (5*24 - hours) * 128 / 5*24;
			}
		}
		else {
			switch (parcel.status_icon) {
				case slots::ParcelInfo::PRE_TRANSIT:
					bulbpos = 12;
					break;
				case slots::ParcelInfo::IN_TRANSIT:
					bulbpos = 64;
					break;
				case slots::ParcelInfo::OUT_FOR_DELIVERY:
					bulbpos = 120;
					break;
				case slots::ParcelInfo::FAILED_TO_DELIVER:
				case slots::ParcelInfo::READY_FOR_PICKUP:
				case slots::ParcelInfo::GENERAL_ERROR:
				case slots::ParcelInfo::CANCELLED:
				case slots::ParcelInfo::DELIVERED:
				case slots::ParcelInfo::RETURN_TO_SENDER:
					bulbpos = 127;
					break;
				default:
					bulbpos = 64;
					break;
			}
		}

		// draw connecting linkage
		matrix.get_inactive_buffer().at(6, y) = iconcolorbase; 
		// draw line to bulb left
		draw::line(matrix.get_inactive_buffer(), 7, y + 1, bulbpos - 1, y + 1, iconcolorbase);
		// draw top/buttom of bulb
		matrix.get_inactive_buffer().at(bulbpos, y) = matrix.get_inactive_buffer().at(bulbpos, y + 2) = matrix.get_inactive_buffer().at(bulbpos + 1, y + 1) = iconcolorbase;
		// draw line to end marker
		
		if (bulbpos < 127) {
			draw::line(matrix.get_inactive_buffer(), bulbpos + 2, y + 1, 127, y + 1, 0x22_c);
			matrix.get_inactive_buffer().at(127, y) = matrix.get_inactive_buffer().at(127, y + 2) = 0x22_c;
		}
	}

	y += 4; height += 4;

	const static auto status_font = font::tahoma_9::info;

	// draw status text
	if (parcel.status.flags & (parcel.status.HAS_STATUS | parcel.status.HAS_LOCATION)) {
		const uint8_t * status_text = (parcel.status.flags & parcel.status.HAS_STATUS) ? statuses + parcel.status.status_offset : nullptr;
		if (status_text && parcel.status.status_offset > servicer[slots::PARCEL_STATUS_SHORT].datasize) status_text = nullptr;
		const uint8_t * location_text = (parcel.status.flags & parcel.status.HAS_LOCATION) ? statuses + parcel.status.location_offset : nullptr;
		if (location_text && parcel.status.location_offset > servicer[slots::PARCEL_STATUS_SHORT].datasize) location_text = nullptr;
		bool has_both = (parcel.status.flags & (parcel.status.HAS_STATUS | parcel.status.HAS_LOCATION)) == (parcel.status.HAS_STATUS | parcel.status.HAS_LOCATION);
		// compute text size
		int textlen = (has_both ? draw::text_size(" - ", status_font) : 0)
			+ draw::text_size(status_text, status_font) + draw::text_size(location_text, status_font);
		// draw scrolling status ticker
		draw::multi_text(matrix.get_inactive_buffer(), status_font, 1 + draw::scroll(rtc_time / 8, textlen), y + 7, status_text, 0xff_c, has_both ? " - " : nullptr, 0xcc_c, location_text, 0x4444ff_cc);
		y += 9; height += 9;
	}

	// draw updated time
	if (parcel.status.flags & parcel.status.HAS_UPDATED_TIME) {
		char buf[32];
		draw::format_relative_date(buf, 32, parcel.updated_time);
		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, 127 - draw::text_size(buf, font::lcdpixel_6::info), y + 5, 0xaa_c);
		y += 6; height += 6;
	}

	return height + 1;
}
