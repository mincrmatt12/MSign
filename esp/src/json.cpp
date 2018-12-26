#include "json.h"
#include "string.h"

json::JSONParser::JSONParser(JSONCallback && c) : cb(c) {
	this->stack_ptr = 0;
	push();
}

json::JSONParser::~JSONParser() {
	while (this->stack_ptr > 0) pop();
}

bool json::JSONParser::parse(const char *text) {
	return parse(text, strlen(text));
}

bool json::JSONParser::parse(const char *text, size_t size) {
	this->text = text;
	this->text_size = size;
	this->head = 0;
	while (this->stack_ptr > 1) pop(); // just in case
	if (this->stack_ptr < 1) push(); // root node

	return parse_value();
}

bool json::JSONParser::parse_value() {
	// PARSE VALUE: skips whitespace, then calls the correct function to parse a value. Assumes the context on the stack is set properly.
	
	if (!advance_whitespace()) return false;

	// check the current value at head
	switch (text[head]) {
		case '{':
			return parse_object();
		case '-':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			return parse_number();
		case '[':
			return parse_array();
		case '"':
			return parse_string();
		case 't':
		case 'f':
		case 'n':
			return parse_singleton();
		default:
			return false;
	}
}

bool json::JSONParser::parse_number() {
	bool negative = false;
	int fractional_offset = 0;
	int64_t integer = 0;
	float whole = 0.0;
	bool isint = true;

	if (text[head] == '-') {
		++head;
		negative = true;
		if (!advance_whitespace()) return false;
	}

	if (text[head] == '0') {
		++head;
		if (!advance_whitespace()) return false;
	}
	else {
		if (text[head] > '9' || text[head] < '1') return false;
		while (head < text_size) {
			switch (text[head]) {
				case '0':
					integer *= 10;
					break;
				case '1':
					integer *= 10;
					integer += 1;
					break;
				case '2':
					integer *= 10;
					integer += 2;
					break;
				case '3':
					integer *= 10;
					integer += 3;
					break;
				case '4':
					integer *= 10;
					integer += 4;
					break;
				case '5':
					integer *= 10;
					integer += 5;
					break;
				case '6':
					integer *= 10;
					integer += 6;
					break;
				case '7':
					integer *= 10;
					integer += 7;
					break;
				case '8':
					integer *= 10;
					integer += 8;
					break;
				case '9':
					integer *= 10;
					integer += 9;
					break;
				default:
					goto ok;
			}
			++head;
		}
		return false;
	}
ok:
	// Check if we have to do a fractional part
	if (!advance_whitespace()) return false;
	if (text[head] == '.') {
		whole = integer;
		isint = false;
		++head;
		if (!advance_whitespace()) return false;
		while (head < text_size) {
			switch (text[head]) {
				case '0':
					whole *= 10;
					break;
				case '1':
					whole *= 10;
					whole += 1;
					break;
				case '2':
					whole *= 10;
					whole += 2;
					break;
				case '3':
					whole *= 10;
					whole += 3;
					break;
				case '4':
					whole *= 10;
					whole += 4;
					break;
				case '5':
					whole *= 10;
					whole += 5;
					break;
				case '6':
					whole *= 10;
					whole += 6;
					break;
				case '7':
					whole *= 10;
					whole += 7;
					break;
				case '8':
					whole *= 10;
					whole += 8;
					break;
				case '9':
					whole *= 10;
					whole += 9;
					break;
				default:
					goto ok2;
			}
			++head;
			fractional_offset += 1;
		}
		return false;
	ok2:
		for (int i = 0; i < fractional_offset; ++i) whole /= 10;
	}
	if (!advance_whitespace()) return false;
	if (text[head] == 'e' || text[head] == 'E') {
		// parse an E string
		if (isint) {
			isint = false;
			whole = integer;
		}
		int e_str = 0;
		bool div = false;
		++head;
		if (text[head] == '-') div = true;
		++head;
		while (head < text_size) {
			switch (text[head]) {
				case '0':
					e_str *= 10;
					break;
				case '1':
					e_str *= 10;
					e_str += 1;
					break;
				case '2':
					e_str *= 10;
					e_str += 2;
					break;
				case '3':
					e_str *= 10;
					e_str += 3;
					break;
				case '4':
					e_str *= 10;
					e_str += 4;
					break;
				case '5':
					e_str *= 10;
					e_str += 5;
					break;
				case '6':
					e_str *= 10;
					e_str += 6;
					break;
				case '7':
					e_str *= 10;
					e_str += 7;
					break;
				case '8':
					e_str *= 10;
					e_str += 8;
					break;
				case '9':
					e_str *= 10;
					e_str += 9;
					break;
				default:
					goto ok3;
			}
			++head;
		}
		return false;
	ok3:
		if (div) for (int i = 0; i < e_str; ++i) whole /= 10;
		else     for (int i = 0; i < e_str; ++i) whole *= 10;
	}

	if (isint) {
		Value v{negative ? -integer : integer};
		cb(stack, stack_ptr, v);
	}
	else {
		Value v{negative ? -whole : whole};
		cb(stack, stack_ptr, v);
	}

	return true;
}

bool json::JSONParser::advance_whitespace() {
	while (text[head] == ' ' ||
		   text[head] == '\t' ||
		   text[head] == '\n') {
		if (++head == text_size) return false;
	}
	return true;
}

char * json::JSONParser::parse_string_text() {
	if (text[head] != '"') return nullptr;
	++head;
	ptrdiff_t start = head;
	ptrdiff_t length = 0;
	while (text[head] != '"') {
		if (text[head] != '\\') ++head, ++length;
		else {
			++head;
			if (text[head] == 'u') {
				head += 5;
			}
			else {
				head += 1;
			}
			++length;
		}
	}

	char * buf = (char *)malloc(length+1);
	buf[length] = 0;
	for (ptrdiff_t lhead = start, i = 0; lhead < head; ++lhead, ++i) {
		if (text[lhead] != '\\') buf[i] = text[lhead];
		else {
			++lhead;
			switch (text[lhead]) {
				case '"': buf[i] = '"'; break;
				case '\\': buf[i] = '\\'; break;
				case 'b': buf[i] = '\b'; break;
				case 'f': buf[i] = '\f'; break;
				case 'n': buf[i] = '\n'; break;
				case 'r': buf[i] = '\r'; break;
				case 't': buf[i] = '\t'; break;
				case 'u':
					lhead += 3; // lazy
				default:
					buf[i] = text[lhead];
			}
		}
	}
	++head;
	return buf;
}

bool json::JSONParser::parse_array() {
	top().array = true;
	top().index = 0;

	while (head < text_size) {
		++head;
		parse_value();
		if (!advance_whitespace()) return false;
		if (text[head] == ',') ++top().index;
		else if (text[head] == ']') break;
		else return false;
	}
	 
	++head;
	return true;
}

bool json::JSONParser::parse_object() {
	while (head < text_size) {
		++head;
		char * n = parse_string_text();
		if (n == nullptr) return false;
		push(n);
		if (!advance_whitespace()) return false;
		if (text[head] != ':') return false;
		++head;
		parse_value();
		pop();
		if (!advance_whitespace()) return false;
		if (text[head] == ',') continue;
		else if (text[head] == '}') break;
		else return false;
	}

	++head;
	cb(stack, stack_ptr, Value{Value::OBJ});
	return true;
}

bool json::JSONParser::parse_string() {
	char * b = parse_string_text();
	if (b == nullptr) return false;
	else {
		Value v{b};
		cb(stack, stack_ptr, v);
		free(b);
	}
	return true;
}

bool json::JSONParser::parse_singleton() {
	switch (text[head]) {
		case 't':
			cb(stack, stack_ptr, Value{true});
			head += 4;
			break;
		case 'f':
			cb(stack, stack_ptr, Value{false});
			head += 5;
			break;
		case 'n':
			cb(stack, stack_ptr, Value{});
			head += 4;
			break;
		default:
			return false;
	}
	return true;
}
