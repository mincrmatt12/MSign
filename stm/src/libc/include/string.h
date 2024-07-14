#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifndef __clang__
__attribute__((
	access(write_only, 1, 3),
	access(read_only,  2, 3)
))
#endif
void* memcpy(void * __restrict__ dest, const void * __restrict__ src, size_t count);
#ifndef __clang__
__attribute__((
	access(write_only, 1, 3),
	access(read_only,  2, 3)
))
#endif
void* memmove(void* dest, const void *src, size_t count);
#ifndef __clang__
__attribute__((
	access(write_only, 1, 3),
))
#endif
void* memset(void* dest, int ch, size_t count);
#ifndef __clang__
__attribute((
	null_terminated_string_arg(1)
))
#endif
size_t strlen(const char *s);
#ifndef __clang__
__attribute((
	access(write_only, 1, 3),
	null_terminated_string_arg(2),
))
#endif
char* strncpy(char * __restrict__ dest, const char * __restrict__ src, size_t count);

#ifdef __cplusplus
}
#endif
