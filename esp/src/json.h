#ifndef JSON_H
#define JSON_H

#include <cstdlib>
#include <cstdint>
#include <functional>

namespace json {
	struct PathNode {
		char * name = nullptr;
		uint16_t index;
		bool array;

		PathNode() {}
		PathNode(char * name_in) : name(name_in), index(0), array(false) {}
		PathNode(char * name_in, uint16_t index) : name(name_in), index(index), array(true) {}

		~PathNode() {free(name);}

		inline bool is_array() const {return array && !is_root();}
		inline bool is_root() const {return name == nullptr;}
		inline bool is_obj() const {return !array && !is_root();}
	};

	struct Value {
		enum Type : uint8_t {
			STR,
			FLOAT,
			INT,
			BOOL,
			NONE
		} type;
		
		union {
			const char* str_val;
			float float_val;
			int64_t int_val;
			bool bool_val;
		};

		Value() : type(NONE) {}
		Value(float v) : type(FLOAT), float_val(v) {}
		Value(int64_t v) : type(INT), int_val(v) {}
		Value(bool b) : type(BOOL), bool_val(b) {}
		Value(const char * str) : type(STR), str_val(str) {}
	};

	typedef std::function<void (PathNode **, uint8_t, const Value&)> JSONCallback;

	class JSONParser {
	public:
		JSONParser(JSONCallback && c);
		~JSONParser();

		bool parse(const char * text);
		bool parse(const char * text, size_t size);
	private:
		PathNode& top() {
			return *stack[stack_ptr - 1];
		}

		template<typename... Args>
		inline void push(Args&&... args) {
			stack[stack_ptr++] = new PathNode(std::forward<Args...>(args)...);
		}
		void pop () {
			delete stack[--stack_ptr];
		}

		bool parse_value();

		bool parse_object();
		bool parse_array();

		bool parse_string();
		bool parse_number();
		bool parse_singleton();

		char * parse_string_text();

		bool advance_whitespace();

		PathNode * stack[32];
		uint8_t    stack_ptr;

		const char * text;
		ptrdiff_t    head;
		size_t       text_size;

		JSONCallback cb;
	};
}

#endif
