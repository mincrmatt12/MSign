#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

enum sign_kind {
	only_negative,
	always,
	space_or_positive
};

static inline enum sign_kind update_signkind(enum sign_kind sk, enum sign_kind op) {
	switch (op) {
		case always:
			return op;
		case space_or_positive:
			if (sk != always) return op;
		default:
			return sk;
	}
}

struct conv_buf {
	union {
		char   res[11];
		const char * src;
	};
	uint8_t reslen; // 0xff = use strlen(src)
	char sign;
};

struct conv_buf write_int_unsigned(unsigned value, bool hex) {
	struct conv_buf res = {
		.reslen = 0,
		.sign = '\0'
	};

	unsigned base = hex ? 16 : 10;
	static const char digits[] = "0123456789abcdef";

	do {
		res.res[res.reslen++] = digits[value % base];
		value /= base;
	} while (value);

	return res;
}

struct conv_buf write_int_signed(int value, enum sign_kind sk, bool hex) {
	struct conv_buf res = write_int_unsigned(
		(value < 0) ? -((unsigned)value) : (unsigned)value,
		hex
	);

	if (value < 0) {
		res.sign = '-';
	}
	else switch (sk) {
		case always:
			res.sign = '+';
			break;
		case space_or_positive:
			res.sign = ' ';
		default:
			break;
	}
	return res;
}

int snprintf(char *__restrict__ dest, size_t length, const char *__restrict__ fmt, ...) {
	va_list args;
	va_start(args, fmt);

	int count = 0; char c;

	char * dest_end = dest + length - 1;

#define WRITE(c) (++count, (dest < dest_end) ? *dest++ = c : c)
	
	// this loop exits 
	while ((c = *fmt++)) {
		if (c != '%') {
			WRITE(c);
		}
		else {
			bool pad_zero = false;
			bool justify_right = true;
			enum sign_kind sign = only_negative;
			int  min_length = 0;
			int  max_string_length = -1; // only honored for strings
			struct conv_buf temp;

			// read the specifier
			c = *fmt++;

			// check for flags
flag:
#define next c = *fmt++; goto flag
			switch (c) {
				case '-': justify_right = false; next;
				case '+': sign = update_signkind(sign, always); next;
				case ' ': sign = update_signkind(sign, space_or_positive); next;
				case '0': pad_zero = true; next;
				deafult: break;
#undef next
			}

			// check for width
			if (c == '*') {
				// read width from arg
				min_length = va_arg(args, int);
				if (min_length < 0) {
					justify_right = false;
					min_length = -min_length;
				}
				c = *fmt++;
			}
			else while (c >= '0' && c <= '9') {
				min_length *= 10;
				min_length += c - '0';
				c = *fmt++;
			}
			// check for max length
			if (c == '.') {
				c = *fmt++;
				max_string_length = 0;
				if (c == '*') {
					// read width from arg
					max_string_length = va_arg(args, int);
					c = *fmt++;
				}
				else while (c >= '0' && c <= '9') {
					max_string_length *= 10;
					max_string_length += c - '0';
					c = *fmt++;
				}
			}
			// prepare conv_buf based on type
			if (c == 's') {
				temp = (struct conv_buf){
					.src = va_arg(args, const char *),
					.reslen = 0xff,
					.sign = '\0'
				};
			}
			else if (c == 'c') {
				temp = (struct conv_buf){
					.res = {(char)va_arg(args, int)},
					.reslen = 1,
					.sign = '\0'
				};
			}
			else if (c == 'd' || c == 'i') {
				temp = write_int_signed(va_arg(args, int), sign, false);
			}
			else if (c == 'x' || c == 'u') {
				temp = write_int_unsigned(va_arg(args, unsigned int), c == 'x');
			}
			else if (c == 'n') {
				*va_arg(args, int *) = count;
				continue;
			}
			else if (c == '%') {
				WRITE('%');
				continue;
			}
			else continue;
			if (c != 's')
				max_string_length = -1;
			// write conv buf
			//
			// determine padding:
			int slen = temp.reslen == 0xff ? strlen(temp.src) : temp.reslen;
			slen += temp.sign != '\0';
			if (max_string_length != -1 && slen > max_string_length)
				slen = max_string_length;
			const char * src = temp.reslen == 0xff ? temp.src : temp.res;
			char fill = (pad_zero && justify_right) ? '0' : ' ';
			if (justify_right) {
				while (slen < min_length) {
					WRITE(fill);
					--min_length;
				}
				if (temp.sign != '\0')
					WRITE(temp.sign);
			}
			else if (temp.sign != '\0')
				WRITE(temp.sign);
			if (temp.reslen == 0xff) {
				int eff_slen = slen;
				while ((c = *src++) && eff_slen--) {
					WRITE(c);
				}
			}
			else {
				int eff_slen = slen - (temp.sign != '\0');
				src += eff_slen;
				while (eff_slen--) {
					WRITE(*--src);
				}
			}
			if (!justify_right) {
				while (slen < min_length) {
					WRITE(fill);
					--min_length;
				}
			}
		}
	}

	va_end(args);
	*dest = 0;
	return count;
}
