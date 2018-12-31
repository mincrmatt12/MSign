#include "weather.h"
#include "srv.h"
#include "tasks/timekeeper.h"
#include "common/slots.h"
#include "fonts/latob_15.h"

#include "draw.h"

extern uint64_t rtc_time;
extern led::Matrix<led::FrameBuffer<64, 32>> matrix;
extern tasks::Timekeeper timekeeper;
extern srv::Servicer servicer;

namespace bitmap {
}

bool tasks::WeatherScreen::init() {
	// TODO: fix me
	if (!(
		servicer.open_slot(slots::WEATHER_INFO, true, this->s_info) &&
		servicer.open_slot(slots::WEATHER_ICON, true, this->s_icon) &&
		servicer.open_slot(slots::WEATHER_STATUS, false, this->s_status)
	)) {
		return false;
	}
	this->status_update_state = 1;
	return true;
}

bool tasks::WeatherScreen::deinit() {
	// TODO: fix me
	if (!(
		servicer.close_slot(slots::WEATHER_INFO) &&
		servicer.close_slot(slots::WEATHER_ICON) &&
		servicer.close_slot(slots::WEATHER_STATUS)
	)) {
		return false;
	}
	return true;
}

void tasks::WeatherScreen::update_status() {
	switch (this->status_update_state) {
		case 0:
			return;
		case 1:
			this->status_update_state = 2;
			servicer.ack_slot(this->s_status);
			break;
		case 2:
			if (servicer.slot_dirty(this->s_status, true)) {
				const auto& vs = servicer.slot<slots::VStr>(this->s_status);
				if (vs.size > 128) {
					this->status_update_state = 0;
					return;
				}
				
				uint16_t size = 14;
				if (vs.size - vs.index <= 14){
					size = (vs.size - vs.index);
					this->status_update_state = 0;
					memcpy((this->status + vs.index), vs.data, size);
					return;
				} 

				memcpy((this->status + vs.index), vs.data, size);
				servicer.ack_slot(s_status);
			}
			[[fallthrough]];
		default:
			break;
	}
}

void tasks::WeatherScreen::loop() {
	if (!servicer.slot_connected(this->s_status)) return;
	update_status();

	if (servicer.slot_dirty(s_info, true)) this->status_update_state = 1;
	
	char disp_buf[16];
	float ctemp = servicer.slot<slots::WeatherInfo>(s_info).ctemp;
	snprintf(disp_buf, 16, "%.01f", ctemp);

	uint16_t text_size;

	text_size = draw::text_size(disp_buf, font::lato_bold_15::info);

	if (ctemp > 0.0 && ctemp < 16.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 14, 240, 240, 240);
	}
	else if (ctemp < 0.0 && ctemp > -10.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 14, 100, 100, 240);
	}
	else if (ctemp >= 16.0 && ctemp < 30.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 14, 50, 240, 50);
	}
	else if (ctemp > 30.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 14, 250, 30, 30);
	}
	else {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 14, 40, 40, 255);
	}
}
