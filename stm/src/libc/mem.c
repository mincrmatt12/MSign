#include <stdint.h>
#include <stddef.h>

// These definitions are not optimized, however we build this with -O3 so in practice 
// memcpy() becomes more efficient than newlib-nano without having to make a bunch of
// ugly manual loop unrolling
//
// Additionally, we have to follow the spec here and return the original dest so that
// we can safely compile with -fbuiltin

void *memcpy(void * __restrict__ dest, const void * __restrict__ src, size_t count) {
	char * __restrict__ _dest = (char *)dest;
	const char * __restrict__ _src  = (const char *)src;

	while (count--) {
		*_dest++ = *_src++;
	}

	return dest;
}

// no restrict because it's memmove
void *memmove(void * dest, const void * src, size_t count) {
	char * _dest = (char *)dest;
	const char * _src  = (const char *)src;

	// if overlap would occur while going forwards, go backwards instead
	if (_src < _dest && _dest < _src + count) {
		_src  += count;
		_dest += count;
		while (count--) {
			*--_dest = *--_src;
		}
		return dest;
	}
	else return memcpy(dest, src, count);
}

void *memset(void *dest, int ch, size_t count) {
	char * dst = (char *)dest;
	while (count--) *dst++ = ch;
	return dest;
}
