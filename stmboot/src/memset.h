#pragma once

#include <stddef.h>

void memset(void *s, int c, size_t n);
void memcpy(void *dst, const void *src, size_t n);
// bs memcmp that only returns 0 or 1
int  memcmp(const void *a, const void *b, size_t n);
