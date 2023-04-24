#ifndef THREED_H
#define THREED_H

#include <stdint.h>
#include "../draw.h"
#include "base.h"
#include <string.h>
#include <cmath>
#include "../common/slots.h"
#include "threed/fixed.h"

namespace threed {
	struct Vec3 {
		m::fixed_t x, y, z;

		constexpr Vec3()                                         : x(0), y(0), z(0) {}
		constexpr Vec3(m::fixed_t a, m::fixed_t b, m::fixed_t c) : x(a), y(b), z(c) {}
		constexpr Vec3(m::fixed_t a)                             : x(a), y(a), z(a) {}
		constexpr Vec3(const slots::Vec3& v)                     : x(v.x, 512), y(v.y, 512), z(v.z, 512) {}

		m::fixed_t length() const;
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

		Vec3& operator*=(const m::fixed_t rhs) {
			this->x *= rhs;
			this->y *= rhs;
			this->z *= rhs;
			return *this;
		}

		Vec3& operator/=(const m::fixed_t rhs) {
			this->x /= rhs;
			this->y /= rhs;
			this->z /= rhs;
			return *this;
		}

		Vec3& operator*=(const int rhs) {
			this->x *= rhs;
			this->y *= rhs;
			this->z *= rhs;
			return *this;
		}

		Vec3& operator/=(const int rhs) {
			this->x /= rhs;
			this->y /= rhs;
			this->z /= rhs;
			return *this;
		}

		Vec3 operator+(const Vec3& rhs) const { return (Vec3(*this) += rhs); }
		Vec3 operator-(const Vec3& rhs)  const{ return (Vec3(*this) -= rhs); }
		Vec3 operator/(const m::fixed_t rhs) const { return (Vec3(*this) /= rhs); }
		Vec3 operator*(const m::fixed_t rhs) const { return (Vec3(*this) *= rhs); }
		Vec3 operator/(const int rhs) const { return (Vec3(*this) /= rhs); }
		Vec3 operator*(const int rhs) const { return (Vec3(*this) *= rhs); }
		Vec3 operator-() const {
			return Vec3(-x, -y, -z);
		}

		m::fixed_t dot(const Vec3& other) const;
		Vec3 cross(const Vec3& other) const;
	};

	struct Vec4 {
		m::fixed_t x, y, z, w;

		Vec4() : x(0), y(0), z(0), w(0) {}
		Vec4(const Vec3 & a, m::fixed_t w) : x(a.x), y(a.y), z(a.z), w(w) {}
		Vec4(m::fixed_t a, m::fixed_t b, m::fixed_t c, m::fixed_t d) : x(a), y(b), z(c), w(d) {}
		Vec4(slots::Vec3 & a) : x(a.x), y(a.y), z(a.z), w(1) {}

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

		Vec4& operator*=(const m::fixed_t& rhs) {
			this->x *= rhs;
			this->y *= rhs;
			this->z *= rhs;
			this->w *= rhs;
			return *this;
		}

		Vec4& operator/=(const m::fixed_t& rhs) {
			this->x /= rhs;
			this->y /= rhs;
			this->z /= rhs;
			this->w /= rhs;
			return *this;
		}

		Vec4& operator*=(const int& rhs) {
			this->x *= rhs;
			this->y *= rhs;
			this->z *= rhs;
			this->w *= rhs;
			return *this;
		}

		Vec4& operator/=(const int& rhs) {
			this->x /= rhs;
			this->y /= rhs;
			this->z /= rhs;
			this->w /= rhs;
			return *this;
		}

		Vec4 operator+(const Vec4& rhs) const { return (Vec4(*this) += rhs); }
		Vec4 operator-(const Vec4& rhs)  const{ return (Vec4(*this) -= rhs); }
		Vec4 operator/(const m::fixed_t& rhs) const { return (Vec4(*this) /= rhs); }
		Vec4 operator*(const m::fixed_t& rhs) const { return (Vec4(*this) *= rhs); }
		Vec4 operator/(const int& rhs) const { return (Vec4(*this) /= rhs); }
		Vec4 operator*(const int& rhs) const { return (Vec4(*this) *= rhs); }
		Vec4 operator-() const {
			return Vec4(-x, -y, -z, -w);
		}
	};

	struct Mat4 {
		m::fixed_t a[4], b[4], c[4], d[4];

		Mat4() : a{1, 0, 0, 0}, b{0, 1, 0, 0}, c{0, 0, 1, 0}, d{0, 0, 0, 1} {}
		Mat4(m::fixed_t x) : a{x, 0, 0, 0}, b{0, x, 0, 0}, c{0, 0, x, 0}, d{0, 0, 0, x} {}
		Mat4(m::fixed_t m00, m::fixed_t m01, m::fixed_t m02, m::fixed_t m03, 
			 m::fixed_t m10, m::fixed_t m11, m::fixed_t m12, m::fixed_t m13, 
			 m::fixed_t m20, m::fixed_t m21, m::fixed_t m22, m::fixed_t m23, 
			 m::fixed_t m30, m::fixed_t m31, m::fixed_t m32, m::fixed_t m33)
		: a{m00, m01, m02, m03}, b{m10, m11, m12, m13}, c{m20, m21, m22, m23}, d{m30, m31, m32, m33} {}

		static Mat4 translate(const Vec3 &by);
		static Mat4 perspective(m::fixed_t aspect, m::fixed_t fov, m::fixed_t zn, m::fixed_t zf);
		static Mat4 rotate(const Vec3& a, m::fixed_t rad);
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

		bool interact();
		constexpr static inline bool require_clearing() {return false;}

		void refresh();
	private:
		void draw_triangle(const Tri& t, bool enable_lighting);
		void update_matricies();

		int current_tri = 0;

		Mat4 perpview;
		Vec3 camera_pos, camera_target;
		Vec3 current_pos, current_look, current_lookdir;
		Vec3 camera_look, camera_look_target;
		uint16_t interp_progress = 20000;
		uint32_t last_update, last_new_data;

		enum InteractMode : uint8_t {
			MOVE_POS,
			MOVE_LOOK
		} im=MOVE_POS;
	};

	extern size_t tri_count;
}

#endif
