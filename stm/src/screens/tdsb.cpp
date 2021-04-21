#include "tdsb.h"
#include "../draw.h"
#include "../common/slots.h"
#include "../srv.h"
#include "../fonts/latob_11.h"
#include "../fonts/tahoma_9.h"
#include "../fonts/lcdpixel_6.h"
#include "../fonts/dejavu_12.h"
#include "../rng.h"

extern uint64_t rtc_time;
extern matrix_type matrix;
extern srv::Servicer servicer;

namespace bitmap::tdsb {
	// w=6, h=9, stride=1, color=255, 255, 255
	const uint8_t door[] = {
		0b11111100,
		0b10000100,
		0b10110100,
		0b10110100,
		0b10000100,
		0b10001100,
		0b10000100,
		0b10000100,
		0b11111100
	};

	// w=14, h=9, stride=2, color=255, 255, 255
	const uint8_t bed[] = {
		0b10000000,0b00000000,
		0b11000000,0b00000000,
		0b11000000,0b00000000,
		0b11111111,0b11111000,
		0b11111111,0b11111100,
		0b11111111,0b11111100,
		0b10000000,0b00000100,
		0b10000000,0b00000100,
		0b10000000,0b00000100
	};

	// w=14, h=9, stride=2, color=255, 255, 255
	const uint8_t monitor[] = {
		0b11111111,0b11111100,
		0b10000000,0b00000100,
		0b10111111,0b11000100,
		0b10000000,0b00000100,
		0b10111111,0b11110100,
		0b10000000,0b00000100,
		0b11111111,0b11111100,
		0b00000011,0b00000000,
		0b00000111,0b10000000
	};
}

namespace screen {
	void TDSBScreen::prepare(bool) {
		servicer.set_temperature_all(bheap::Block::TemperatureWarm, 
			slots::TIMETABLE_AM_CODE,
			slots::TIMETABLE_AM_TEACHER,
			slots::TIMETABLE_AM_NAME,
			slots::TIMETABLE_AM_ROOM,
			slots::TIMETABLE_PM_CODE,
			slots::TIMETABLE_PM_TEACHER,
			slots::TIMETABLE_PM_NAME,
			slots::TIMETABLE_PM_ROOM,
			slots::TIMETABLE_HEADER
		);
	}

	TDSBScreen::TDSBScreen() {
		servicer.set_temperature_all(bheap::Block::TemperatureHot, 
			slots::TIMETABLE_AM_CODE,
			slots::TIMETABLE_AM_TEACHER,
			slots::TIMETABLE_AM_NAME,
			slots::TIMETABLE_AM_ROOM,
			slots::TIMETABLE_PM_CODE,
			slots::TIMETABLE_PM_TEACHER,
			slots::TIMETABLE_PM_NAME,
			slots::TIMETABLE_PM_ROOM,
			slots::TIMETABLE_HEADER
		);

		// initialize colorbeat
		for (int i = 0; i < 6; ++i) {
			colorbeat[i / 3][i % 3] = rng::getclr();
		}
		last_colorbeat = rtc_time;
	}

	TDSBScreen::~TDSBScreen() {
		servicer.set_temperature_all(bheap::Block::TemperatureCold, 
			slots::TIMETABLE_AM_CODE,
			slots::TIMETABLE_AM_TEACHER,
			slots::TIMETABLE_AM_NAME,
			slots::TIMETABLE_AM_ROOM,
			slots::TIMETABLE_PM_CODE,
			slots::TIMETABLE_PM_TEACHER,
			slots::TIMETABLE_PM_NAME,
			slots::TIMETABLE_PM_ROOM,
			slots::TIMETABLE_HEADER
		);
	}

	void TDSBScreen::draw_noschool(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
		// update colorbeat (period = 1000)
		if (rtc_time - last_colorbeat > 1000) {
			last_colorbeat = rtc_time;
			for (int i = 0; i < 3; ++i) {
				colorbeat[0][i] = colorbeat[1][i];
				colorbeat[1][i] = rng::getclr();
			}
		}

		// party colors
		int16_t activecolor[3];
		for (int i = 0; i < 3; ++i) {
			// offset
			float offset = colorbeat[1][i] - colorbeat[0][i];

			// scale offset by pos 
			float pos = (rtc_time - last_colorbeat) / 1000.f;
			pos = powf(pos, 20.f);

			offset *= pos;
			activecolor[i] = colorbeat[0][i] + (int16_t)offset;
		}

		draw::hatched_rect(matrix.get_inactive_buffer(), x0, y0, x1, y1, activecolor[0], activecolor[1], activecolor[2], 0, 0, 0);
		
		int16_t noschool_size = draw::text_size("no school", font::dejavusans_12::info) + /* padding */ 2;

		// pick graphic colours based on party
		for (int i = 0; i < 3; ++i) {
			// offset
			float offset = colorbeat[0][i] - colorbeat[1][i];

			// scale offset by pos 
			float pos = (rtc_time - last_colorbeat) / 1000.f;
			pos = powf(pos, 20.f);

			offset *= pos;
			activecolor[i] = colorbeat[1][i] + (int16_t)offset;
		}

		// can we fit the graphic and the text?
		if ((x1 - x0) > noschool_size && (y1 - y0) > (12 /* font height */ + 9 /* bed height */ + 2 /* padding */)) {
				
			// fill empty space for padding
			//   bed
			draw::rect(matrix.get_inactive_buffer(), (x0 + x1) / 2 - ((14 / 2) + 1), (y0 + y1) / 2 - 10, (x0 + x1) / 2 + ((14 / 2) + 1), (y0 + y1) / 2, 0, 0, 0);
			//   text
			draw::rect(matrix.get_inactive_buffer(), (x0 + x1) / 2 - ((noschool_size / 2) + 1), (y0 + y1) / 2, (x0 + x1) / 2 + ((noschool_size / 2) + 1), (y0 + y1) / 2 + 12, 0, 0, 0);

			// actual graphics
			draw::text(matrix.get_inactive_buffer(), "no school", font::dejavusans_12::info, (x0 + x1) / 2 - ((noschool_size / 2)), (y0 + y1) / 2 + 11, 127_c, 127_c, 127_c);
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::tdsb::bed, 14, 9, 2, (x0 + x1) / 2 - (14 / 2), (y0 + y1) / 2 - 9, activecolor[0], activecolor[1], activecolor[2]);
		}
		// can we fit the graph
		else if ((x1 - x0) > (14 + 2) && (y1 - y0) > (9 + 2)) {
			draw::rect(matrix.get_inactive_buffer(), (x0 + x1) / 2 - ((14 / 2) + 1), (y0 + y1) / 2 - 5, (x0 + x1) / 2 + ((14 / 2) + 1), (y0 + y1) / 2 + 5, 0, 0, 0);
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::tdsb::bed, 14, 9, 2, (x0 + x1) / 2 - (14 / 2), (y0 + y1) / 2 - 4, activecolor[0], activecolor[1], activecolor[2]);
		}
	}

	void TDSBScreen::draw_current_segment(int16_t y, SegmentType type, const uint8_t * course_code, const uint8_t * course_title, const uint8_t * teacher_name, const uint8_t * room_text) {
		// Show stripey pattern
		draw::hatched_rect_unaligned(matrix.get_inactive_buffer(), 0 - ((rtc_time / 200) % 4), y, 96, y+26,
				y == 13 ? 65_c  : 39_c,
				y == 13 ? 119_c : 140_c,
				y == 13 ? 145_c : 223_c,
				0, 0, 0
		);

		// Display scrolling bottom text
		{
			char bottomtext[128];
			snprintf(bottomtext, 128, "%s / %s", course_title, teacher_name);
			int16_t x = draw::scroll(rtc_time / 11, draw::text_size(bottomtext, font::tahoma_9::info), 71);
			// draw text
			draw::rect(matrix.get_inactive_buffer(), 0, y + 13, 96, y + 26, 0, 0, 0);
			draw::multi_text(matrix.get_inactive_buffer(), font::tahoma_9::info, x + 25, y + 22, course_title, 224_c, 126_c, 0, " / ", 127_c, 127_c, 127_c, teacher_name, 255_c, 255_c, 255_c);
		}

		// Display static top screen text
		{
			int16_t x = 95 - draw::text_size(course_code, font::dejavusans_12::info);
			draw::rect(matrix.get_inactive_buffer(), x, y + 1, 95, y + 12, 0, 0, 0);
			draw::text(matrix.get_inactive_buffer(), course_code, font::dejavusans_12::info, x, y + 11, 255_c, 255_c, 255_c);
			draw::rect(matrix.get_inactive_buffer(), 0, y + 14, 21, y + 26, 0, 0, 0);
		}

		switch (type) {
			case InPersonSchool:
				{
					// Draw door icon
					draw::bitmap(matrix.get_inactive_buffer(), bitmap::tdsb::door, 6, 9, 1, 1, y + 15, 255_c, 180_c, 175_c);
					// Draw room text
					draw::text(matrix.get_inactive_buffer(), room_text, font::lcdpixel_6::info, 9, y + 22, 255_c, 255_c, 255_c);
				}
				break;
			case AsynchronousSchool:
				{
					// Draw bed icon
					draw::bitmap(matrix.get_inactive_buffer(), bitmap::tdsb::bed, 14, 9, 2, 3, y + 15, 200_c, 255_c, 200_c);
				}
				break;
			case SynchronousSchool:
				{
					// Draw monitor icon
					draw::bitmap(matrix.get_inactive_buffer(), bitmap::tdsb::monitor, 14, 9, 2, 3, y + 15, 204_c, 205_c, 255_c);
				}
			default:
				break;
		}
	}
	void TDSBScreen::draw_next_segment(int16_t y, SegmentType type) {
		// Only draw ideographs 

		switch (type) {
			case InPersonSchool:
				{
					// Draw door icon
					draw::bitmap(matrix.get_inactive_buffer(), bitmap::tdsb::door, 6, 9, 1, 109, y + 15, 255_c, 180_c, 175_c);
				}
				break;
			case AsynchronousSchool:
				{
					// Draw bed icon
					draw::bitmap(matrix.get_inactive_buffer(), bitmap::tdsb::bed, 14, 9, 2, 105, y + 15, 200_c, 255_c, 200_c);
				}
				break;
			case SynchronousSchool:
				{
					// Draw monitor icon
					draw::bitmap(matrix.get_inactive_buffer(), bitmap::tdsb::monitor, 14, 9, 2, 105, y + 15, 204_c, 205_c, 255_c);
				}
			default:
				break;
		}
	}

	void TDSBScreen::draw() {
		srv::ServicerLockGuard g(servicer);
		const auto& hdr = servicer.slot<slots::TimetableHeader>(slots::TIMETABLE_HEADER);

		if (!hdr) {
			// draw the noschool screen as a temp TODO
			draw_noschool(0, 0, 128, 64);
			return;
		}

		// Check if we can defer to full-screen no-school
		if (hdr->current_day == 0 && hdr->next_day == 0) {
			draw_noschool(0, 0, 128, 64);
			return;
		}

		// Is there school setup for today?
		if (hdr->current_day) {
			// Draw current_day header text
			{
				char buf[16];
				snprintf(buf, 16, "Day %d", hdr->current_day);
				
				int16_t pos = 96 / 2 - draw::text_size(buf, font::lato_bold_11::info) / 2;

				draw::text(matrix.get_inactive_buffer(), buf, font::lato_bold_11::info, pos, 9, 255_c, 255_c, 255_c);
			}
			// Draw top area

			switch (hdr->current_layout) {
				case slots::TimetableHeader::AM_INCLASS_ONLY:
				case slots::TimetableHeader::AM_INCLASS_PM:
					draw_current_segment(13, InPersonSchool, *servicer[slots::TIMETABLE_AM_CODE],
							*servicer[slots::TIMETABLE_AM_NAME], *servicer[slots::TIMETABLE_AM_TEACHER],
							*servicer[slots::TIMETABLE_AM_ROOM]
					);
					break;
				case slots::TimetableHeader::AM_ONLY:
				case slots::TimetableHeader::AM_PM:
					draw_current_segment(13, AsynchronousSchool, *servicer[slots::TIMETABLE_AM_CODE],
							*servicer[slots::TIMETABLE_AM_NAME], *servicer[slots::TIMETABLE_AM_TEACHER]
					);
					break;
				default:
					draw_noschool(0, 13, 96, 38);
					break;
			}

			// Draw "AM" stamp
			draw::rect(matrix.get_inactive_buffer(), 1, 14, 10, 21, 0, 0, 0);
			draw::text(matrix.get_inactive_buffer(), "AM", font::lcdpixel_6::info, 2, 20, 255_c, 255_c, 255_c);
			
			// Draw divider
			draw::rect(matrix.get_inactive_buffer(), 0, 38, 96, 39, 255_c, 255_c, 255_c);

			// Draw bottom area
			switch (hdr->current_layout) {
				case slots::TimetableHeader::AM_PM:
				case slots::TimetableHeader::AM_INCLASS_PM:
				case slots::TimetableHeader::PM_ONLY:
					draw_current_segment(39, SynchronousSchool, *servicer[slots::TIMETABLE_PM_CODE],
							*servicer[slots::TIMETABLE_PM_NAME], *servicer[slots::TIMETABLE_PM_TEACHER]
					);
					break;
				default:
					draw_noschool(0, 39, 96, 64);
					break;
			}

			// Draw PM stamp
			draw::rect(matrix.get_inactive_buffer(), 1, 40, 10, 47, 0, 0, 0);
			draw::text(matrix.get_inactive_buffer(), "PM", font::lcdpixel_6::info, 2, 46, 255_c, 255_c, 255_c);
		}
		else {
			// Draw noschool
			draw_noschool(0, 0, 96, 64);
		}

		// Fill right hand section (simplifies scrolling design) + draw vertical divider
		draw::rect(matrix.get_inactive_buffer(), 96, 0, 128, 64, 0, 0, 0);
		draw::rect(matrix.get_inactive_buffer(), 96, 0, 97, 64,  255_c, 255_c, 255_c);
		// Draw vertical divider
		draw::rect(matrix.get_inactive_buffer(), (hdr->current_day ? 0 : 96), 12, 128, 13, 255_c, 255_c, 255_c);

		// Is there school setup for tommorow?
		if (hdr->next_day) {
			// Draw next_day header text
			{
				// TODO: cool "rightwards moving" animation on this
				char buf[16];
				snprintf(buf, 16, "> D%d", hdr->next_day);
				
				int16_t pos = 96 + 32 / 2 - draw::text_size(buf, font::tahoma_9::info) / 2;

				draw::text(matrix.get_inactive_buffer(), buf, font::tahoma_9::info, pos, 9, 200_c, 200_c, 200_c);
			}
			// Draw top area
			switch (hdr->next_layout) {
				case slots::TimetableHeader::AM_INCLASS_ONLY:
				case slots::TimetableHeader::AM_INCLASS_PM:
					draw_next_segment(13, InPersonSchool);
					break;
				case slots::TimetableHeader::AM_ONLY:
				case slots::TimetableHeader::AM_PM:
					draw_next_segment(13, AsynchronousSchool);
					break;
				default:
					draw_noschool(97, 13, 128, 38);
					break;
			}

			// Draw "AM" stamp (if not already drawn for left side)
			if (!hdr->current_day) {
				draw::text(matrix.get_inactive_buffer(), "AM", font::lcdpixel_6::info, 98, 19, 255_c, 255_c, 255_c);
			}
			
			// Draw divider
			draw::rect(matrix.get_inactive_buffer(), 96, 38, 128, 39, 255_c, 255_c, 255_c);
			
			// Draw bottom area
			switch (hdr->next_layout) {
				case slots::TimetableHeader::AM_PM:
				case slots::TimetableHeader::AM_INCLASS_PM:
				case slots::TimetableHeader::PM_ONLY:
					draw_next_segment(39, SynchronousSchool);
					break;
				default:
					draw_noschool(97, 39, 128, 64);
					break;
			}

			// Draw PM stamp (if not already drawn for left side)
			if (!hdr->current_day) {
				draw::text(matrix.get_inactive_buffer(), "PM", font::lcdpixel_6::info, 98, 45, 255_c, 255_c, 255_c);
			}
		}
		else {
			// Draw noschool (shrunk size)
			draw_noschool(97, 0, 128, 64);
		}
	}
}
