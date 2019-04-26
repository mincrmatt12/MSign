#include "rng.h"
#include "threed.h"
#include "draw.h"

extern matrix_type matrix;
extern uint64_t rtc_time;

namespace threed {

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
		Vec3 zaxis = (from - to).normalize();
		Vec3 xaxis = zaxis.cross(up).normalize();
		Vec3 yaxis = xaxis.cross(zaxis);

		return {
			xaxis.x , xaxis.y , xaxis.z , -xaxis.dot (from) ,
			yaxis.x , yaxis.y , yaxis.z , -yaxis.dot(from)  ,
			zaxis.x , zaxis.y , zaxis.z , -zaxis.dot(from)  ,
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

	bool Renderer::done() {
		return current_tri == 206;
	}

	void Renderer::loop() {
		if (current_tri == 206) current_tri = 0;
		if (current_tri == 0) {
			update_matricies();
		}
		draw_triangle(tris[current_tri]);
		++current_tri;
	}

	void Renderer::update_matricies() {
		interp_progress += (rtc_time - last_update) * 2;
		last_update = rtc_time;

		if (interp_progress > 10000) {
			interp_progress = 0;

			camera_pos = camera_target;
			camera_look = camera_look_target;

			camera_target = Vec3{
				((float)(rng::get() % 2000) / 1000.0f) - 1.0f,
				(float)(rng::get() % 1000) / 1000.0f,
				(float)(rng::get() % 1000) / 1000.0f
			};
			camera_look_target = Vec3{
				(float)(rng::get() % 2) - 1.0f,
				(float)(rng::get() % 2) - 1.0f,
				(float)(rng::get() % 2) - 1.0f
			};
		}

		Vec3 current_pos = camera_pos + (camera_target - camera_pos) * ((float)interp_progress / 10000.0f);
		Vec3 current_look = camera_look + (camera_look_target - camera_look) * ((float)interp_progress / 10000.0f);

		perpview = Mat4::perspective(2.0f, 1.0f, 0.05f, 20.0f) * Mat4::lookat(current_pos, {0, 0, 0}, {0, 1, 0});

		for (uint16_t x = 0; x < 64; ++x) {
			for (uint16_t y = 0; y < 32; ++y) {
				z_buf[x][y] = 500.0f;
			}
		}
	}

	void Renderer::draw_triangle(const Tri& t) {
		Vec4 a = perpview * Vec4(t.p1, 1.0);
		Vec4 b = perpview * Vec4(t.p2, 1.0);
		Vec4 c = perpview * Vec4(t.p3, 1.0);

		a = a / a.w;
		b = b / b.w;
		c = c / c.w;

		a.y = -a.y;
		b.y = -b.y;
		c.y = -c.y;

		int16_t ax = round(((a.x + 1) / 2) * 64);
		int16_t bx = round(((b.x + 1) / 2) * 64);
		int16_t cx = round(((c.x + 1) / 2) * 64);

	    int16_t ay = round(((a.y + 1) / 2) * 32);
		int16_t by = round(((b.y + 1) / 2) * 32);
		int16_t cy = round(((c.y + 1) / 2) * 32);

		float avg = (a.z + b.z + c.z) / 3.0f;
		avg = powf((avg + 1) / 2.03, 2.0f);

		uint8_t cr = (float)t.r * (1 - avg);
		uint8_t cg = (float)t.g * (1 - avg);
		uint8_t cb = (float)t.b * (1 - avg);

		if (1 > a.z && -1 < a.z && 1 > b.z && -1 < b.z)
			line(matrix.get_inactive_buffer(), ax, ay, bx, by, a.z, b.z, cr, cg, cb);
		if (1 > b.z && -1 < b.z && 1 > c.z && -1 < c.z)
			line(matrix.get_inactive_buffer(), bx, by, cx, cy, b.z, c.z, cr, cg, cb);
		if (1 > a.z && -1 < a.z && 1 > c.z && -1 < c.z)
			line(matrix.get_inactive_buffer(), cx, cy, ax, ay, c.z, a.z, cr, cg, cb);
	}

}
