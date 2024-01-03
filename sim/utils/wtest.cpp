#include <stdio.h>
#include <weathersummarizer.h>
#include <json.h>

int main() {
	weather::PrecipitationSummarizer minute, hour;
	hour.is_hourly = true;
	minute.is_hourly= false;
	weather::SingleDatapoint datapoint;

	int in_which_block = -1;

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
			if (in_which_block == 0) {
				minute.append(stack[3]->index, datapoint);
			}
			else if (in_which_block == 1 && stack[3]->index < 36)
				hour.append(stack[3]->index, datapoint);
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

	char buf[512]{};
	auto code = generate_summary(minute, hour, buf, sizeof buf);

	switch (code) {
		case weather::SummaryResult::Empty:
			printf("no summary\n");
			return 1;
		case weather::SummaryResult::TotalSummary:
			printf("total summary: %s\n", buf);
			break;
		case weather::SummaryResult::PartialSummary:
			printf("partial summary: %s\n", buf);
			break;
	}

	return 0;
}
