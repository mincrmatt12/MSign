#include "draw.h"

namespace draw {
	uint16_t text_size(const char * text, const void * const font[], bool kern_on) {
		return text_size(reinterpret_cast<const uint8_t *>(text), font, kern_on);
	}

	void bitmap(matrix_type::framebuffer_type &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, led::color_t rgb, bool mirror) {
		for (uint16_t i = 0, y0 = y; i < height; ++i, ++y0) {
			for (uint16_t j0 = 0, x0 = x; j0 < width; ++j0, ++x0) {
				uint16_t j = mirror ? (width - j0) - 1 : j0;
				uint8_t byte = j / 8;
				uint8_t bit = 1 << (7 - (j % 8));
				if ((bitmap[(i * stride) + byte] & bit) != 0) {
					fb.at(x0, y0) = rgb;
				}
			}
		}
	}

	uint16_t text(matrix_type::framebuffer_type &fb, const uint8_t *text, const void * const font[], uint16_t x, uint16_t y, led::color_t rgb, bool kern_on) {
		uint16_t pen = x;
		uint8_t c, c_prev = 0;

		auto data = reinterpret_cast<const uint8_t * const * const >(font[1]);
		auto metrics = reinterpret_cast<const int16_t *>(font[0]);
		const int16_t * kern;
		uint32_t kern_table_size = 0;
		if (font[2] != nullptr) {
			kern_table_size = *reinterpret_cast<const uint32_t *>(font[2]);
			kern = reinterpret_cast<const int16_t *>(font[3]);
		}
		while ((c = *(text++)) != 0) {
			if (c == ' ') {
				pen += 2;
				continue;
			}
			if (c_prev != 0 && kern_table_size != 0 && kern_on) {
				pen += search_kern_table(c_prev, c, kern, kern_table_size);
			}
			c_prev = c;
			if (data[c] == nullptr) continue; // invalid character
			bitmap(fb, data[c], *(metrics + (c * 6) + 0), *(metrics + (c * 6) + 1), *(metrics + (c * 6) + 2), pen + *(metrics + (c * 6) + 4), y - *(metrics + (c * 6) + 5), rgb);
			pen += *(metrics + (c * 6) + 3);
		}
		return pen;
	}

	void rect(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, led::color_t rgb) {
		for (uint16_t x = x0; x < x1; ++x) {
			for (uint16_t y = y0; y < y1; ++y) {
				fb.at(x, y) = rgb;
			}
		}
	}

	void hatched_rect(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, led::color_t rgb0, led::color_t rgb1) {
		for (uint16_t x = x0; x < x1; ++x) {
			for (uint16_t y = y0; y < y1; ++y) {
				bool color = static_cast<uint16_t>((x % 4) - (y % 4)) % 4 < 2;
				if (color) {
					fb.at(x, y) = rgb0;
				}
				else {
					fb.at(x, y) = rgb1;
				}
			}
		}
	}

	void hatched_rect_unaligned(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb0, led::color_t rgb1) {
		uint16_t i = 0;
		for (int16_t x = x0; x < x1; ++x, ++i) {
			uint16_t j = 0;
			for (int16_t y = y0; y < y1; ++y, ++j) {
				bool color = static_cast<uint16_t>((i % 4) - (j % 4)) % 4 < 2;
				if (color) {
					fb.at(x, y) = rgb0;
				}
				else {
					fb.at(x, y) = rgb1;
				}
			}
		}
	}

	led::color_t cvt(led::color_t in) {
		return {cvt(in.r >> 4), cvt(in.g >> 4), cvt(in.b >> 4)};
	}

	void gradient_rect(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, led::color_t rgb0, led::color_t rgb1, bool v) {
		uint16_t i = 0;
		uint16_t range = v ? (y1-y0-1) : (x1-x0-1);
		for (uint16_t x = x0; x < x1; ++x) {
			for (uint16_t y = y0; y < y1; ++y) {
				fb.at(x, y) = cvt(rgb0.mix(rgb1, (255*i)/range));
				if (v) ++i;
			}
			if (!v) ++i;
			else i = 0;
		}
	}

	void outline(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, led::color_t rgb) {
		for (int y = y0; y < y1; ++y) {
			fb.at(x0, y) = rgb;
			fb.at(x1, y) = rgb;
		}

		for (int x = x0; x < x1; ++x) {
			fb.at(x, y0) = rgb;
			fb.at(x, y1) = rgb;
		}
	}

	void dashed_outline(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, led::color_t rgb) {
		for (int y = y0; y < y1; y += 4) {
			fb.at(x0, y) = rgb;
			fb.at(x1, y) = rgb;
			fb.at(x0, y+1) = rgb;
			fb.at(x1, y+1) = rgb;
		}

		for (int x = x0; x < x1; x += 4) {
			fb.at(x, y0) = rgb;
			fb.at(x, y1) = rgb;
			fb.at(x+1, y0) = rgb;
			fb.at(x+1, y1) = rgb;
		}
	}

	uint16_t text(matrix_type::framebuffer_type &fb, const char * text, const void * const font[], uint16_t x, uint16_t y, led::color_t rgb, bool kern_on) {
		return ::draw::text(fb, reinterpret_cast<const uint8_t *>(text), font, x, y, rgb, kern_on);
	}

	int16_t search_kern_table(uint8_t a, uint8_t b, const int16_t * kern, const uint32_t size) {
		uint32_t start = 0, end = size;
		uint16_t needle = (((uint16_t)a << 8) + (uint16_t)b);
		while (start != end) {
			uint32_t head = (start + end) / 2;
			uint16_t current = ((uint16_t)(kern[head*3]) << 8) + (uint16_t)(kern[head*3 + 1]);
			if (current == needle) {
				return kern[head*3 + 2];
			}
			else {
				if (start - end == 1 || end - start == 1) {
					return 0;
				}
				if (current > needle) {
					end = head;
				}
				else {
					start = head;
				}
			}
		}
		return 0;
	}

	uint16_t text_size(const uint8_t *text, const void * const font[], bool kern_on) {
		uint16_t len = 0;
		uint8_t c, c_prev = 0;

		auto data = reinterpret_cast<const uint8_t * const * const >(font[1]);
		auto metrics = reinterpret_cast<const int16_t *>(font[0]);
		const int16_t * kern;
		uint32_t kern_table_size = 0;
		if (font[2] != nullptr) {
			kern_table_size = *reinterpret_cast<const uint32_t *>(font[2]);
			kern = reinterpret_cast<const int16_t *>(font[3]);
		}

		while ((c = *(text++)) != 0) {
			if (c == ' ') {
				len += 2;
				continue;
			}
			if (c_prev != 0 && kern_table_size != 0 && kern_on) {
				len += search_kern_table(c_prev, c, kern, kern_table_size);
			}
			c_prev = c;
			if (data[c] == nullptr) continue; // invalid character
			len += *(metrics + (c * 6) + 3);
		}

		c = *(text - 1);
		len -= *(metrics + (c * 6) + 3);
		len += *(metrics + (c * 6));

		return len;
	}

	namespace detail {
		
		void line_impl_low(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb) {
			int dx = x1 - x0;
			int dy = y1 - y0;
			int yi = 1;
			if (dy < 0) {
				yi = -1;
				dy = -dy;
			}
			int D = 2*dy - dx;
			int16_t y = y0;

			for (int16_t x = x0; x <= x1; ++x) {
				fb.at((uint16_t)x, (uint16_t)y) = rgb;
				
				if (D > 0) {
					y += yi;
					D -= 2*dx;
				}
				D += 2*dy;
			}
		}

		
		void line_impl_high(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb) {
			int dx = x1 - x0;
			int dy = y1 - y0;
			int xi = 1;
			if (dx < 0) {
				xi = -1;
				dx = -dx;
			}
			int D = 2*dx - dy;
			int16_t x = x0;

			for (int16_t y = y0; y <= y1; ++y) {
				fb.at((uint16_t)x, (uint16_t)y) = rgb;
				
				if (D > 0) {
					x += xi;
					D -= 2*dy;
				}
				D += 2*dx;
			}
		}
	}

	void fill(matrix_type::framebuffer_type &fb, led::color_t rgb) {
		rect(fb, 0, 0, matrix_type::framebuffer_type::width, matrix_type::framebuffer_type::height, rgb);
	}

	void line(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, led::color_t rgb) {
		if (std::abs(y1 - y0) < std::abs(x1 - x0)) {
			if (x0 > x1)
				detail::line_impl_low(fb, x1, y1, x0, y0, rgb);
			else
				detail::line_impl_low(fb, x0, y0, x1, y1, rgb);
		}
		else {
			if (y0 > y1)
				detail::line_impl_high(fb, x1, y1, x0, y0, rgb);
			else
				detail::line_impl_high(fb, x0, y0, x1, y1, rgb);
		}
	}

	int32_t fastsin(int32_t in, int32_t fac, int32_t fac_out) {
		if (fac_out == 0) return 0;
		int32_t phase = in % (fac * 2);
		if (phase > fac) return -fastsin(phase - fac, fac, fac_out);
		if (phase > fac / 2) return fastsin(fac - phase, fac, fac_out);
		if (phase == fac / 2) return fac_out;
		int32_t raw = sin_table[(phase*512)/fac];
		if (!fac_out) fac_out = fac;
		return (raw * fac_out) / INT16_MAX;
	}

	int16_t distorted_ease_wave(int64_t timebase, int64_t tT, int64_t tS, int16_t U) {
		const static int16_t table[256] = {0,0,0,1,2,3,4,6,8,11,14,17,21,26,31,36,42,49,56,64,72,81,91,101,112,123,135,148,162,176,191,207,223,240,258,276,296,316,337,358,381,404,428,453,479,505,532,561,590,620,650,682,715,748,782,818,854,891,929,968,1007,1048,1090,1133,1176,1221,1266,1313,1360,1409,1458,1509,1560,1613,1667,1721,1777,1833,1891,1950,2009,2070,2132,2195,2259,2324,2390,2458,2526,2595,2666,2738,2810,2884,2959,3035,3113,3191,3270,3351,3433,3516,3600,3685,3772,3859,3948,4038,4129,4222,4315,4410,4506,4603,4701,4801,4902,5004,5107,5212,5317,5424,5533,5642,5753,5865,5978,6092,6208,6325,6444,6563,6684,6806,6930,7054,7180,7308,7437,7567,7698,7830,7964,8100,8236,8374,8513,8654,8796,8939,9084,9230,9377,9526,9676,9828,9981,10135,10291,10448,10606,10766,10927,11089,11253,11419,11586,11754,11923,12095,12267,12441,12616,12793,12971,13151,13332,13514,13698,13884,14071,14259,14449,14640,14833,15027,15223,15420,15619,15819,16020,16223,16428,16634,16842,17051,17261,17473,17687,17902,18119,18337,18557,18778,19001,19225,19451,19678,19907,20138,20370,20603,20839,21075,21313,21553,21795,22038,22282,22528,22776,23025,23276,23528,23782,24038,24295,24554,24814,25076,25340,25605,25872,26141,26411,26682,26956,27231,27507,27785,28065,28347,28630,28915,29201,29489,29779,30070,30363,30658,30954,31252,31552,31853,32156,32461};

		const int64_t period = (tT + tS), twoperiod = 2*(tT + tS);
		bool isU = (timebase % twoperiod) < period;

		if (timebase % period < tS) return isU * U;

		int t = ((timebase % period) - tS); // = t from 0-tT

		int16_t powt = table[std::clamp<int>(t*256 / tT, 0, 255)];
		int16_t powtminone = table[255 - std::clamp<int>(t*256 / tT, 0, 255)];

		int16_t result = ((int)U * powt) / (powt + powtminone);

		if (isU) 		
			return U - result;
		else
			return result;
	}
}

uint8_t draw::frame_parity = 0;

// COLOR TABLE

const uint32_t draw::gamma_cvt[256] = {
	GAMMA_TABLE
};

const int16_t draw::sin_table[256] = {
	0,201,402,603,804,1005,1206,1407,1608,1809,2009,2210,2410,2611,2811,3012,3212,3412,3612,3811,4011,4210,4410,4609,4808,5007,5205,5404,5602,5800,5998,6195,6393,6590,6786,6983,7179,7375,7571,7767,7962,8157,8351,8545,8739,8933,9126,9319,9512,9704,9896,10087,10278,10469,10659,10849,11039,11228,11417,11605,11793,11980,12167,12353,12539,12725,12910,13094,13279,13462,13645,13828,14010,14191,14372,14553,14732,14912,15090,15269,15446,15623,15800,15976,16151,16325,16499,16673,16846,17018,17189,17360,17530,17700,17869,18037,18204,18371,18537,18703,18868,19032,19195,19357,19519,19680,19841,20000,20159,20317,20475,20631,20787,20942,21096,21250,21403,21554,21705,21856,22005,22154,22301,22448,22594,22739,22884,23027,23170,23311,23452,23592,23731,23870,24007,24143,24279,24413,24547,24680,24811,24942,25072,25201,25329,25456,25582,25708,25832,25955,26077,26198,26319,26438,26556,26674,26790,26905,27019,27133,27245,27356,27466,27575,27683,27790,27896,28001,28105,28208,28310,28411,28510,28609,28706,28803,28898,28992,29085,29177,29268,29358,29447,29534,29621,29706,29791,29874,29956,30037,30117,30195,30273,30349,30424,30498,30571,30643,30714,30783,30852,30919,30985,31050,31113,31176,31237,31297,31356,31414,31470,31526,31580,31633,31685,31736,31785,31833,31880,31926,31971,32014,32057,32098,32137,32176,32213,32250,32285,32318,32351,32382,32412,32441,32469,32495,32521,32545,32567,32589,32609,32628,32646,32663,32678,32692,32705,32717,32728,32737,32745,32752,32757,32761,32765,32766	
};
