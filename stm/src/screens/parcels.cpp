#include "parcels.h"
#include "../srv.h"
#include "../fonts/tahoma_9.h"
#include "../fonts/lcdpixel_6.h"

extern srv::Servicer servicer;
extern matrix_type matrix;
extern uint64_t rtc_time;

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
	
	{
		auto y = scroll_tracker.begin();
		
		for (const auto& parcel : parcels) {
			// Draw parcel to screen
			y += draw_short_parcel_entry(y, parcel);
		}
	}
}

int16_t screen::ParcelScreen::draw_short_parcel_entry(int16_t y, const slots::ParcelInfo& parcel) {
	if (!servicer[slots::PARCEL_NAMES] || !servicer[slots::PARCEL_STATUS_SHORT]) return 0; // show loading or something
	// expects lock to be held
	auto names = *servicer[slots::PARCEL_NAMES], statuses = *servicer[slots::PARCEL_STATUS_SHORT];

	int16_t height = 0;
	led::color_t iconcolorbase = 0x2243ce_cc; // todo: set to actual color

	// draw name
	draw::text(matrix.get_inactive_buffer(), names + parcel.name_offset, font::tahoma_9::info, 12 + draw::scroll(rtc_time / 8, draw::text_size(names + parcel.name_offset, font::tahoma_9::info), 118), y + 8, 0xff_c);

	// draw icon
	{
		// todo: icons
		draw::rect(  matrix.get_inactive_buffer(), 0, y, 5, y + 9, 0);              // fill half-circle so scrolling doesn't look weird
		draw::circle(matrix.get_inactive_buffer(), 1, y, 11, y + 10, iconcolorbase);  // circle background
	}

	y += 10; height += 10;

	// draw progress bar
	{
		// todo: determine bulb position
		int16_t bulbpos = 64;

		// draw connecting linkage
		matrix.get_inactive_buffer().at(6, y) = iconcolorbase;  // todo: do we want this a faded version?
		matrix.get_inactive_buffer().at(7, y) = iconcolorbase;  // todo: do we want this a faded version?
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
		const uint8_t * location_text = (parcel.status.flags & parcel.status.HAS_LOCATION) ? statuses + parcel.status.location_offset : nullptr;
		bool has_both = (parcel.status.flags & (parcel.status.HAS_STATUS | parcel.status.HAS_LOCATION)) == (parcel.status.HAS_STATUS | parcel.status.HAS_LOCATION);
		// compute text size
		int textlen = (has_both ? draw::text_size(" - ", status_font) : 0)
			+ draw::text_size(status_text, status_font) + draw::text_size(location_text, status_font);
		// draw scrolling status ticker
		draw::multi_text(matrix.get_inactive_buffer(), status_font, draw::scroll(rtc_time / 8, textlen), y + 7, status_text, 0xff_c, has_both ? " - " : nullptr, 0xcc_c, location_text, 0x4444ff_cc);
		y += 9; height += 9;
	}

	// draw updated time
	if (parcel.status.flags & parcel.status.HAS_UPDATED_TIME) {
		char buf[32];
		draw::format_relative_date(buf, 32, parcel.updated_time);
		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, 127 - draw::text_size(buf, font::lcdpixel_6::info), y + 5, 0xaa_c);
		// todo: make nice "time formatter"
		y += 6; height += 6;
	}

	return height + 1;
}
