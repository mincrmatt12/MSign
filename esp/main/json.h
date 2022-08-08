#ifndef JSON_H
#define JSON_H

#include <cstdlib>
#include <cstdint>
#include <functional>
#include "dwhttp.h"

namespace json {
	struct PathNode {
		const char * name = nullptr;
		uint16_t index = 0;
		bool array = false;

		// constructs root
		PathNode() : name(ROOT_NAME) {}
		explicit PathNode(bool array) : name(ANON_NAME), array(array) {}
		explicit PathNode(const char * name_in) : name(name_in), index(0), array(false) {}
		PathNode(const char * name_in, uint16_t index) : name(name_in), index(index), array(true) {}

		inline bool is_array() const {return array && !is_root();}
		inline bool is_root() const {return name == ROOT_NAME;}
		inline bool is_anon() const {return name == ANON_NAME;}
		inline bool is_obj() const {return !array && !is_root();}
	
	private:
		static const char * const ROOT_NAME;
		static const char * const ANON_NAME;
	};

	struct Value {
		enum Type : uint8_t {
			STR,
			FLOAT,
			INT,
			BOOL,
			NONE,
			OBJ // technically a none, but called at the end of an object.
		} type;
		
		union {
			const char* str_val;
			float float_val;
			int64_t int_val;
			bool bool_val;
		};

		Value(Type t=NONE) : type(t) {}
		Value(float v) : type(FLOAT), float_val(v) {}
		Value(int64_t v) : type(INT), int_val(v) {}
		Value(bool b) : type(BOOL), bool_val(b) {}
		Value(const char * str) : type(STR), str_val(str) {}

		inline bool is_number() const {return type == FLOAT || type == INT;}
		inline float as_number() const {return type == FLOAT ? float_val : int_val;}
	};

	typedef std::function<void (PathNode **, uint8_t, const Value&)> JSONCallback;
	typedef std::function<int16_t (void)> TextCallback;

	constexpr inline static size_t TreeBlock = 128;

	struct TreeSlabAllocator {
		TreeSlabAllocator();
		~TreeSlabAllocator();

		// Allocate a single object
		template<typename T, typename ...Args>
		T* make(Args&& ...args) {
			if (!append(nullptr, sizeof(T))) return nullptr;
			return new (end()) T (std::forward<Args>(args)...);
		}

		// Push a character onto a variable length object
		bool push(uint8_t c); // Returns false if out of memory
		// Finish a variable length object
		uint8_t * end();

		// Free up the latest allocation
		void finish(void * obj);

	private:
		bool append(const uint8_t * data, size_t amount);

		size_t offset() {
			return _start - tail->data;
		}

		size_t curlen() {
			return _last - _start;
		}

		struct Chunk {
			Chunk(Chunk *onto=nullptr) {
				length = TreeBlock;
				used = 0;
				previous = onto;
				data = (uint8_t *)malloc(TreeBlock);
			}

			~Chunk() {
				if (data) free(data);
				if (previous) delete previous;
			}

			uint16_t length{};
			uint16_t used{};
			Chunk * previous{};
			uint8_t * data{};

			bool expand(size_t to) {
				length = to;
				data = (uint8_t *)realloc(data, to);
				return data;
			}
		} *tail;

		uint8_t *_last, *_start;
	};

	struct JSONParser {
		JSONParser(JSONCallback && c, bool is_utf8=false);
		~JSONParser();

		bool parse(const char * text);
		bool parse(const char * text, size_t size);
		bool parse(TextCallback && c);
#ifndef STANDALONE_JSON
		bool parse(dwhttp::Download & d);
#endif

	private:
		PathNode& top() {
			return *stack[stack_ptr - 1];
		}

		template<typename... Args>
		inline void push(Args&&... args) {
			stack[stack_ptr++] = memory.make<PathNode>(std::forward<Args>(args)...);
		}
		void pop () {
			memory.finish(stack[--stack_ptr]);
		}

		bool parse_value();

		bool parse_object();
		bool parse_array();

		bool parse_string();
		bool parse_number();
		bool parse_singleton();

		char * parse_string_text();

		bool advance_whitespace();

		char peek();
		char next();

		char temp = 0;
		uint8_t    stack_ptr;

		bool need = true;
		bool is_utf8 = false;

		PathNode * stack[20];

		JSONCallback cb;
		TextCallback tcb;

		TreeSlabAllocator memory;
	};
}

#endif
