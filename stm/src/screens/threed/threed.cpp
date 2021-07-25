#include "../../rng.h"
#include "../../srv.h"
#include "../threed.h"
#include "../../draw.h"
#include "../../common/slots.h"
#include "../../tasks/screen.h"
#include "../../fonts/tahoma_9.h"
#include "mesh.h"

extern matrix_type matrix;
extern uint64_t rtc_time;
extern srv::Servicer servicer;
extern tasks::DispMan dispman;

namespace threed {
	constexpr DefaultMeshHolder default_mesh{};

	float Vec3::length() const {
		return sqrtf(
				this->x*this->x +
				this->y*this->y +
				this->z*this->z
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
		return {1.f, 0.f, 0.f, by.x,
				0.f, 1.f, 0.f, by.y,
		        0.f, 0.f, 1.f, by.z,
				0.f, 0.f, 0.f, 1.f};
	}

	Mat4 Mat4::perspective(float aspect, float fov, float zn, float zf) {
		float t = tanf(fov/2.0f) * zn;
		float b = -t;
		float l = b * aspect;
		float r = t * aspect;
		return {(2.f*zn)/(r-l), 0.f, (r+l)/(r-l), 0.f,
				0.f, (2.f*zn)/(t-b), (t+b)/(t-b), 0.f,
				0.f, 0.f, -((zf+zn)/(zf-zn)), -((2.f*zf*zn)/(zf-zn)),
				0.f, 0.f, -1.f, 0.f};
	}

	Mat4 Mat4::rotate(const Vec3 &u, float t) {
		return {cosf(t) + u.x*u.x * (1.f - cosf(t)), u.x*u.y*(1.f-cosf(t) - u.z*sinf(t)), u.x*u.z*(1.f-cosf(t)) + u.y*sinf(t), 0,
				u.y*u.z*(1.f-cosf(t)) + u.z*sinf(t), cosf(t) + u.y*u.y * (1.f - cosf(t)), u.y*u.z*(1.f-cosf(t)) - u.x*sinf(t), 0,
				u.z*u.x*(1.f-cosf(t)) - u.y*sinf(t), u.z*u.y*(1.f-cosf(t)) + u.x*sinf(t), cosf(t) + u.z*u.z * (1.f - cosf(t)), 0,
				0.f,                                 0.f,                                 0.f,                                 1.f
		};
	} 

	Mat4 Mat4::lookat(const Vec3& from, const Vec3& to, const Vec3& up) {
		Vec3 zaxis = (from - to).normalize();
		Vec3 xaxis = zaxis.cross(up).normalize();
		Vec3 yaxis = xaxis.cross(zaxis);

		return {
			xaxis.x , xaxis.y , xaxis.z , -xaxis.dot(from),
			yaxis.x , yaxis.y , yaxis.z , -yaxis.dot(from),
			zaxis.x , zaxis.y , zaxis.z , -zaxis.dot(from),
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

	void Renderer::draw() {
		int tri_count;
		bool enable_lighting = true;
		{
			srv::ServicerLockGuard g(servicer);
			update_matricies();
			current_tri = 0;
			if (!servicer.slot(slots::MODEL_INFO) || servicer.slot<slots::ModelInfo>(slots::MODEL_INFO)->tri_count == 0) tri_count = default_mesh.tri_count;
			else {
				tri_count = servicer.slot<slots::ModelInfo>(slots::MODEL_INFO)->tri_count;
				enable_lighting = servicer.slot<slots::ModelInfo>(slots::MODEL_INFO)->use_lighting;
			}
		}

		servicer.take_lock();
		const auto& x = servicer.slot<Tri*>(slots::MODEL_DATA);
		servicer.give_lock();

		while (current_tri < tri_count) {
			if (x) {
				if (sizeof(Tri)*current_tri < x.datasize) {
					// Lock separately to allow the servicer to respond during a potentially >0.5s frame
					draw_triangle(x.data()[current_tri], enable_lighting);
				}
			}
			else {
				// don't lock for default mesh
				draw_triangle(default_mesh.tris[current_tri], enable_lighting);
			}
			++current_tri;
		}
	}

	void Renderer::update_matricies() {
		interp_progress += (rtc_time - last_update) * 2;
		last_update = rtc_time;

		if (interp_progress > 4500) {
			interp_progress = 0;

			camera_pos = camera_target;
			camera_look = camera_look_target;

			if (!servicer.slot(slots::MODEL_INFO) || servicer.slot<slots::ModelInfo>(slots::MODEL_INFO)->tri_count == 0 || 
				!servicer.slot(slots::MODEL_CAM_MINPOS) || !servicer.slot(slots::MODEL_CAM_MAXPOS)) {
				camera_target = Vec3{
					rng::getrange(-1.0f, 1.0f),
					rng::getrange(0, 1.0f),
					rng::getrange(0, 1.0f)
				};
				if (rng::get() % 2 == 0) {
					camera_look_target = Vec3{0, 0, 0};
				}
				else {
					camera_look_target = Vec3{-0.5, 0.1, 0};
				}
			}
			else {
				camera_target = Vec3{
					rng::getrange(servicer.slot<slots::Vec3>(slots::MODEL_CAM_MINPOS)->x, servicer.slot<slots::Vec3>(slots::MODEL_CAM_MAXPOS)->x),
					rng::getrange(servicer.slot<slots::Vec3>(slots::MODEL_CAM_MINPOS)->y, servicer.slot<slots::Vec3>(slots::MODEL_CAM_MAXPOS)->y),
					rng::getrange(servicer.slot<slots::Vec3>(slots::MODEL_CAM_MINPOS)->z, servicer.slot<slots::Vec3>(slots::MODEL_CAM_MAXPOS)->z)
				};

				int num_focuses = servicer.slot(slots::MODEL_CAM_FOCUS).datasize / sizeof(slots::Vec3);

				camera_look_target = servicer.slot<slots::Vec3 *>(slots::MODEL_CAM_FOCUS)[rng::get() % num_focuses];
			}
		}

		if (!dispman.interacting()) {
			current_pos = camera_pos + (camera_target - camera_pos) * ((float)interp_progress / 4500.0f);
			current_look = camera_look + (camera_look_target - camera_look) * ((float)interp_progress / 4500.0f);
		}

		perpview = Mat4::perspective(2.0f, 1.0f, 0.05f, 20.0f) * Mat4::lookat(current_pos, current_look, {0.f, 1.f, 0.f});

		led::color_t fill(0);
		fill.set_spare(INT16_MAX);

		for (int i = 0; i < matrix_type::framebuffer_type::width; ++i) {
			for (int j = 0; j < matrix_type::framebuffer_type::height; ++j) {
				matrix.get_inactive_buffer().at(i, j) = fill;
			}
		}
	}

	bool Renderer::interact() {
		Vec3 target{0};
		float amt = ui::buttons.held(ui::Buttons::MENU) ? -0.4f : 0.4f;
		amt *= ui::buttons.frame_time();
		amt /= 1000.f;
		if (ui::buttons.held(ui::Buttons::PRV)) target.x += amt;
		if (ui::buttons.held(ui::Buttons::NXT)) target.z += amt;
		if (ui::buttons.held(ui::Buttons::SEL)) target.y += amt;

		Vec3 for_ = (current_look - current_pos).normalize();
		if (im == MOVE_POS) {
			Vec3 left_ = -(Vec3{0.f, 1.f, 0.f}.cross(for_));
			current_pos += for_ * target.x + left_ * target.z;
			current_pos.y += target.y;
			current_look += for_ * target.x + left_ * target.z;
			current_look.y += target.y;
		}
		else {
			for_.y = 0;
			for_ = for_.normalize();
			Vec3 left_ = -(Vec3{0.f, 1.f, 0.f}.cross(for_));
			current_look += for_ * target.x + left_ * target.z;
			current_look.y += target.y;
		}

		if (ui::buttons[ui::Buttons::POWER]) {
			im = (im == MOVE_POS) ? MOVE_LOOK : MOVE_POS;
		}

		// draw label
		draw::text(matrix.get_inactive_buffer(), im == MOVE_POS ? "move" : "look", font::tahoma_9::info, 1, 63, 0x5555ff_cc);
		return false;
	}

	void Renderer::draw_triangle(const Tri& t, bool enable_lighting) {
		Vec4 a = perpview * Vec4(t.p1, 1.f);
		Vec4 b = perpview * Vec4(t.p2, 1.f);
		Vec4 c = perpview * Vec4(t.p3, 1.f);

		a = a / a.w;
		b = b / b.w;
		c = c / c.w;

		a.y = -a.y;
		b.y = -b.y;
		c.y = -c.y;

		Vec3 projnormal = (Vec3{b} - Vec3{a}).cross(Vec3{c} - Vec3{a});
		if (projnormal.z < 0.f) return;

		int16_t ax = round(((a.x + 1.f) / 2.f) * matrix_type::framebuffer_type::width);
		int16_t bx = round(((b.x + 1.f) / 2.f) * matrix_type::framebuffer_type::width);
		int16_t cx = round(((c.x + 1.f) / 2.f) * matrix_type::framebuffer_type::width);

		int16_t ay = round(((a.y + 1.f) / 2.f) * matrix_type::framebuffer_type::height);
		int16_t by = round(((b.y + 1.f) / 2.f) * matrix_type::framebuffer_type::height);
		int16_t cy = round(((c.y + 1.f) / 2.f) * matrix_type::framebuffer_type::height);

		uint16_t cr;
        uint16_t cg;
        uint16_t cb;

		if (enable_lighting) {
			float avg = std::min((t.p1 - current_pos).length(), std::min(
						 (t.p2 - current_pos).length(),
						 (t.p3 - current_pos).length()));
			avg = std::min(0.58f, (avg * avg * 0.25f));
			cr = powf((float)t.r / 255.0f * (1.f - avg), 2.6f) * 4095.f;
			cg = powf((float)t.g / 255.0f * (1.f - avg), 2.6f) * 4095.f;
			cb = powf((float)t.b / 255.0f * (1.f - avg), 2.6f) * 4095.f;
		}
		else {
			cr = draw::cvt(t.r);
			cg = draw::cvt(t.g);
			cb = draw::cvt(t.b);
		}

		if (1.f > a.z && -1.f < a.z && 1.f > b.z && -1.f < b.z)
			line(matrix.get_inactive_buffer(), ax, ay, bx, by, a.z, b.z, cr, cg, cb);
		if (1.f > b.z && -1.f < b.z && 1.f > c.z && -1.f < c.z)
			line(matrix.get_inactive_buffer(), bx, by, cx, cy, b.z, c.z, cr, cg, cb);
		if (1.f > a.z && -1.f < a.z && 1.f > c.z && -1.f < c.z)
			line(matrix.get_inactive_buffer(), cx, cy, ax, ay, c.z, a.z, cr, cg, cb);
	}

	void Renderer::prepare(bool) {
		servicer.set_temperature_all<
			slots::MODEL_CAM_FOCUS,
			slots::MODEL_CAM_MINPOS,
			slots::MODEL_CAM_MAXPOS,
			slots::MODEL_INFO,
			slots::MODEL_DATA
		>(bheap::Block::TemperatureWarm);
	}
	
	Renderer::Renderer() {
		servicer.set_temperature_all<
			slots::MODEL_CAM_FOCUS,
			slots::MODEL_CAM_MINPOS,
			slots::MODEL_CAM_MAXPOS,
			slots::MODEL_INFO,
			slots::MODEL_DATA
		>(bheap::Block::TemperatureHot);
	}

	Renderer::~Renderer() {
		servicer.set_temperature_all<
			slots::MODEL_CAM_FOCUS,
			slots::MODEL_CAM_MINPOS,
			slots::MODEL_CAM_MAXPOS,
			slots::MODEL_INFO,
			slots::MODEL_DATA
		>(bheap::Block::TemperatureCold);
	}
}
