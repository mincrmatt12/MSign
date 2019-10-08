#include "tasks/timekeeper.h"
#include "dispman.h"

extern tasks::Timekeeper timekeeper;
extern sched::TaskPtr task_list[8];

void tasks::DispMan::init() {
	setup(0);
	activate(0);
	setup(1);
	this->on = 0;
}

void tasks::DispMan::loop() {
	communicate();

	if (data_ok && !(this->on == 2 && this->threedbg.leaveon()) && timekeeper.current_time - this->last_screen_transition > times_on[this->on]) {
		teardown(this->on);
		this->on = next(this->on);
		activate(this->on);
		setup(next(this->on));
		last_screen_transition = timekeeper.current_time;
	}
}

void tasks::DispMan::communicate() {
	if (counter == 2) {
		servicer.open_slot(slots::SCCFG_INFO, true, this->slot_cfg, true);
		servicer.open_slot(slots::SCCFG_TIMING, false, this->slot_time, true);
		++counter;
	}
	else if (counter == 3) {
	}
	else {
		++counter;
		return;
	}

	if (!servicer.slot_connected(this->slot_cfg)) return;

	if (servicer.slot_dirty(this->slot_cfg, true)) {
		// begin reading
		
		this->data_ok = false;
		this->enabled_mask = servicer.slot<slots::ScCfgInfo>(this->slot_cfg).enabled_mask;

		servicer.ack_slot(this->slot_time);
	}

	if (servicer.slot_dirty(this->slot_time, true)) {
		const auto& dat = servicer.slot<slots::ScCfgTime>(this->slot_time);
		this->times_on[dat.idx_order] = dat.millis_enabled;

		if (dat.idx_order >= count - 1) {
			this->data_ok = true;
		}
		else {
			servicer.ack_slot(this->slot_time);
		}
	}
}

void tasks::DispMan::setup(uint8_t i) {
	switch (i) {
		case 0:
			ttc.init();
			break;
		case 1:
			weather.init();
			break;
		case 2:
			threedbg.init();
			clockfg.init();
			break;
		case 3:
			calfix.init();
			break;
	}
}

void tasks::DispMan::teardown(uint8_t i, bool call_deinit) {
	switch (i) {
		case 0:
			ttc.deinit();
			break;
		case 1:
			weather.deinit();
			break;
		case 2:
			threedbg.deinit();
			clockfg.deinit();

			task_list[1] = nullptr;
			break;
		case 3:
			calfix.deinit();
			break;
	}
}

void tasks::DispMan::activate(uint8_t i) {
	switch (i) {
		case 0:
			task_list[0] = &ttc;
			break;
		case 1:
			task_list[0] = &weather;
			break;
		case 2:
			task_list[0] = &threedbg;
			task_list[1] = &clockfg;
			break;
		case 3:
			task_list[0] = &calfix;
			break;
	}
}

uint8_t tasks::DispMan::next(uint8_t i) {
	++i; i %= count;
	if (!enabled_mask) return i; // avoid infinite recursion
	return (enabled_mask & (1 << i)) ? i : next(i);
}
