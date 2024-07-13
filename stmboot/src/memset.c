#include "memset.h"

void memset(void *s, int c, size_t n) {
	char * dst = s;
	while (n--) *dst++ = c;
}

void memcpy(void *d, const void *s, size_t n) {
	char *dst = d;
	const char *src = s;
	while (n--) *dst++ = *src++;
}

int memcmp(const void *a, const void *b, size_t n) {
	const char *a_ = a;
	const char *b_ = b;
	while (n--)
		if (*a_++ != *b_++) return 1;
	return 0;
}
