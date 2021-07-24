#ifndef MSN_SCREEN_BASE_H
#define MSN_SCREEN_BASE_H

#include <utility>
#include <tuple>
#include <stdint.h>
#include <stddef.h>
#include "../ui.h"

namespace screen {

	// Override these (and the destructor/constructor); not virtual because we use a tagged union
	struct Screen {
		// Set necessary slots to warm here. Note that only a minimum of slots should be enabled here.
		//
		// This is called twice, once when it's the next screen to appear and once a second before we
		// initialize the screen (this allows you to do stuff like pre-load data conditionally)
		static void prepare(bool which_run /* true if first call */) {};

		// Draw to the screen
		void draw() {};

		// Does this screen require clearing?
		constexpr static inline bool require_clearing() { return true; }

		// Handle button interactions. Only called if interaction is occuring; return value
		// is whether or not to exit interaction. Typically this is bound to the top of the "back" tree -- usually from the power button.
		//
		// Note that calls to interact() can end whenever due to both inactivity timeouts and the catch all exit shortcut (hold power for 1.5 seconds)
		bool interact() {return ui::buttons[ui::Buttons::POWER];};
	};

	// A screen which layers multiple other screens
	template<typename ...Layered>
	struct LayeredScreen : Screen {
		static void prepare(bool which_run) {
			(Layered::prepare(which_run), ...);
		}

		void draw() {
			(std::get<Layered>(screens).draw(), ...);
		}

		constexpr static inline bool require_clearing() {
			return require_clearing_impl<Layered...>();
		}

	private:
		std::tuple<Layered...> screens;

		template<typename Bottom, typename ...Args>
		constexpr static inline bool require_clearing_impl() {
			return Bottom::require_clearing();
		}
	};

	// Screen IDs are order in template parameters.
	//
	// Starts with showing no screen at all (state can be induced with shutoff)
	template<typename ...Screens>
	class ScreenSwapper {
		typename std::aligned_union<0, Screens...>::type storage;
		constexpr inline static size_t npos = ~0u;

		size_t selected = npos;

		template<size_t ...Idx>
		inline void _destruct(size_t idx, std::index_sequence<Idx...>) {
			(void)((idx == Idx && (reinterpret_cast<Screens *>(&storage)->~Screens(), true)) || ...);
		}

		template<size_t ...Idx>
		inline void _construct(size_t idx, std::index_sequence<Idx...>) {
			(void)((idx == Idx && (new (&storage) Screens{}, true)) || ...);
		}

		template<size_t ...Idx>
		inline bool _require_clearing(size_t idx, std::index_sequence<Idx...>) {
			return ((idx == Idx ? Screens::require_clearing() : false) || ...);
		}

		template<size_t ...Idx>
		inline void _notify(size_t idx, bool param, std::index_sequence<Idx...>) {
			(void)((idx == Idx && (Screens::prepare(param), true)) || ...);
		}

		template<size_t ...Idx>
		inline void _draw(size_t idx, std::index_sequence<Idx...>) {
			(void)((idx == Idx && (reinterpret_cast<Screens *>(&storage)->draw(), true)) || ...);
		}

		template<size_t ...Idx>
		inline bool _interact(size_t idx, std::index_sequence<Idx...>) {
			return ((idx == Idx ? reinterpret_cast<Screens *>(&storage)->interact() : false) || ...);
		}

	public:
		void shutoff() { // disable screenswapper and have draw return nothing
			if (selected != npos) {
				_destruct(selected, std::index_sequence_for<Screens...>{});
			}
			selected = npos;
		}

		void draw() {
			if (selected != npos)
				_draw(selected, std::index_sequence_for<Screens...>{});
		}

		bool interact() {
			if (selected != npos)
				return _interact(selected, std::index_sequence_for<Screens...>{});
			return true;
		}

		void transition(size_t which) {
			if (selected != npos) {
				_destruct(selected, std::index_sequence_for<Screens...>{});
			}
			selected = which;
			_construct(which,  std::index_sequence_for<Screens...>{});
		}

		void notify_before_transition(size_t id, bool which_run) {
			_notify(id, which_run, std::index_sequence_for<Screens...>{});
		}

		bool require_clearing() {
			return _require_clearing(selected, std::index_sequence_for<Screens...>{});
		}
	};
}

#endif
