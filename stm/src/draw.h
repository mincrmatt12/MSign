#ifndef DRAW_H
#define DRAW_H

#include "matrix.h"

#ifndef SIM
typedef led::Matrix<led::FrameBuffer<128, 64, led::FourPanelSnake>> matrix_type;
#else
typedef led::Matrix<led::FrameBuffer<128, 64>> matrix_type;
#endif

namespace draw {
	// Increments every frame.
	extern uint8_t frame_parity;

	// Draw a bitmap image to the framebuffer.
	void bitmap(matrix_type::framebuffer_type &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, led::color_t rgb, bool mirror=false); 

	// Draw a bitmap image to the framebuffer. Pixels adjacent to drawn pixels are drawn in black to emphasize the bitmap against a colored background.
	void outline_bitmap(matrix_type::framebuffer_type &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, led::color_t rgb); 

	int16_t search_kern_table(uint8_t a, uint8_t b, const int16_t * kern, const uint32_t size);

	// How large horizontally is the given text in the font.
	uint16_t text_size(const uint8_t *text, const void * const font[], bool kern_on=true); 
	uint16_t text_size(const char* text,    const void * const font[], bool kern_on=true);

	// Draw text to the framebuffer, returning the position where the next character would go.
	uint16_t text(matrix_type::framebuffer_type &fb, const uint8_t *text, const void * const font[], uint16_t x, uint16_t y, led::color_t rgb, bool kern_on=true); 
	uint16_t text(matrix_type::framebuffer_type &fb, const char * text, const void * const font[], uint16_t x, uint16_t y, led::color_t rgb, bool kern_on=true); 

	// Draw text to the framebuffer, returning the position where the next character would go. Text is outlined with black to emphasize it against
	// a colored background.
	uint16_t outline_text(matrix_type::framebuffer_type &fb, const uint8_t *text, const void * const font[], uint16_t x, uint16_t y, led::color_t rgb, bool kern_on=true); 
	uint16_t outline_text(matrix_type::framebuffer_type &fb, const char * text, const void * const font[], uint16_t x, uint16_t y, led::color_t rgb, bool kern_on=true); 

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

		template<typename TextPtr>
		inline uint16_t outline_multi_text_impl(matrix_type::framebuffer_type& fb, const void * const font[], uint16_t y, uint16_t x, const TextPtr *text, led::color_t rgb) {
			return ::draw::outline_text(fb, text, font, x, y, rgb);
		}
		
		template<typename TextPtr, typename ...Args>
		inline uint16_t outline_multi_text_impl(matrix_type::framebuffer_type& fb, const void * const font[], uint16_t y, uint16_t x, const TextPtr *text, led::color_t rgb, Args&& ...next_strings) {
			return outline_multi_text_impl(fb, font, y, outline_multi_text_impl(fb, font, y, x, text, rgb), std::forward<Args>(next_strings)...);
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
	inline uint16_t outline_multi_text(matrix_type::framebuffer_type& fb, const void * const font[], uint16_t x, uint16_t y, Args&& ...text_then_colors) {
		return detail::outline_multi_text_impl(fb, font, y, x, std::forward<Args>(text_then_colors)...);
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
			return params.start_y - scroll_offset - animation_offset();
		}

		// get the extra scroll amount performed during a transition
		int16_t animation_offset();

		struct ScrollTracker {
			friend struct PageScrollHelper;

			operator int16_t() const {return y;}
			ScrollTracker& operator+=(int16_t entry_height) {
				int16_t onscreen_coord_without_animation = y + animation_offset;
				y += entry_height;
				if (onscreen_coord_without_animation + entry_height <= outer.params.threshold_screen_end && 
						onscreen_coord_without_animation >= outer.params.threshold_screen_start)
					height_onscreen += entry_height;
				total_height += entry_height;

				return *this;
			}
			
			ScrollTracker(const ScrollTracker& other) = delete;
			~ScrollTracker() {
				if (fixed_requested) {
					outer.fix_at(total_height, fix_min_y, fix_max_y);
				}
				else {
					outer.update_with(height_onscreen, total_height);
				}
			}

			void fix(int16_t entry_height) {
				fix_min_y = y - outer.current();
				(*this) += entry_height;
				fix_max_y = y - outer.current();
				fixed_requested = true;
			}

		private:
			ScrollTracker(PageScrollHelper& outer) : outer(outer), y(outer.current()) {
				animation_offset = outer.animation_offset();
			}
			int16_t y, height_onscreen = 0, total_height = 0, animation_offset, fix_min_y = 0, fix_max_y = 0;
			bool fixed_requested = false;
			PageScrollHelper& outer;
		};

		ScrollTracker begin();

		struct Params {
			int16_t start_y = 10; // initial y coordinate
			int16_t threshold_screen_end = 56; // content that spans y-space after this coordinate is considered "off-screen"
			int16_t threshold_screen_start = 10; // content that spans y-space before this is considered "off-screen"
			int16_t screen_region_end = 63; // actual highest coordinate of visible region (if total content fits in this, do not scroll)
			int transition_time = 280; // see tT and tS in draw::distorted_ease_wave
			int hold_time = 1900;
		};

		PageScrollHelper(const Params& params);
	private:
		void update_with(int16_t onscreen_height, int16_t total_height);
		void fix_at(int16_t total_height, int16_t min_y, int16_t max_y);

		const Params& params;
		int16_t scroll_offset = 0, scroll_target = 0;
		uint32_t last_scrolled_at = 0;
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
	in %= 256;
	constexpr uint32_t gamma_cvt[256] = {
#define GAMMA_TABLE 0,5457,29817,80516,162923,281458,439954,641840,890240,1188039,1537929,1942444,2403978,2924810,3507116,4152982,4864412,5643341,6491634,7411101,8403494,9470517,10613827,11835037,13135722,14517420,15981632,17529827,19163445,20883896,22692560,24590795,26579932,28661279,30836123,33105727,35471336,37934175,40495450,43156350,45918046,48781693,51748430,54819382,57995658,61278352,64668548,68167312,71775702,75494761,79325520,83269000,87326210,91498147,95785800,100190146,104712152,109352777,114112969,118993667,123995802,129120297,134368064,139740008,145237029,150860014,156609846,162487399,168493540,174629130,180895023,187292063,193821092,200482942,207278440,214208408,221273660,228475005,235813246,243289179,250903597,258657285,266551025,274585592,282761756,291080282,299541931,308147457,316897611,325793140,334834783,344023278,353359357,362843747,372477171,382260349,392193995,402278820,412515531,422904831,433447417,444143985,454995226,466001827,477164472,488483841,499960610,511595451,523389035,535342027,547455090,559728882,572164060,584761277,597521181,610444420,623531635,636783469,650200557,663783533,677533030,691449674,705534092,719786906,734208736,748800199,763561908,778494476,793598512,808874621,824323407,839945472,855741414,871711829,887857311,904178450,920675835,937350054,954201689,971231322,988439532,1005826897,1023393991,1041141387,1059069655,1077179363,1095471077,1113945361,1132602776,1151443882,1170469237,1189679396,1209074911,1228656336,1248424219,1268379107,1288521546,1308852080,1329371250,1350079596,1370977655,1392065965,1413345058,1434815468,1456477724,1478332356,1500379891,1522620853,1545055767,1567685153,1590509533,1613529424,1636745343,1660157806,1683767325,1707574412,1731579578,1755783331,1780186178,1804788625,1829591174,1854594329,1879798591,1905204457,1930812428,1956622997,1982636661,2008853912,2035275242,2061901143,2088732102,2115768607,2143011144,2170460198,2198116253,2225979790,2254051289,2282331231,2310820093,2339518352,2368426482,2397544959,2426874253,2456414837,2486167181,2516131754,2546309023,2576699454,2607303513,2638121662,2669154366,2700402085,2731865278,2763544406,2795439927,2827552295,2859881968,2892429398,2925195040,2958179346,2991382765,3024805748,3058448743,3092312198,3126396559,3160702271,3195229778,3229979524,3264951950,3300147498,3335566607,3371209715,3407077261,3443169682,3479487413,3516030888,3552800542,3589796806,3627020113,3664470893,3702149575,3740056589,3778192362,3816557321,3855151890,3893976496,3933031561,3972317509,4011834762,4051583739,4091564863,4131778550,4172225221,4212905291,4253819177,4294967295
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
