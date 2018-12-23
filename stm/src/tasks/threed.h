#include <stdint.h>
#include "sched.h"
#include <string.h>

namespace tasks {
	struct Vec3 {
		float x, y, z;

		Vec3()                          : x(0), y(0), z(0) {}
		Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
		Vec3(float a)                   : x(a), y(a), z(a) {}

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

	struct Renderer : public sched::Task, public sched::Screen {
		void init() override;

		void loop() override;
		bool done() override;
	private:
		void draw_triangle(const Tri& t);
		void update_matricies();

		int current_tri = 0;
		Tri tris[16];

		Mat4 perpview;
	};
}
