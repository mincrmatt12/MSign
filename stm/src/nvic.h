#ifndef NVIC_H
#define NVIC_H

namespace nvic {
	void init();
    [[noreturn]] void show_error_screen(const char * errcode);
}

#endif
