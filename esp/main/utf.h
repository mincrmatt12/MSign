#ifndef MSN_UTF_H
#define MSN_UTF_H

// Very minimal UTF8 parser and Unicode "normalizer": converts a few select utf8-encoded unicode codepoints to normal ascii.

namespace utf8 {
	void process(char * str_in_out);
};

#endif
