#include "mintime.h"

extern uint64_t rtc_time;

namespace mint {
	tm::tm(uint64_t ts) {
		tm_milli = ts % 1000;
		ts /= 1000;

		// time in day
		int tid = ts % (24*60*60);
		tm_sec = tid % 60;
		tid /= 60;
		tm_min = tid % 60;
		tm_hour = tid / 60;
		// https://howardhinnant.github.io/date_algorithms.html#civil_from_days
		int tbd = ts/(24*60*60);
		int z   = tbd + 719468;
		tm_wday = (tbd + 4) % 7; // weekday calendar does not get borked by leap years; 1st day of 1970 was a thursday
		int era = z / 146097; // days in era (accounting for leap years)
		unsigned doe = z % 146097; // day in era
		unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365; // year of era (account for leap with magic correction factors)
		int y = int(yoe) + era*400; // era is 400 years exactly
		unsigned doy = doe - (365*yoe + yoe/4 - yoe/100); // account for leap years in day of year (365 days to a year + 1 every 4, minus 1 every 400)
		unsigned mp = (5*doy + 2)/153; 
		tm_day = doy - (153*mp+2)/5 + 1; // bs linear relation to get month offsets from days, picked by brute force search
		tm_mon = mp < 10 ? mp+3 : mp-9;
		tm_year = y + (tm_mon <= 2);
	}

	const char * tm::abbrev_month() const {
		static const char * const month_names =
			"Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec";
		return month_names + (tm_mon - 1)*4;
	}

	const char * tm::abbrev_weekday() const {
		static const char * const wday_names =
			"Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";
		return wday_names + tm_wday * 4;
	}

	tm now() {
		return tm{rtc_time};
	}
}
