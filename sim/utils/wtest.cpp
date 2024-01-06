#include <stdio.h>
#include <weathersummarizer.h>
#include <json.h>

int main(int argc, char ** argv) {
	weather::PrecipitationSummarizer minute, hour;
	weather::HourlyConditionSummarizer hour_total;
	hour.is_hourly = true;
	minute.is_hourly= false;
	weather::SingleDatapoint datapoint;
	slots::WeatherStateCode wsc;

	int in_which_block = -1;

	bool debug = argc > 2 && !strcmp(argv[1], "--debug");

	json::JSONParser jp([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& val) {
		if (stack_ptr < 3) return;
		if (stack_ptr == 4 && !strcmp(stack[1]->name, "data") && !strcmp(stack[2]->name, "timelines") && !strcmp(stack[3]->name, "timestep") && val.type == val.STR) {
			in_which_block = -1;
			if (!strcmp(val.str_val, "1h")) in_which_block = 1;
			else if (!strcmp(val.str_val, "5m")) in_which_block = 0;
		}
		else if (stack_ptr == 6 && !strcmp(stack[4]->name, "values")) {
			datapoint.load_from_json(stack[5]->name, val);
		}
		else if (stack_ptr == 5 && !strcmp(stack[4]->name, "values") && val.type == val.OBJ) {
			if (in_which_block == 0 && stack[3]->index < 144) {
				minute.append(stack[3]->index, datapoint);
			}
			else if (in_which_block == 1 && stack[3]->index < 36) {
				if (stack[3]->index == 0)
					wsc = datapoint.weather_code;
				hour.append(stack[3]->index, datapoint);
				hour_total.append(stack[3]->index, datapoint);
			}
			datapoint = {};
		}
	}, true);

	{
		int code = jp.parse([]() -> int16_t {
			int x = getchar();
			if (x == EOF) {
				fprintf(stderr, "\njdump: got eof during parse.\n");
				return -1;
			}
			return x;
		});

		if (!code)
			return 2;
	}

	char buf[512]{}, buf2[512]{};
	auto code = generate_summary(minute, hour, buf, sizeof buf);
	hour_total.generate_summary(wsc, code, buf2, sizeof buf2);

	int mode = 0; // 0 print both, 1 print hour first both, 2 print hour, 3 print first

	switch (code) {
		case weather::SummaryResult::Empty:
			if (debug) {
				printf("no precip summary\n");
				printf("hour summary: %s\n", buf2);
			}

			mode = 2;
			break;
		case weather::SummaryResult::TotalSummary:
			if (debug) printf("total summary: %s\n", buf);
			mode = 3;
			if (hour_total.has_important_message(wsc)) {
				if (debug) {
					printf("hour summary forced first\n");
					printf("hour summary: %s\n", buf2);
				}
				mode = 1;
			}
			break;
		case weather::SummaryResult::PartialSummary:
			if (debug) printf("partial summary: %s\n", buf);
			mode = 3;
			if (!hour_total.should_ignore_hourly_summary(wsc)) {
				if (debug) printf("hour summary: %s\n", buf2);
				mode = 0;
				if (hour_total.has_important_message(wsc)) {
					if (debug) printf("hour summary goes first\n");
					mode = 1;
				}
			}
			break;
	}

	if (debug) puts("");

	switch (mode) {
	case 0:
		printf("real summary: %s %s\n", buf, buf2); return 0;
	case 1:
		printf("real summary: %s %s\n", buf2, buf); return 0;
	case 2:
		printf("real summary: %s\n", buf2); return 0;
	case 3:
		printf("real summary: %s\n", buf); return 0;
	}

	return 0;
}
