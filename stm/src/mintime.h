#ifndef MINTIME_H
#define MINTIME_H

#include <stdint.h>

// min-time
namespace mint {
	// Vaguely compliant version of time.h's struct tm
	struct tm {
		tm() = delete;
		// specific time
		explicit tm(uint64_t millis);
	
		int tm_milli;
		int tm_sec;
		int tm_min;
		int tm_hour;
		int tm_wday;
		int tm_day;
		int tm_mon;
		int tm_year;

		const char * abbrev_month() const;
		const char * abbrev_weekday() const;
	};

	tm now(); 
}

#endif
