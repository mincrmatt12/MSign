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
} *ui_shm_dat = nullptr;

ui::Buttons ui::buttons;

void ui::Buttons::init() {
	if (servicer.ready()) {
		// Ask for button map to be made warm for remote control.
		servicer.set_temperature(slots::VIRTUAL_BUTTONMAP, bheap::Block::TemperatureWarm);
	}

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
	if (duration) last_update = now;

	last_held = current_held;
	current_held = 0;

	if (ui_shm_dat != nullptr && ui_shm_fd >= 0) {
		current_held = ui_shm_dat->GPIOA_IDR;
	}

	if (servicer.ready()) {
		srv::ServicerLockGuard g(servicer);

		auto& blk = servicer.slot<uint16_t>(slots::VIRTUAL_BUTTONMAP);
		if (blk && blk.datasize == 2) current_held |= *blk;
	}

	for (int i = 0; i < 11; ++i) {
		if (current_held & (1 << i)) {
			held_duration[i] += duration;
		}
		else held_duration[i] = 0;
	}
}

bool ui::Buttons::held(Button b, TickType_t mindur) const {
	return current_held & (1 << b) && held_duration[b] >= mindur;
}

bool ui::Buttons::rel(Button b) const {
	return !held(b) && (current_held ^ last_held) & (1 << b);
}

bool ui::Buttons::press(Button b) const {
	return held(b) && (current_held ^ last_held) & (1 << b);
}

bool ui::Buttons::changed() const {
	return current_held ^ last_held;
}

TickType_t ui::Buttons::frame_time() const {
	return 16;
}
