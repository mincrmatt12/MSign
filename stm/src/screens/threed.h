#ifndef THREED_H
#define THREED_H

#include <stdint.h>
#include "../draw.h"
#include "base.h"
#include <string.h>
#include <cmath>
#include "../common/slots.h"

namespace threed {
	struct Vec3 {
		float x, y, z;

		constexpr Vec3()                          : x(0), y(0), z(0) {}
		constexpr Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
		constexpr Vec3(float a)                   : x(a), y(a), z(a) {}
		constexpr Vec3(const slots::Vec3& v)      : x(v.x), y(v.y), z(v.z) {}

		float length() const;
		Vec3 normalize() const;

		Vec3 & operator+=(const Vec3& rhs) {
			this->x += rhs.x;
			this->y += rhs.y;
			this->z += rhs.z;
			return *this;
		}

		Vec3 & operator-=(const Vec3& rhs) {
			this->x -= rhs.x;
			this->y -= rhs.y;
			this->z -= rhs.z;
			return *this;
		}

		Vec3& operator*=(const float& rhs) {
			this->x *= rhs;
			this->y *= rhs;
			this->z *= rhs;
			return *this;
		}

		Vec3& operator/=(const float& rhs) {
			this->x /= rhs;
			this->y /= rhs;
			this->z /= rhs;
			return *this;
		}

		Vec3 operator+(const Vec3& rhs) const { return (Vec3(*this) += rhs); }
		Vec3 operator-(const Vec3& rhs)  const{ return (Vec3(*this) -= rhs); }
		Vec3 operator/(const float& rhs) const { return (Vec3(*this) /= rhs); }
		Vec3 operator*(const float& rhs) const { return (Vec3(*this) *= rhs); }
		Vec3 operator-() const {
			return Vec3(-x, -y, -z);
		}

		float dot(const Vec3& other) const;
		Vec3 cross(const Vec3& other) const;
	};

	struct Vec4 {
		float x, y, z, w;

		Vec4() : x(0), y(0), z(0), w(0) {}
		Vec4(const Vec3 & a, float w) : x(a.x), y(a.y), z(a.z), w(w) {}
		Vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
		Vec4(slots::Vec3 & a) : x(a.x), y(a.y), z(a.z), w(1.0) {}

		operator Vec3() const {return Vec3(x, y, z);}

		Vec4 & operator+=(const Vec4& rhs) {
			this->x += rhs.x;
			this->y += rhs.y;
			this->z += rhs.z;
			this->w += rhs.w;
			return *this;
		}

		Vec4 & operator-=(const Vec4& rhs) {
			this->x -= rhs.x;
			this->y -= rhs.y;
			this->z -= rhs.z;
			this->w -= rhs.w;
			return *this;
		}

		Vec4& operator*=(const float& rhs) {
			this->x *= rhs;
			this->y *= rhs;
			this->z *= rhs;
			this->w *= rhs;
			return *this;
		}

		Vec4& operator/=(const float& rhs) {
			this->x /= rhs;
			this->y /= rhs;
			this->z /= rhs;
			this->w /= rhs;
			return *this;
		}

		Vec4 operator+(const Vec4& rhs) const { return (Vec4(*this) += rhs); }
		Vec4 operator-(const Vec4& rhs)  const{ return (Vec4(*this) -= rhs); }
		Vec4 operator/(const float& rhs) const { return (Vec4(*this) /= rhs); }
		Vec4 operator*(const float& rhs) const { return (Vec4(*this) *= rhs); }
		Vec4 operator-() const {
			return Vec4(-x, -y, -z, -w);
		}
	};

	struct Mat4 {
		float a[4], b[4], c[4], d[4];

		Mat4() : a{1, 0, 0, 0}, b{0, 1, 0, 0}, c{0, 0, 1, 0}, d{0, 0, 0, 1} {}
		Mat4(float x) : a{x, 0, 0, 0}, b{0, x, 0, 0}, c{0, 0, x, 0}, d{0, 0, 0, x} {}
		Mat4(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33) :
			a{m00, m01, m02, m03}, b{m10, m11, m12, m13}, c{m20, m21, m22, m23}, d{m30, m31, m32, m33} {}

		static Mat4 translate(const Vec3 &by);
		static Mat4 perspective(float aspect, float fov, float zn, float zf);
		static Mat4 rotate(const Vec3& a, float rad);
		static Mat4 lookat(const Vec3& from, const Vec3& to, const Vec3& up);

		Mat4& operator*=(const Mat4& other) {*this = *this * other; return *this;}

		Vec4  operator*(const Vec4&  other) const;

		Mat4  operator*(const Mat4 &rhs) const;
	};

	struct Tri {
		Vec3 p1, p2, p3;
		uint8_t r, g, b;
	};

	struct Renderer : public screen::Screen {
		static void prepare(bool);

		Renderer();
		~Renderer();

		void draw();

		constexpr static inline bool require_clearing() {return false;}
	private:
		void draw_triangle(const Tri& t, bool enable_lighting);
		void update_matricies();

		int current_tri = 0;

		Mat4 perpview;
		Vec3 camera_pos, camera_target;
		Vec3 current_pos, current_look;
		Vec3 camera_look, camera_look_target;
		uint16_t interp_progress = 20000;
		uint64_t last_update, last_new_data;

		/*
		inline void set_color_and_z(matrix_type::framebuffer_type &fb, uint16_t x, uint16_t y, uint16_t r, uint16_t g, uint16_t b, int16_t z) const {
			fb.r(x, y) = (r & 0xFFF) | ((z & 0x00F)) << 12;
			fb.g(x, y) = (g & 0xFFF) | ((z & 0x0F0) >> 4) << 12;
			fb.b(x, y) = (b & 0xFFF) | ((z & 0xF00) >> 8) << 12;
		}
		*/

		void line_impl_low(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, float d1, float d2, uint16_t r, uint16_t g, uint16_t b) {
			int dx = x1 - x0;
			int dy = y1 - y0;
			int yi = 1;
			if (dy < 0) {
				yi = -1;
				dy = -dy;
			}
			int D = 2*dy - dx;
			int16_t y = y0;

			float d = d1, d_off = (d2 - d1) / (float)dx;
			led::color_t c(r, g, b);

			for (int16_t x = x0; x <= x1; ++x) {
				if (fb.on_screen(x, y) && d * 410.f < fb.at(x, y).get_spare()) {
					c.set_spare(d * 410.f);
					fb.at(x, y) = c;
				}
				if (D > 0) {
					y += yi;
					D -= 2*dx;
				}
				D += 2*dy;
				d += d_off;
			}
		}

		
		void line_impl_high(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, float d1, float d2, uint16_t r, uint16_t g, uint16_t b) {
			int dx = x1 - x0;
			int dy = y1 - y0;
			int xi = 1;
			if (dx < 0) {
				xi = -1;
				dx = -dx;
			}
			int D = 2*dx - dy;
			int16_t x = x0;

			float d = d1, d_off = (d2 - d1) / (float)dy;
			led::color_t c(r, g, b);

			for (int16_t y = y0; y <= y1; ++y) {
				if (fb.on_screen(x, y) && d * 410.f < fb.at(x, y).get_spare()) {
					c.set_spare(d * 410.f);
					fb.at(x, y) = c;
				}
				if (D > 0) {
					x += xi;
					D -= 2*dy;
				}
				D += 2*dx;
				d += d_off;
			}
		}

		
		void line(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, float d1, float d2, uint16_t r, uint16_t g, uint16_t b) {
			if (abs(y1 - y0) < abs(x1 - x0)) {
				if (x0 > x1)
					line_impl_low(fb, x1, y1, x0, y0, d2, d1, r, g, b);
				else
					line_impl_low(fb, x0, y0, x1, y1, d1, d2, r, g, b);
			}
			else {
				if (y0 > y1)
					line_impl_high(fb, x1, y1, x0, y0, d2, d1, r, g, b);
				else
					line_impl_high(fb, x0, y0, x1, y1, d1, d2, r, g, b);
			}
		}
	};

	extern size_t tri_count;
}

#endif
