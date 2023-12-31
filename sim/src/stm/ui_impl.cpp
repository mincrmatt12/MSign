#include "ui.h"
#include "srv.h"

#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>

#include <fcntl.h>

extern srv::Servicer servicer;

static int ui_shm_fd = -1;

static struct SharedData {
	uint16_t GPIOA_IDR;
	uint16_t adc_x_raw;
	uint16_t adc_y_raw;
} *ui_shm_dat = nullptr;

namespace ui {
	const uint32_t physical_button_masks[]{
		/* [Buttons::MENU] = */ (1 << 7),
		/* [Buttons::TAB] = */ (1 << 6),
		/* [Buttons::SEL] = */ (1 << 8),
		/* [Buttons::POWER] = */ (1 << 0),
		/* [Buttons::STICK] = */ (1 << 5)
	};
}

void ui::Buttons::init() {
	// Check if we can open the msign button communicator
	if (ui_shm_fd != -1) return;

	int fd = shm_open("/msign_buttons", O_RDONLY, 0);

	if (fd >= 0) {
		// map object
		ui_shm_fd = fd;
		ui_shm_dat = (SharedData *)mmap(NULL, sizeof(SharedData), PROT_READ, MAP_SHARED, fd, 0);

		if (ui_shm_dat == MAP_FAILED) {
			ui_shm_fd = -1;
			ui_shm_dat = nullptr;

			perror("opening mapped");
			exit(1);
		}
	}
	else {
		if (errno == EINVAL || errno == ENOENT) {
			// ok, no data
			ui_shm_dat = nullptr;
			ui_shm_fd = -1;
		}
	}
}

void ui::Buttons::update() {
	if (!last_update) last_update = xTaskGetTickCount();
	auto now = xTaskGetTickCount();
	TickType_t duration = now - last_update;
	if (duration) {
		last_update = now;
		last_duration = duration;
	}
	else last_duration = 0;

	last_held = current_held;

	if (ui_shm_dat != nullptr && ui_shm_fd >= 0) {
		current_held = 0;
		uint16_t ormask = 1;
		uint32_t sampled = ui_shm_dat->GPIOA_IDR;
		for (auto pinmask : physical_button_masks) {
			if (sampled & pinmask)
				current_held |= ormask;
			ormask <<= 1;
		}
	}

	if (adc_ready()) {
		uint16_t raw_x, raw_y;
		read_raw_adc(raw_x, raw_y);

		x_axis = adjust_adc_value(raw_x, adc_calibration.x);
		y_axis = adjust_adc_value(raw_y, adc_calibration.y);

		if (x_axis > 120) current_held |= RIGHT;
		else if (x_axis < -120) current_held |= LEFT;
		if (y_axis < -120) current_held |= UP;
		else if (y_axis > 120) current_held |= DOWN;
	}
	else if (cur_adc_state == ADC_UNINIT && servicer.ready()) {
		switch (servicer.get_adc_calibration(adc_calibration)) {
			case slots::protocol::AdcCalibrationResult::OK:
				cur_adc_state = ADC_READY;
				break;
			case slots::protocol::AdcCalibrationResult::MISSING:
				cur_adc_state = ADC_UNCALIBRATED;
				break;
			case slots::protocol::AdcCalibrationResult::MISSING_IGNORE:
				cur_adc_state = ADC_DISABLED;
				break;
			case slots::protocol::AdcCalibrationResult::TIMEOUT:
				break;
		}
	}

	for (int i = 0; i < 9; ++i) {
		if (current_held & (1 << i)) {
			held_duration[i] += duration;
		}
		else held_duration[i] = 0;
	}
}

void ui::Buttons::read_raw_adc(uint16_t &x, uint16_t &y) const {
	x = 0;
	y = 0;
	if (adc_lock) {
		return;
	}
	adc_lock = true;
	if (ui_shm_dat != nullptr && ui_shm_fd >= 0) {
		x = ui_shm_dat->adc_x_raw;
		y = ui_shm_dat->adc_y_raw;
	}
	adc_lock = false;
}
