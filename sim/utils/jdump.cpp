#include "json.h"
#include <iostream>

void show_stack(json::PathNode &pn) {
	std::cout << pn.name;
	if (pn.is_root()) return;
	if (pn.is_array()) {
		std::cout << "[" << (int)pn.index << "]";
	}
}

void callback(json::PathNode ** stack, uint8_t sptr, const json::Value& value) {
	for (int i = 0; i < sptr; ++i) {
		if (i) {
			std::cout << "<-";
		}
		show_stack(*stack[i]);
	}
	std::cout << " = ";

	switch (value.type) {
		case json::Value::BOOL:
			if (value.bool_val) std::cout << "true";
			else                std::cout << "false";
			break;
		case json::Value::FLOAT:
			std::cout << value.float_val << "f";
			break;
		case json::Value::INT:
			std::cout << value.int_val;
			break;
		case json::Value::NONE:
			std::cout << "null";
			break;
		case json::Value::OBJ:
			std::cout << "{}";
			break;
		case json::Value::STR:
			std::cout << '"' << value.str_val << '"';
		default:
			break;
	}

	std::cout << "\n";
}

int main(int argc, char ** argv) {
	json::JSONParser p(callback, argc > 1);
	int code = p.parse([]() -> int16_t {
		int x = getchar();
		if (x == EOF) {
			std::cerr << "\njdump: got eof during parse.\n";
			return -1;
		}
		return x;
	});
	return !code;
}
