#ifndef DRAW_H
#define DRAW_H

#include "matrix.h"
#include <cmath>
#include <cstdlib>

#ifndef SIM
typedef led::Matrix<led::FrameBuffer<128, 64, led::FourPanelSnake>> matrix_type;
#else
typedef led::Matrix<led::FrameBuffer<128, 64>> matrix_type;
#endif

namespace draw {
	// Increments every frame.
	extern uint8_t frame_parity;

	// Draw a bitmap image to the framebuffer. Mirroring is based on bit ordering.
	void bitmap(matrix_type::framebuffer_type &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, led::color_t rgb, bool mirror=false); 

	int16_t search_kern_table(uint8_t a, uint8_t b, const int16_t * kern, const uint32_t size);

	// How large horizontally is the given text in the font.
	uint16_t text_size(const uint8_t *text, const void * const font[], bool kern_on=true); 
	uint16_t text_size(const char* text,    const void * const font[], bool kern_on=true);

	// Draw text to the framebuffer, returning the position where the next character would go.
	uint16_t text(matrix_type::framebuffer_type &fb, const uint8_t *text, const void * const font[], uint16_t x, uint16_t y, led::color_t rgb, bool kern_on=true); 
	uint16_t text(matrix_type::framebuffer_type &fb, const char * text, const void * const font[], uint16_t x, uint16_t y, led::color_t rgb, bool kern_on=true); 

	// Draw a filled rectangle using exclusive coordinates (occupies x0 to x1 but not including x1)
	void rect(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb); 

	// Draw a hatched rectangle using exclusive coordinates. Hatching will create a checkerboard pattern between the two colors provided.
	void hatched_rect(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb0, led::color_t rgb1);

	// Like, hatched_rect, but with the coordinate grid unaligned so you can animate it.
	void hatched_rect_unaligned(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb0, led::color_t rgb1);

	// Draw a gradient rectangle (gradient is horizontal). Takes _ungammacorrected_ colors, (use _ccu)
	void gradient_rect(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, led::color_t rgb0, led::color_t rgb1, bool vertical=false);

	// Draw an outline rectangle
	void outline(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb);

	// Draw dashed outline
	void dashed_outline(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb);

	// Fill the entire screen.
	void fill(matrix_type::framebuffer_type &fb, led::color_t rgb); 

	namespace detail {
		void line_impl_low(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb); 
		void line_impl_high(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb); 
	}
	
	// Draw a line including both endpoints
	void line(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb); 

	// Draw a filled in circle inside a given bounding box (with _inclusive_ coordinates)
	void circle(matrix_type::framebuffer_type& fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb);

	// Gamma correction table
	extern const uint32_t gamma_cvt[256];

	// Perform gamma correction and 12-bit conversion on 8-bit color
	inline uint16_t cvt(uint8_t in) {
		return gamma_cvt[in] >> 20;
	}

	// Same as cvt, except convert with full 16-bit precision
	template<size_t Precision=16>
	inline uint32_t cvt_full(uint8_t in) {
		return gamma_cvt[in] >> (32 - Precision);
	}

	// Stores 1/4 of a sinewave, fractional components such that 0 is 0 and INT16_MAX is 1.
	extern const int16_t sin_table[256];

	// Computes an approximation of sin((in/fac)*pi)*fac_out.
	//
	// If fac_out > INT16_MAX, overflow will occur
	int32_t fastsin(int32_t in, int32_t fac=1500, int32_t fac_out=1500); 

	// Gamma correct an entire color
	led::color_t cvt(led::color_t in);

	// Perform basic temporal pattern-ordered 2x2 dithering with input at an arbitrary bit-depth on a single channel. Assumes input is pre gamma-corrected.
	template<size_t Precision=16>
	int16_t dither(std::conditional_t<(Precision > 16), uint64_t, uint32_t> fullcolor, int16_t x, int16_t y) {
		// maximum 64
		static const uint8_t dithermap[2*2] = {
			0, 255,
			170, 85
		};

		// In 32-bit space, from squaring -- assumes gamma = 2.0 but eh it's better than nothing

		decltype(fullcolor) below = fullcolor & ~0xf;
		decltype(fullcolor) above = below + (1 << (Precision - 12));
		below *= below; 
		above *= above;

		// Compute offset: 
		decltype(fullcolor) offset = (255*((fullcolor*fullcolor)-below)) / (above - below);

		char dithervalue = dithermap[(((y%2)*2 + (x%2)) + draw::frame_parity) % 4];

		if (dithervalue < offset) return (fullcolor >> (Precision - 12)) + 1;
		return (fullcolor >> (Precision - 12)) + 0;
	}

	namespace detail {
		template<typename TextPtr>
		inline uint16_t multi_text_impl(matrix_type::framebuffer_type& fb, const void * const font[], uint16_t y, uint16_t x, const TextPtr *text, led::color_t rgb) {
			return ::draw::text(fb, text, font, x, y, rgb);
		}
		
		template<typename TextPtr, typename ...Args>
		inline uint16_t multi_text_impl(matrix_type::framebuffer_type& fb, const void * const font[], uint16_t y, uint16_t x, const TextPtr *text, led::color_t rgb, Args&& ...next_strings) {
			return multi_text_impl(fb, font, y, multi_text_impl(fb, font, y, x, text, rgb), std::forward<Args>(next_strings)...);
		}

		inline void multi_gradient_rect(matrix_type::framebuffer_type& fb, uint16_t x0, uint16_t y0, uint16_t y1, led::color_t rgb0) {}

		template<typename ...Args>
		inline void multi_gradient_rect(matrix_type::framebuffer_type& fb, uint16_t x0, uint16_t y0, uint16_t y1, led::color_t rgb0, uint16_t pole0, led::color_t rgb1, Args&& ...next_colors) {
			gradient_rect(fb, x0, y0, pole0, y1, rgb0, rgb1);
			multi_gradient_rect(fb, pole0, y0, y1, rgb1, std::forward<Args>(next_colors)...);
		}
	}

	template<typename ...Args>
	inline uint16_t multi_text(matrix_type::framebuffer_type& fb, const void * const font[], uint16_t x, uint16_t y, Args&& ...text_then_colors) {
		return detail::multi_text_impl(fb, font, y, x, std::forward<Args>(text_then_colors)...);
	}

	template<typename ...Args>
	inline void multi_gradient_rect(matrix_type::framebuffer_type& fb, uint16_t x0, uint16_t y0, uint16_t y1, led::color_t rgb0, Args&& ...pos_then_colors) {
		return detail::multi_gradient_rect(fb, x0, y0, y1, rgb0, std::forward<Args>(pos_then_colors)...);
	}
	
	// Easing/scrolling helper functions.
	//
	// These generally take a timebase to use and content size params.
	
	// Scrolls content. Output moves backwards, so for left-right scrolling it goes from the right.
	inline int16_t scroll(int64_t timebase, int16_t content_size, int16_t scroll_size) {
		if (content_size < scroll_size) return 0;
		timebase %= (content_size + scroll_size) + 1;
		timebase =  ((content_size + scroll_size) + 1) - timebase;
		timebase -= content_size;
		return timebase;
	}

	// Scrolls content. Output moves backwards, so for left-right scrolling it goes from the right.
	inline int16_t scroll(int64_t timebase, int16_t content_size) {
		return scroll(timebase, content_size, matrix_type::framebuffer_type::width);
	}

	// ```
	//           .......            --U
	//         ..| tS  |..|tT |
	//        .           .
	//       .             .
	// |tS|..               ..| tS |
	// .... | tT |            ......--0
	// |                      |
	// 0                  2(tS+tT)
	// ```
	//
	// Creates a wave of the above pattern. Generally used to show two "screens" of data at once (or more, if used correctly)
	//
	// timebase: Current time (in units same as tT & tS)
	// tT: time Transition: how long the easing lasts
	// tS: time Stable: how long before the easing starts
	// U:  Upper value: where to ease to.
	int16_t distorted_ease_wave(int64_t timebase, int64_t tT, int64_t tS, int16_t U);

	// Helper to auto-scroll through vertical "segments" on a timer.
	//
	// Construct in class, then use as
	//
	// {
	// 		auto y = scroll_helper.begin(10);
	// 		int16_t height = draw_something(y, ...);
	// 		y += height;
	//
	// 		... etc ...
	// }
	struct PageScrollHelper {
		// get the current y offset of the scroll
		int16_t current() {
			return -scroll_offset - animation_offset();
		}

		// get the extra scroll amount performed during a transition
		int16_t animation_offset();

		struct ScrollTracker {
			friend struct PageScrollHelper;

			operator int16_t() const {return y;}
			ScrollTracker& operator+=(int16_t entry_height) {
				int16_t onscreen_coord_without_animation = y + animation_offset;
				y += entry_height;
				if (onscreen_coord_without_animation + entry_height < outer.params.threshold_screen_end && 
						onscreen_coord_without_animation > outer.params.threshold_screen_start)
					height_onscreen += entry_height;
				total_height += entry_height;

				return *this;
			}
			
			ScrollTracker(const ScrollTracker& other) = delete;
			~ScrollTracker() {
				outer.update_with(height_onscreen, total_height);
			}

		private:
			ScrollTracker(PageScrollHelper& outer, int16_t initial_height) : outer(outer), y(initial_height) {
				animation_offset = outer.animation_offset();
			}
			int16_t y, height_onscreen = 0, total_height = 0, animation_offset;
			PageScrollHelper& outer;
		};

		ScrollTracker begin(int16_t start_y=0);

		struct Params {
			int16_t threshold_screen_end = 56; // content that spans y-space after this coordinate is considered "off-screen"
			int16_t threshold_screen_start = 10; // content that spans y-space before this is considered "off-screen"
			int16_t screen_region_end = 63; // actual highest coordinate of visible region (if total content fits in this, do not scroll)
			int transition_time = 280; // see tT and tS in draw::distorted_ease_wave
			int hold_time = 1900;
		};

		PageScrollHelper(const Params& params);
	private:
		void update_with(int16_t onscreen_height, int16_t total_height);

		const Params& params;
		int16_t scroll_offset = 0, scroll_target = 0;
		uint64_t last_scrolled_at = 0;
	};

	// Format a timestamp as a string with relative measurements.
	// Only designed for fairly nearby dates
	//
	// For example,
	//    "4 seconds ago",
	//    "in 5 minutes",
	//    "2 hours ago",
	//    "in 28 hours",
	//    "in a day",
	//    "yesterday",
	//    "2 days ago",
	//    "May 13"
	//
	// Fills buf up to buflen.
	void format_relative_date(char * buf, size_t buflen, uint64_t date);
}

inline constexpr uint16_t operator ""_cu(unsigned long long in) {
	return (uint16_t)(in & 0xff) << 4;
}

inline constexpr uint16_t operator ""_c(unsigned long long in) {
	constexpr uint32_t gamma_cvt[256] = {
#define GAMMA_TABLE 0,2377,14409,41350,87360,156055,250697,374292,529650,719426,946140,1212206,1519941,1871579,2269281,2715143,3211200,3759435,4361781,5020128,5736320,6512167,7349442,8249883,9215197,10247063,11347132,12517027,13758347,15072670,16461547,17926512,19469076,21090731,22792952,24577195,26444898,28397485,30436362,32562922,34778542,37084585,39482400,41973325,44558683,47239787,50017935,52894416,55870507,58947474,62126574,65409051,68796142,72289073,75889059,79597309,83415021,87343385,91383582,95536785,99804160,104186863,108686045,113302846,118038403,122893841,127870282,132968840,138190620,143536723,149008244,154606269,160331879,166186151,172170152,178284947,184531593,190911143,197424642,204073132,210857648,217779222,224838877,232037636,239376513,246856518,254478658,262243933,270153339,278207869,286408509,294756243,303252048,311896900,320691768,329637617,338735410,347986103,357390652,366950005,376665108,386536904,396566330,406754322,417101810,427609721,438278979,449110505,460105216,471264023,482587839,494077568,505734115,517558380,529551260,541713647,554046434,566550507,579226751,592076047,605099274,618297307,631671019,645221279,658948955,672854910,686940006,701205101,715651050,730278708,745088925,760082548,775260422,790623390,806172293,821907967,837831248,853942968,870243958,886735045,903417054,920290809,937357130,954616836,972070742,989719661,1007564406,1025605786,1043844608,1062281675,1080917792,1099753759,1118790373,1138028431,1157468728,1177112055,1196959202,1217010957,1237268106,1257731434,1278401722,1299279750,1320366296,1341662136,1363168045,1384884795,1406813156,1428953896,1451307784,1473875582,1496658056,1519655965,1542870069,1566301126,1589949892,1613817121,1637903566,1662209976,1686737102,1711485691,1736456488,1761650237,1787067681,1812709561,1838576614,1864669580,1890989194,1917536189,1944311299,1971315256,1998548788,2026012623,2053707488,2081634109,2109793208,2138185507,2166811728,2195672589,2224768807,2254101099,2283670179,2313476761,2343521556,2373805275,2404328627,2435092319,2466097057,2497343547,2528832492,2560564593,2592540552,2624761068,2657226840,2689938563,2722896933,2756102644,2789556390,2823258862,2857210750,2891412742,2925865528,2960569792,2995526222,3030735499,3066198308,3101915330,3137887245,3174114732,3210598469,3247339133,3284337399,3321593942,3359109435,3396884550,3434919957,3473216327,3511774328,3550594628,3589677892,3629024787,3668635975,3708512120,3748653884,3789061928,3829736911,3870679491,3911890327,3953370074,3995119389,4037138925,4079429335,4121991272,4164825387,4207932330,4251312750,4294967295
		GAMMA_TABLE
	};
	return gamma_cvt[in] >> 20;
}

inline constexpr led::color_t operator ""_cc(unsigned long long in) {
	return {operator""_c((in >> 16) & 0xff), operator""_c((in >> 8) & 0xff), operator""_c(in & 0xff)};
}

inline constexpr led::color_t operator ""_ccu(unsigned long long in) {
	return {operator""_cu((in >> 16) & 0xff), operator""_cu((in >> 8) & 0xff), operator""_cu(in & 0xff)};
}

#endif
