#include "../draw.h"
#include "../fonts/latob_15.h"
#include "../fonts/lcdpixel_6.h"
#include "../fonts/tahoma_9.h"
#include "../common/bootcmd.h"
#include "../srv.h"
#include "../ui.h"
#include "../crash/main.h"
#include "tetris.h"
#include <algorithm>
#include <numeric>
#include "../rng.h"
#include "../tasks/timekeeper.h"

extern srv::Servicer servicer;
extern matrix_type matrix;
extern tasks::Timekeeper timekeeper;

namespace bitmap::tetris {
	// w=3,h=3,stride=1
	const uint8_t normal[] = {
		0b111'00000,
		0b101'00000,
		0b111'00000
	};

	// w=3,h=3,stride=1
	const uint8_t phantom[] = {
		0b000'00000,
		0b010'00000,
		0b000'00000
	};
}

namespace screen::game {
	const TetrisPiece pieces[] = {
        {
			TetrisPiece::Color::I,
            3, -2,
            {
                {
					0b0000,
                    0b1111,
					0b0000,
					0b0000,
                },
                {
                    0b0010,
                    0b0010,
                    0b0010,
                    0b0010,
                },
                {
					0b0000,
					0b0000,
                    0b1111,
					0b0000
                },
                {
                    0b0100,
                    0b0100,
                    0b0100,
                    0b0100,
                },
            }
        },
        {
			TetrisPiece::Color::J,
            3, -2,
            {
                {
                    0b0000,
                    0b1110,
                    0b0010,
                },
                {
                    0b0100,
                    0b0100,
                    0b1100,
                },
                {
                    0b1000,
                    0b1110,
                },
                {
                    0b0110,
                    0b0100,
                    0b0100,
                }
            }
        },
        {
			TetrisPiece::Color::L,
            3, -2,
            {
                {
                    0b0000,
                    0b1110,
                    0b1000,
                },
                {
                    0b1100,
                    0b0100,
                    0b0100,
                },
                {
                    0b0010,
                    0b1110,
                },
                {
                    0b0100,
                    0b0100,
                    0b0110,
                },
            }
        },
        {
			TetrisPiece::Color::O,
            4, -1,
            {
                {
                    0b1100,
                    0b1100,
                },
                {
                    0b1100,
                    0b1100,
                },
                {
                    0b1100,
                    0b1100,
                },
                {
                    0b1100,
                    0b1100,
                },
            }
        },
        {
			TetrisPiece::Color::S,
            3, -2,
            {
                {
                    0b0000,
                    0b0110,
                    0b1100,
                },
                {
                    0b0100,
                    0b0110,
                    0b0010,
                },
                {
                    0b0110,
                    0b1100,
                },
                {
                    0b1000,
                    0b1100,
                    0b0100,
                },
            }
        },
        {
			TetrisPiece::Color::Z,
            3, -2,
            {
                {
                    0b0000,
                    0b1100,
                    0b0110,
                },
                {
                    0b0010,
                    0b0110,
                    0b0100,
                },
                {
                    0b1100,
                    0b0110,
                },
                {
                    0b0100,
                    0b1100,
                    0b1000,
                },
            }
        },
        {
			TetrisPiece::Color::T,
            3, -2,
            {
                {
                    0b0000,
                    0b1110,
                    0b0100,
                },
                {
                    0b0100,
                    0b1100,
                    0b0100,
                },
                {
                    0b0100,
                    0b1110,
                    0b0000,
                },
                {
                    0b0100,
                    0b0110,
                    0b0100,
                }
            }
        },
	};

	const TetrisPiece& ActiveTetrisPiece::blueprint() const {
		return pieces[id];
	}

	void TetrisBag::reset() {
		std::iota(std::begin(bag), std::end(bag), 0);
		std::shuffle(std::begin(bag), std::end(bag), rng::randengine{});

		pos = 0;
	}

	uint8_t TetrisBag::next() {
		if (pos >= 7) reset();
		return bag[pos++];
	}

	bool TetrisBoard::collide(const ActiveTetrisPiece& piece) {
		for (int x = 0; x < 4; ++x) {
			for (int y = 0; y < 4; ++y) {
				if (piece.map().at(x, y) && occupied(piece.x + x, piece.y + y)) return true;
			}
		}
		return false;
	}

	void TetrisBoard::put(int x, int y, TetrisPiece::Color col) {
		msign_assert(at(x, y) != TetrisPiece::OutOfBounds, "out of bounds put");

		board[y][x] = col;
	}

	void TetrisBoard::put(const ActiveTetrisPiece& piece) {
		for (int x = 0; x < 4; ++x) {
			for (int y = 0; y < 4; ++y) {
				if (piece.map().at(x, y)) put(piece.x + x, piece.y + y, piece.blueprint().color);
			}
		}
	}

	TetrisPiece::Color TetrisBoard::at(int x, int y) {
		if (x < 0 || x >= 10 || y >= 20) return TetrisPiece::OutOfBounds;
		if (y < 0) return TetrisPiece::Empty;

		return board[y][x];
	}

	void Tetris::handle_buttons() {
		// Handle harddrop first

		if (ui::buttons[ui::Buttons::SEL] && timekeeper.current_time - drop_timeout > 80) {
			while (!board.collide(current)) ++current.y;
			--current.y;
			place_piece();
			drop_timeout = timekeeper.current_time;
			return;
		}

		// Move left/right
		ActiveTetrisPiece next_pos = current;
		
		if (ui::buttons[ui::Buttons::PRV]) {
			next_pos.x -= 1;
			das_timer = pdMS_TO_TICKS(250);
		}
		if (ui::buttons[ui::Buttons::NXT]) {
			next_pos.x += 1;
			das_timer = pdMS_TO_TICKS(250);
		}

		if (!board.collide(next_pos)) current = next_pos;

		if (ui::buttons[ui::Buttons::POWER] && hold_ready) {
			if (hold_present) {
				std::swap(hold, current);
			}
			else {
				hold_present = true;
				hold = current;
				pull_next_piece();
			}
			hold_ready = false;
			start_piece();
		}

		if (ui::buttons[ui::Buttons::MENU]) {
			ActiveTetrisPiece rot_pos = current;
			// rotate
			rot_pos.rotation++;
			if (rot_pos.rotation > 3) rot_pos.rotation = 0;

			if (board.collide(rot_pos)) {
				rot_pos.x--;
			}
			if (board.collide(rot_pos)) {
				rot_pos.x += 2;
			}
			if (board.collide(rot_pos)) {
				rot_pos.x--;
				rot_pos.y--;
			}
			if (!board.collide(rot_pos)) {
				current = rot_pos;
			}
		}
	}

	void Tetris::clear_lines(bool from_hard_drop) {
		const static int line_points[] = {
			0, 40, 100, 300, 1200, 1500
		};

		// write line and source line
		//
		// we iterate until all lines are updated, moving the source to avoid filled in lines
		int wptr = 19, sptr = 19;
		int lines = 0;

		while (wptr >= 0) {
			if (sptr < 0 || std::end(board.board[sptr]) != std::find(std::begin(board.board[sptr]), std::end(board.board[sptr]), TetrisPiece::Empty)) {
				if (sptr != wptr) {
					if (sptr < 0)
						std::fill(std::begin(board.board[wptr]), std::end(board.board[wptr]), TetrisPiece::Empty);
					else
						std::copy(std::begin(board.board[sptr]), std::end(board.board[sptr]), std::begin(board.board[wptr]));
				}
				--wptr;
			}
			else {
				++lines;
			}
			--sptr;
		}

		if (lines) {
			combo += 1;
			if (lines == combo_n || combo_n == 0) {
				combo_similar += 1;
			}
			else {
				combo_similar = 0;
			}
			combo_n = lines;
		}
		else {
			combo = 0;
			combo_n = 0;
			combo_similar = 0;
		}

		current_score += line_points[from_hard_drop && lines ? lines + 1 : lines] * current_level().score_mult;
		current_lines += lines;
	}

	void Tetris::place_piece() {
		if (current.y < current.map().min_height()) {
			state = StateGameEnd;
			return;
		}
		// Put piece into board
		board.put(current);
		// Check for line clears
		clear_lines();
		pull_next_piece();
		start_piece();
		// Reset hold
		hold_ready = true;
	}

	void Tetris::pull_next_piece() {
		current = next;
		next = bag.next();
	}

	const Tetris::LevelInfo& Tetris::current_level() {
		const static LevelInfo level_table[] = {
			// lines, speed
			{200, 5, 15},
			{150, 6, 13},
			{120, 7, 12},
			{100, 8, 11},
			{90, 10, 10},
			{80, 13, 9},
			{70, 16, 8},
			{60, 25, 7},
			{50, 28, 6},
			{40, 30, 5},
			{30, 33, 4},
			{20, 38, 3},
			{10, 43, 2},
			{0,  48, 1}
		};

		for (const auto& x : level_table) {
			if (current_lines >= x.min_lines) return x;
		}
		
		return level_table[0];
	}

	void Tetris::start_piece() {
		current.spawn();
		ticks_till_drop = current_level().gravity + 1;

		// todo: gameover
		if (board.collide(current)) {
			state = StateGameEnd;
		}
	}

	void Tetris::tick() {
		// Drop piece
		if (ticks_till_drop-- <= 0) {
			ActiveTetrisPiece next_pos = current;
			next_pos.y++;
			// Is piece still in air?
			if (board.collide(next_pos)) {
				// Next piece
				place_piece();
			}
			else {
				current = next_pos;
				ticks_till_drop = current_level().gravity;
			}
		}

		// Repeated shifts
		if (das_timer) {
			int offs = 0;
			if (ui::buttons.held(ui::Buttons::PRV, das_timer)) {
				offs = -1;
			}
			else if (ui::buttons.held(ui::Buttons::NXT, das_timer)) {
				offs = 1;
			}
			else if (!ui::buttons.held(ui::Buttons::PRV) && !ui::buttons.held(ui::Buttons::NXT)) {
				das_timer = 0;
			}

			if (offs != 0) {
				ActiveTetrisPiece next_pos = current;

				das_timer += pdMS_TO_TICKS(32);
				next_pos.x += offs;
				if (!board.collide(next_pos)) current = next_pos;
			}
		}
	}

	inline int board_start_x = 49, board_start_y = 3;

	ActiveTetrisPiece onscreen_piece(const ActiveTetrisPiece& atp) {
		auto result = atp;
		result.x = board_start_x + atp.x*3;
		result.y = board_start_y + atp.y*3;
		return result;
	}

	void Tetris::draw_board() {
		// Draw blocks
		for (int y = 0; y < 20; ++y) {
			for (int x = 0; x < 10; ++x) {
				draw_block(board_start_x + x*3, board_start_y + y * 3, board.at(x, y));
			}
		}

		// Draw current piece
		draw_piece(onscreen_piece(current));

		// Draw phantom 
		ActiveTetrisPiece phantom = current;
		while (!board.collide(phantom)) ++phantom.y;
		--phantom.y;
		draw_piece(onscreen_piece(phantom), true);
		
		// Draw border
		draw::rect(matrix.get_inactive_buffer(),    board_start_x-1, 0, board_start_x+30, board_start_y-1, 0);
		draw::outline(matrix.get_inactive_buffer(), board_start_x-1, board_start_y-2, board_start_x + 30, board_start_y + 60, 0xff_c);
	}

	void Tetris::draw_block(int16_t x, int16_t y, TetrisPiece::Color col, bool phantom) {
		led::color_t physical_color = 0_c;

		switch (col) {
			case TetrisPiece::I:
				physical_color = 0xaaaaff_cc;
				break;
			case TetrisPiece::J:
				physical_color = 0x4444dd_cc;
				break;
			case TetrisPiece::L:
				physical_color = 0xff7700_cc;
				break;
			case TetrisPiece::O:
				physical_color = 0xffee00_cc;
				break;
			case TetrisPiece::S:
				physical_color = 0x00ff00_cc;
				break;
			case TetrisPiece::Z:
				physical_color = 0xff4444_cc;
				break;
			case TetrisPiece::T:
				physical_color = 0xaaaaaa_cc;
				break;
			default:
				break;
		}

		if (phantom) physical_color = physical_color.mix(0x22_c, 180);

		draw::bitmap(matrix.get_inactive_buffer(), phantom ? bitmap::tetris::phantom : bitmap::tetris::normal, 3, 3, 1, x, y, physical_color);
	}

	void Tetris::draw_piece(const ActiveTetrisPiece& piece, bool phantom) {
		for (int x = 0; x < 4; ++x) {
			for (int y = 0; y < 4; ++y) {
				if (piece.map().at(x, y)) {
					draw_block(piece.x + x*3, piece.y + y*3, piece.blueprint().color, phantom);
				}
			}
		}
	}

	void Tetris::draw_boxed_piece(const ActiveTetrisPiece& atp, const char * text) {
		draw_piece(atp);
		draw::text(matrix.get_inactive_buffer(), text, font::tahoma_9::info, atp.x - 7, atp.y - 3, 0xaa_c);
		draw::outline(matrix.get_inactive_buffer(), atp.x - 9, atp.y - 13, atp.x + 18, atp.y + 16, 0xff_c);
	}

	void Tetris::restart() {
		tick_accum = 0;

		memset(board.board, 0, sizeof board.board);
		current_score = 0;
		current_lines = 0;
		ticks_till_drop = 0;
		das_timer = 0;

		bag.reset();
		pull_next_piece();
		pull_next_piece();
		start_piece();
		state = StatePlaying;
		hold_present = false;
		hold_ready = true;
		drop_timeout = timekeeper.current_time;

		combo = combo_similar = combo_n = 0;
	}

	void Tetris::pause() {
		if (state == StatePlaying) state = StatePaused;
	}

	void Tetris::unpause() {
		if (state == StatePaused) {
			drop_timeout = timekeeper.current_time;
			state = StatePlaying;
		}
	}

	void Tetris::draw_score() {
		auto pos = board_start_x - 3 - draw::text_size("score", font::tahoma_9::info);
		draw::text(matrix.get_inactive_buffer(), "score", font::tahoma_9::info, pos, 10, 0xaa_c);
		char buf[16]{};

		snprintf(buf, 16, "%d", current_score);
		pos = board_start_x - 3 - draw::text_size(buf, font::lcdpixel_6::info);
		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, pos, 17, 0xff_c);

		pos = board_start_x - 3 - draw::text_size("lines", font::tahoma_9::info);
		draw::text(matrix.get_inactive_buffer(), "lines", font::tahoma_9::info, pos, 25, 0xaa_c);

		snprintf(buf, 16, "%d", current_lines);
		pos = board_start_x - 3 - draw::text_size(buf, font::lcdpixel_6::info);
		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, pos, 32, 0xff_c);
	}

	void Tetris::draw_gameover() {
		int pos = 64 - draw::text_size("GAME OVER", font::lato_bold_15::info) / 2;
		draw::text(matrix.get_inactive_buffer(), "GAME OVER", font::lato_bold_15::info, pos, 15, 0xff6666_cc);
		
		// score

		pos = 32 - draw::text_size("score", font::tahoma_9::info) / 2;
		draw::text(matrix.get_inactive_buffer(), "score", font::tahoma_9::info, pos, 27, 0xaa_c);

		char buf[16]{};
		snprintf(buf, 16, "%d", current_score);
		pos = 32 - draw::text_size(buf, font::lcdpixel_6::info) / 2;
		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, pos, 34, 0xff_c);

		pos = 96 - draw::text_size("lines", font::tahoma_9::info) / 2;
		draw::text(matrix.get_inactive_buffer(), "lines", font::tahoma_9::info, pos, 27, 0xaa_c);

		snprintf(buf, 16, "%d", current_lines);
		pos = 96 - draw::text_size(buf, font::lcdpixel_6::info) / 2;
		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, pos, 34, 0xff_c);

		pos = 64 - draw::text_size("hold SELECT to restart", font::tahoma_9::info) / 2;
		draw::text(matrix.get_inactive_buffer(), "hold SELECT to restart", font::tahoma_9::info, pos, 60, 0x7777ff_c);
	}

	void Tetris::draw() {
		switch (state) {
		case StatePlaying:
			// Update tick + buttons
			handle_buttons();
			tick_accum += ui::buttons.frame_time();

			while (tick_accum > pdMS_TO_TICKS(16)) {
				tick_accum -=   pdMS_TO_TICKS(16);
				tick();
			}
		case StatePaused:

			// Draw everything
			draw_board();
			next.x = board_start_x + 30 + 12;
			next.y = 14;
			next.rotation = 0;
			draw_boxed_piece(next, "next");
			if (hold_present) {
				hold.x = board_start_x + 30 + 12;
				hold.y = 45;
				hold.rotation = 0;
				draw_boxed_piece(hold, "held");
			}

			draw_score();

			break;
		case StateGameEnd:
			if (ui::buttons.held(ui::Buttons::SEL, pdMS_TO_TICKS(1000))) restart();

			draw_gameover();

			break;
		}
	}
}
