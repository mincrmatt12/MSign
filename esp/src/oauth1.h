#ifndef MS_OAUTH1
#define MS_OAUTH1

#include <cstddef>

namespace oauth1 {
	// Creates an authorization header on the heap based on the given parameters. Can take both a lot of time
	// and a lot of RAM.
	char * generate_authorization(const char * const params[][2], const char * base_url, const char * method="GET");

	// Encoding helpers. Params array is expected to be pre-param-encoded.
	size_t percent_encoded_length(const char * in);
	void   percent_encode(const char *in, char *out);
};

#endif
