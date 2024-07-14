#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

__attribute__((
	format(printf, 3, 4)
))
int snprintf(char *__restrict__ dest, size_t length, const char *__restrict__ fmt, ...);

#ifdef __cplusplus
}
#endif
