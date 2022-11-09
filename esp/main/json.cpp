#include "json.h"
#include "utf.h"
#include <string.h>

#ifndef STANDALONE_JSON
#include <esp_log.h>
#endif

const static char * const TAG = "json";

// Declared separately to provide a single address for these (they're compared by address not strcmp for obvious reasons)
const char * const json::PathNode::ROOT_NAME = "(root)";
const char * const json::PathNode::ANON_NAME = "(anon)";

json::TreeSlabAllocator::TreeSlabAllocator() {
	tail = new Chunk();
	_last = tail->data;
	_start = tail->data;
}

json::TreeSlabAllocator::~TreeSlabAllocator() {
	if (tail) delete tail;
}

bool json::TreeSlabAllocator::push(uint8_t c) {
	return append(&c, 1);
}

uint8_t * json::TreeSlabAllocator::end() {
	// If _last is not 4-byte, aligned, make it aligned.
	if ((uintptr_t)_last % 4) {
		if (!append(nullptr, 4 - ((uintptr_t)_last % 4))) {
#ifndef STANDALONE_JSON
			ESP_LOGW(TAG, "failed to align");
#endif
			return nullptr;
		}
	}
	return std::exchange(_start, _last);
}

void json::TreeSlabAllocator::finish(void * obj) {
	// obj is new start
	if (obj > _start) {
#ifndef STANDALONE_JSON
		ESP_LOGW(TAG, "invalid obj in memory.free");
#endif
	}

	_start = (uint8_t *)obj;

	// Subtract size from current chunk
	tail->used -= curlen();
	_last = _start;

	// If the current used is zero and there's a previous block, free into it
	if (tail->previous && !tail->used) {
		Chunk * new_end = std::exchange(tail->previous, nullptr);
		delete tail;
		tail = new_end;

		_last = _start = tail->data + tail->used;
	}
}

bool json::TreeSlabAllocator::append(const uint8_t * data, size_t amount) {
	// Is there enough space?
	if (tail->used + amount > tail->length) {
		// Is this the first thing in the chunk? If so, we can just expand the chunk.
		if (offset() == 0) {
			while (tail->used + amount > tail->length)
				if (!tail->expand(tail->length + TreeBlock)) return false;
			_start = tail->data;
			_last = tail->data + tail->used;
		}
		// Otherwise, we have to allocate a new chunk.
		else {
			Chunk * new_ = new Chunk{tail};
			// Figure out how much is already used and copy that into the new chunk.
			while (curlen()+amount > new_->length) 
				if (!new_->expand(new_->length + TreeBlock)) return false;
			memcpy(new_->data, _start, curlen());
			// Free up space in the old chunk
			tail->used -= curlen();
			// Update start and tail pointers
			new_->used = curlen();
			_start = new_->data;
			_last = new_->data + new_->used;
			tail = new_;
		}
	}
	if (data) memcpy(_last, data, amount);
	tail->used += amount;
	_last += amount;
	return true;
}

json::JSONParser::JSONParser(JSONCallback && c, bool is_utf8) : is_utf8(is_utf8), cb(std::move(c)) {
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
	return parse([&, head]() mutable -> int16_t {
		if (head >= size) return -1;
		return text[head++];
	});
}

bool json::JSONParser::parse(TextCallback && cb) {
	this->tcb = std::move(cb);

	while (this->stack_ptr > 1) pop(); // just in case
	if (this->stack_ptr < 1) push(); // root node

	return parse_value();
}

#ifndef STANDALONE_JSON
bool json::JSONParser::parse(dwhttp::Download & d) {
	return parse([&]() -> int16_t {
		auto v = d();
		return v;
	});
}
#endif

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
		if (next() == 0) {
			return false;
		}
	}
	return true;
}

char * json::JSONParser::parse_string_text() {
	if (peek() != '"') return nullptr;

	next();
	while (peek() != '"') {
		if (peek() == 0) return memory.finish(memory.end()), nullptr;

		if (peek() != '\\') {
			memory.push(peek());
			next();
		}
		else {
			switch (next()) {
				case 'b':
					memory.push('\b');
					break;
				case 'f':
					memory.push('\f');
					break;
				case 'n':
					memory.push('\n');
					break;
				case 'r':
					memory.push('\r');
					break;
				case 't':
					memory.push('\t');
					break;
				case 'u':
					// the sign has no support for unicode anyways, so we just ignore it
					next(); next(); next();
					break;
				default:
					memory.push(peek());
					break;
			}
			next();
		}
	}
	next();

	memory.push(0);
	
	return (char *)memory.end();
}

bool json::JSONParser::parse_array() {
	bool anon_array_required = top().array;
	if (!anon_array_required) {
		top().array = true;
		top().index = 0;
	}
	else {
		push(true);
		top().array = true;
		top().index = 0;
	}

	while (peek() != 0) {
		next();
		if (!advance_whitespace()) return false;
		if (peek() == ']') break;
		if (!parse_value()) return false;
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
		if (peek() == '}') {
			break;
		}
		char * n = parse_string_text();
		if (n == nullptr) return false;
		push(n);
		if (!advance_whitespace()) {free(n); return false;}
		if (peek() != ':') {free(n); return false;}
		next();
		parse_value();
		pop();
		memory.finish(n);
		if (!advance_whitespace()) {return false;}
		if (peek() == ',') continue;
		else if (peek() == '}') break;
		else return false;
	}

	if (stack_ptr > 1) next();
	cb(stack, stack_ptr, Value{Value::OBJ});
	return true;
}

bool json::JSONParser::parse_string() {
	char * b = parse_string_text();
	if (b == nullptr) return false;
	else {
		if (is_utf8) utf8::process(b);
		Value v{b};
		cb(stack, stack_ptr, v);
		memory.finish(b);
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
		auto x = tcb();
		temp = x < 0 ? 0 : x;
		this->need = false;
	}

	return temp;
}

char json::JSONParser::next() {
	this->need = false;
	auto x = tcb();
	temp = x < 0 ? 0 : x;
	return temp;
}
