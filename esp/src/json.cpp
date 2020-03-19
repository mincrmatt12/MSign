#include "json.h"
#include <string.h>

json::JSONParser::JSONParser(JSONCallback && c) : cb(std::move(c)) {
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
	ptrdiff_t head = 0; // hack to get around c++14 only
	return parse([&, head]() mutable {
		if (head >= size) return (char)0;
		return text[head++];
	});
}

bool json::JSONParser::parse(TextCallback && cb) {
	this->tcb = std::move(cb);

	while (this->stack_ptr > 1) pop(); // just in case
	if (this->stack_ptr < 1) push(); // root node

	return parse_value();
}

bool json::JSONParser::parse_value() {
	// PARSE VALUE: skips whitespace, then calls the correct function to parse a value. Assumes the context on the stack is set properly.
	
	if (!advance_whitespace()) return false;

	// check the current value at head
	switch (peek()) {
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

	if (peek() == '-') {
		next();
		negative = true;
		if (!advance_whitespace()) return false;
	}

	if (peek() == '0') {
		next();
		if (!advance_whitespace()) return false;
	}
	else {
		if (peek() > '9' || peek() < '1') return false;
		while (peek() != 0) {
			if (peek() >= '0' && peek() <= '9') {
				integer *= 10;
				integer += peek() - '0';
			}
			else {
				goto ok;
			}
			next();
		}
		return false;
	}
ok:
	// Check if we have to do a fractional part
	if (!advance_whitespace()) return false;
	if (peek() == '.') {
		whole = integer;
		isint = false;
		next();
		if (!advance_whitespace()) return false;
		while (peek() != 0) {
			if (peek() >= '0' && peek() <= '9') {
				whole *= 10;
				whole += peek() - '0';
			}
			else {
				goto ok2;
			}
	
			next();
			fractional_offset += 1;
		}
		return false;
	ok2:
		for (int i = 0; i < fractional_offset; ++i) whole /= 10;
	}
	if (!advance_whitespace()) return false;
	if (peek() == 'e' || peek() == 'E') {
		// parse an E string
		if (isint) {
			isint = false;
			whole = integer;
		}
		int e_str = 0;
		bool div = false;
		next();
		if (peek() == '-') div = true;
		next();
		while (peek() != 0) {
			if (peek() >= '0' && peek() <= '9') {
				e_str *= 10;
				e_str += peek() - '0';
			}
			else {
				goto ok3;
			}
			next();
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
	while (peek() == ' ' ||
		   peek() == '\t' ||
		   peek() == '\n') {
		if (next() == 0) return false;
	}
	return true;
}

char * json::JSONParser::parse_string_text() {
	if (peek() != '"') return nullptr;
	size_t bufsize = 16;
	size_t idx = 0;
	char * tbuf = (char *)malloc(16);

	auto append = [&tbuf, &bufsize, &idx](char c) {
		if (bufsize > 512) return; // Max length
		if (idx < bufsize) {
add:
			tbuf[idx++] = c;
		}
		else {
			bufsize += (bufsize / 2);
			tbuf = (char *)realloc(tbuf, bufsize);
			goto add;
		}
	};

	next();
	while (peek() != '"') {
		if (peek() == 0) return free(tbuf), nullptr;

		if (peek() != '\\') {
			append(peek());
			next();
		}
		else {
			switch (next()) {
				case 'b':
					append('\b');
					break;
				case 'f':
					append('\f');
					break;
				case 'n':
					append('\n');
					break;
				case 'r':
					append('\r');
					break;
				case 't':
					append('\t');
					break;
				case 'u':
					// the sign has no support for unicode anyways, so we just ignore it
					next(); next(); next();
					break;
				default:
					append(peek());
					break;
			}
			next();
		}
	}
	next();
	
	tbuf = (char*)realloc(tbuf, idx + 1);
	tbuf[idx] = 0;
	return tbuf;
}

bool json::JSONParser::parse_array() {
	bool anon_array_required = top().array;
	if (!anon_array_required) {
		top().array = true;
		top().index = 0;
	}
	else {
		push();
		top().array = true;
		top().index = 0;
	}

	while (peek() != 0) {
		next();
		parse_value();
		if (!advance_whitespace()) return false;
		if (peek() == ',') ++top().index;
		else if (peek() == ']') break;
		else return false;
	}
	 
	next();

	if (anon_array_required) {
		pop();
	}

	return true;
}

bool json::JSONParser::parse_object() {
	while (peek() != 0) {
		next();
		if (!advance_whitespace()) return false;
		char * n = parse_string_text();
		if (n == nullptr) return false;
		push(n);
		if (!advance_whitespace()) {free(n); return false;}
		if (peek() != ':') {free(n); return false;}
		next();
		parse_value();
		pop();
		free(n);
		if (!advance_whitespace()) {free(n); return false;}
		if (peek() == ',') continue;
		else if (peek() == '}') break;
		else return false;
	}

	next();
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
	switch (peek()) {
		case 't':
			cb(stack, stack_ptr, Value{true});
			next(); next(); next(); next();
			break;
		case 'f':
			cb(stack, stack_ptr, Value{false});
			next(); next(); next(); next(); next();
			break;
		case 'n':
			cb(stack, stack_ptr, Value{});
			next(); next(); next(); next();
			break;
		default:
			return false;
	}
	return true;
}

char json::JSONParser::peek() {
	if (this->need) {
		temp = tcb();
		this->need = false;
	}

	return temp;
}

char json::JSONParser::next() {
	this->need = false;
	temp = tcb();
	return temp;
}
