#include "threed.h"
#include "draw.h"
#include <cmath>

extern led::Matrix<led::FrameBuffer<64, 32>> matrix;
extern uint64_t rtc_time;

namespace tasks {

	float Vec3::length() const {
		return sqrtf(
				powf(this->x, 2.0f)+
				powf(this->y, 2.0f)+
				powf(this->z, 2.0f)
		);
	}

	Vec3 Vec3::normalize() const {
		return *this / length();
	}

	Vec3 Vec3::cross(const Vec3 & other) const {
		return {this->y * other.z - this->z * other.y,
				this->z * other.x - this->x * other.z,
				this->x * other.y - this->y * other.x,
		};
	}

	float Vec3::dot(const Vec3 & other) const {
		return this->x * other.x + this->y * other.y + this->z * other.z;
	}

    Mat4 Mat4::translate(const Vec3 & by) {
		return {1, 0, 0, by.x,
				0, 1, 0, by.y,
		        0, 0, 1, by.z,
				0, 0, 0, 1};
	}

	Mat4 Mat4::perspective(float aspect, float fov, float zn, float zf) {
		float t = tanf(fov/2.0f) * zn;
		float b = -t;
		float l = b * aspect;
		float r = t * aspect;
		return {(2*zn)/(r-l), 0, (r+l)/(r-l), 0,
				0, (2*zn)/(t-b), (t+b)/(t-b), 0,
				0, 0, -((zf+zn)/(zf-zn)), -((2*zf*zn)/(zf-zn)),
				0, 0, -1, 0};
	}

	Mat4 Mat4::rotate(const Vec3 &u, float t) {
		return {cosf(t) + powf(u.x, 2.0f) * (1 - cosf(t)), u.x*u.y*(1-cosf(t) - u.z*sinf(t)), u.x*u.z*(1-cosf(t)) + u.y*sinf(t), 0,
				u.y*u.z*(1-cosf(t)) + u.z*sinf(t), cosf(t) + powf(u.y, 2.0f) * (1 - cosf(t)), u.y*u.z*(1-cosf(t)) - u.x*sinf(t), 0,
				u.z*u.x*(1-cosf(t)) - u.y*sinf(t), u.z*u.y*(1-cosf(t)) + u.x*sinf(t), cosf(t) + powf(u.z, 2.0f) * (1 - cosf(t)), 0,
				0,                                0,                                   0,                                        1
		};
	} 

	Mat4 Mat4::lookat(const Vec3& from, const Vec3& to, const Vec3& up) {
		Vec3 forward = (from - to).normalize();
		Vec3 right = (-forward.cross(up)).normalize();
		Vec3 real_up = forward.cross(right);

		return {
			right.x, right.y, right.z,       -right.dot  (from),
			up.x, up.y, up.z,                -real_up.dot(from),
			forward.x, forward.y, forward.z, -forward.dot(from),
			0, 0, 0, 1
		};
	}

#define e rhs.a
#define f rhs.b
#define g rhs.c
#define h rhs.d

	Mat4 Mat4::operator* (const Mat4& rhs) const {
		return 
		{
			a[0]*e[0]+a[1]*f[0]+a[2]*g[0]+a[3]*h[0], a[0]*e[1]+a[1]*f[1]+a[2]*g[1]+a[3]*h[1], a[0]*e[2]+a[1]*f[2]+a[2]*g[2]+a[3]*h[2], a[0]*e[3]+a[1]*f[3]+a[2]*g[3]+a[3]*h[3],
			b[0]*e[0]+b[1]*f[0]+b[2]*g[0]+b[3]*h[0], b[0]*e[1]+b[1]*f[1]+b[2]*g[1]+b[3]*h[1], b[0]*e[2]+b[1]*f[2]+b[2]*g[2]+b[3]*h[2], b[0]*e[3]+b[1]*f[3]+b[2]*g[3]+b[3]*h[3],
			c[0]*e[0]+c[1]*f[0]+c[2]*g[0]+c[3]*h[0], c[0]*e[1]+c[1]*f[1]+c[2]*g[1]+c[3]*h[1], c[0]*e[2]+c[1]*f[2]+c[2]*g[2]+c[3]*h[2], c[0]*e[3]+c[1]*f[3]+c[2]*g[3]+c[3]*h[3],
			d[0]*e[0]+d[1]*f[0]+d[2]*g[0]+d[3]*h[0], d[0]*e[1]+d[1]*f[1]+d[2]*g[1]+d[3]*h[1], d[0]*e[2]+d[1]*f[2]+d[2]*g[2]+d[3]*h[2], d[0]*e[3]+d[1]*f[3]+d[2]*g[3]+d[3]*h[3]
		};
	}

#undef e
#undef f
#undef g
#undef h

	Vec4 Mat4::operator*(const Vec4& o) const {
		return {
			a[0] * o.x + a[1] * o.y + a[2] * o.z + a[3] * o.w,
			b[0] * o.x + b[1] * o.y + b[2] * o.z + b[3] * o.w,
			c[0] * o.x + c[1] * o.y + c[2] * o.z + c[3] * o.w,
			d[0] * o.x + d[1] * o.y + d[2] * o.z + d[3] * o.w,
		};
	}

	void Renderer::init() {
		tris[0] = {
			{-1, -1, -1},
			{-1,  1, -1},
			{ 1,  1, -1},
			255,
			255,
			255
		};
		tris[1] = {
			{-1, -1, -1},
			{ 1,  1, -1},
			{ 1, -1, -1},
			255,
			255,
			0
		};
		tris[2] = {
			{-1, -1,  1},
			{-1,  1,  1},
			{ 1,  1,  1},
			0,
			255,
			255
		};
		tris[3] = {
			{-1, -1,  1},
			{ 1,  1,  1},
			{ 1, -1,  1},
			255,
			0,
			255
		};
		tris[4] = {
			{-1, -1, -1},
			{-1, -1,  1},
			{-1,  1,  1},
			0,
			255,
			255
		};
		tris[5] = {
			{-1, -1, -1},
			{-1,  1,  1},
			{-1,  1, -1},
			0,
			255,
			0
		};
		tris[6] = {
			{ 1, -1, -1},
			{ 1, -1,  1},
			{ 1,  1,  1},
			0,
			255,
			255
		};
		tris[7] = {
			{ 1, -1, -1},
			{ 1,  1,  1},
			{ 1,  1, -1},
			0,
			255,
			0
		};
		tris[8] = {
			{-1, -1, -1},
			{ 1, -1, -1},
			{ 1, -1,  1},
			0,
			0,
			255
		};
		tris[9] = {
			{-1, -1, -1},
			{ 1, -1,  1},
			{ 1, -1, -1},
			255,
			0,
			0
		};
		tris[10] = {
			{-1,  1, -1},
			{ 1,  1, -1},
			{ 1,  1,  1},
			127,
			127,
			255
		};
		tris[11] = {
			{-1,  1, -1},
			{ 1,  1,  1},
			{ 1,  1, -1},
			56,
			102,
			214
		};
	}

	bool Renderer::done() {
		return current_tri == 12;
	}

	void Renderer::loop() {
		if (current_tri == 12) current_tri = 0;
		if (current_tri == 0) {
			draw::fill(matrix.get_inactive_buffer(), 0, 0, 0);
			update_matricies();
		}
		draw_triangle(tris[current_tri]);
		++current_tri;
	}

	void Renderer::update_matricies() {
		uint32_t a = rtc_time %= 6000;
		perpview = Mat4::perspective(2.0f, 1.0f, 0.05f, 10.0f) * Mat4::lookat({3, 0, 0}, {0, 0, 0}, {0, 1, 0}) * Mat4::rotate({0, 1, 0}, ((float)a / 6000.0f) * 6.28);
	}

	void Renderer::draw_triangle(const Tri& t) {
		Vec4 a = perpview * Vec4(t.p1, 1.0);
		Vec4 b = perpview * Vec4(t.p2, 1.0);
		Vec4 c = perpview * Vec4(t.p3, 1.0);

		a = a / a.w;
		b = b / b.w;
		c = c / c.w;

		int16_t ax = ((a.x + 1) / 2) * 64;
		int16_t bx = ((b.x + 1) / 2) * 64;
		int16_t cx = ((c.x + 1) / 2) * 64;

	    int16_t ay = ((a.y + 1) / 2) * 32;
		int16_t by = ((b.y + 1) / 2) * 32;
		int16_t cy = ((c.y + 1) / 2) * 32;

		draw::line(matrix.get_inactive_buffer(), ax, ay, bx, by, t.r, t.g, t.b);
		draw::line(matrix.get_inactive_buffer(), bx, by, cx, cy, t.r, t.g, t.b);
		draw::line(matrix.get_inactive_buffer(), cx, cy, ax, ay, t.r, t.g, t.b);
	}

}
