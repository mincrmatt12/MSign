#include "tdsb.h"
#include "../draw.h"
#include "../common/slots.h"
#include "../srv.h"
#include "../fonts/latob_11.h"
#include "../rng.h"

extern uint64_t rtc_time;
extern matrix_type matrix;
extern srv::Servicer servicer;

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
		memcpy(activecolor, colorbeat[0], 3);

		for (int i = 0; i < 3; ++i) {
			activecolor[i] += 
				draw::distorted_ease_wave(rtc_time - last_colorbeat, 200, 800, colorbeat[1][i] - colorbeat[0][i]);
		}

		draw::hatched_rect(matrix.get_inactive_buffer(), x0, y0, x1, y1, activecolor[0], activecolor[1], activecolor[2], 0, 0, 0);
	}

	void TDSBScreen::draw_current_segment(int16_t y, SegmentType type, const uint8_t * course_code, const uint8_t * course_title, const uint8_t * teacher_name, const uint8_t * room_text) {
		// TODO
	}
	void TDSBScreen::draw_next_segment(int16_t y, SegmentType type) {
		// TODO
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
				
				int16_t pos = 96 / 2 - 
					draw::text_size(buf, font::lato_bold_11::info) / 2 + 
					static_cast<int16_t>(6 * std::sin((static_cast<float>(rtc_time % 2000) / 2000.f) * static_cast<float>(M_PI)));

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

			// Draw bottom area
			switch (hdr->current_layout) {
				case slots::TimetableHeader::AM_PM:
				case slots::TimetableHeader::AM_INCLASS_PM:
				case slots::TimetableHeader::PM_ONLY:
					draw_current_segment(39, SynchronousSchool, *servicer[slots::TIMETABLE_PM_CODE],
							*servicer[slots::TIMETABLE_AM_NAME], *servicer[slots::TIMETABLE_PM_TEACHER]
					);
					break;
				default:
					draw_noschool(0, 39, 96, 64);
					break;
			}

			// Draw PM stamp
		}

		// Fill right hand section (simplifies scrolling design) + draw vertical divider
		draw::rect(matrix.get_inactive_buffer(), 96, 0, 128, 64, 0, 0, 0);
		draw::rect(matrix.get_inactive_buffer(), 96, 0, 97, 64,  255_c, 255_c, 255_c);

		// Is there school setup for tommorow?
		if (hdr->next_day) {
			// Draw next_day header text

			// Draw top area

			// Draw bottom area
		}
		else {
			// Draw noschool (shrunk size)
		}
	}
}
