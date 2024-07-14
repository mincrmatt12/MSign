#include <stdint.h>
#include <stddef.h>

#ifndef SIM
size_t strlen(const char *s) {
	// We could use the fun bit magic in glibc to read 4 bytes at a time, but
	// the strings we parse are never going to be that long so just go one byte at a time
	size_t n = 0;
	while (*s++) ++n;
	return n;
}
#endif

char* strncpy(char * __restrict__ dest, const char * __restrict__ src, size_t count) {
	size_t pos = 0;

	if (count == 0)
		return dest;

	// copy until null terminator is reached
	while (pos < count && *src) {
		*dest++ = *src++;
		++pos;
	}

	// if we filled the entire buffer back up a place for a new null terminator
	if (pos == count) {
		--pos;
		--dest;
	}

	// NONSTANDARD: terminate the buffer with a single null in all cases
	*dest = 0;

	// return the original pointer
	return dest - pos;
}
