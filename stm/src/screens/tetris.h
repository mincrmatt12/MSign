#ifndef TETRIS_H
#define TETRIS_H

#include "base.h"
#include <stdint.h>
#include <new>

namespace screen::game {
	struct TetrisPiece {
		struct Map {
			uint8_t data[4];

			bool at(int x, int y) const {
				return data[y] & (0x8 >> x);
			}

			int min_height() const {
				int pos = 0;
				while (!data[-pos]) --pos;
				return pos;
			}
		};

		enum Color : uint8_t {
			Empty = 0,
			OutOfBounds = 1,
			I = 0x10,
			J,
			L,
			O,
			S,
			Z,
			T
		};

		Color color;
		int8_t origin_x, origin_y;
		Map maps[4]; // 4 rotations by 4 rows (max)
	};

	struct ActiveTetrisPiece {
		uint8_t id;
		int8_t x, y;
		uint8_t rotation = 0;

		ActiveTetrisPiece() : id(0xff) {};

		ActiveTetrisPiece(uint8_t id) : id(id) {
			spawn();
		}

		ActiveTetrisPiece(uint8_t id, int x, int y, int rotation) : id(id) {
			this->x = static_cast<int8_t>(x);
			this->y = static_cast<int8_t>(y);
			this->rotation = static_cast<uint8_t>(rotation);
		}

		const TetrisPiece::Map& map() const {return blueprint().maps[rotation];};
		const TetrisPiece& blueprint() const;
		void spawn() {
			x = blueprint().origin_x;
			y = blueprint().origin_y;
			rotation = 0;
		}
	};

	struct TetrisBoard {
		TetrisPiece::Color board[20][10]{};

		TetrisPiece::Color at(int x, int y);
		void put(int x, int y, TetrisPiece::Color col);
		inline bool occupied(int x, int y) { return at(x, y) != TetrisPiece::Empty; }

		bool collide(const ActiveTetrisPiece& piece);
		void put(const ActiveTetrisPiece& piece);

		bool empty();
	};

	struct TetrisNotification {
		int32_t score_amt;
		TetrisPiece::Color color;
		bool alive = false;
		uint32_t birthtime;

		TetrisNotification() {};
		TetrisNotification(int32_t score_diff, TetrisPiece::Color color);

		void draw(int16_t y);

		operator bool() const {return alive;}
	private:
		uint32_t lifetime();
	};

	struct TetrisBag {
		uint8_t next();
		
		void reset();
	private:
		uint8_t bag[7] {};
		uint8_t pos = 7;
	};

	struct Tetris : public Screen {
		Tetris() {restart();}

		void draw();

		void pause();
		void unpause();
		void restart();
	private:
		void tick();
		void handle_buttons();

		void place_piece();
		void pull_next_piece();
		void start_piece();
		void clear_lines();

		void draw_board();
		void draw_block(int16_t x, int16_t y, TetrisPiece::Color color, bool phantom=false);
		void draw_piece(const ActiveTetrisPiece& atp, bool as_phantom=false); // considers x/y as screen coordinates
		void draw_score();
		void draw_notifs();
		void draw_boxed_piece(const ActiveTetrisPiece& atp, const char* text);

		void draw_gameover();

		int get_unused_notif();
		template<typename ...Args>
		void start_notif(Args&& ...args) {
			new (&notifs[get_unused_notif()]) TetrisNotification(std::forward<Args>(args)...);
		}

		int tick_accum = 0;

		enum State {
			StatePlaying,
			StatePaused,
			StateGameEnd
		} state = StatePlaying;

		struct LevelInfo {
			uint32_t min_lines;
			uint8_t gravity;
			int8_t  score_mult;
		};

		const LevelInfo& current_level();

		uint32_t current_score = 0, current_lines = 0;
		uint32_t disp_score    = 0;

		int ticks_till_drop = 0;

		int das_timer = 0;

		uint32_t drop_timeout = 0;

		int combo = 0, combo_similar = 0;

		uint8_t combo_n = 0;

		TetrisBoard board;
		TetrisBag   bag;
		ActiveTetrisPiece current, hold, next;
		TetrisNotification notifs[4];

		bool hold_present = false, hold_ready = true;
	};
}

#endif
