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

	m::fixed_t Vec3::length() const {
		return m::sqrt(
				this->x*this->x +
				this->y*this->y +
				this->z*this->z
		);
	}

	Vec3 Vec3::normalize() const {
		auto len = length();
		if (len == 0) return *this;
		return *this / len;
	}

	Vec3 Vec3::cross(const Vec3 & other) const {
		return {this->y * other.z - this->z * other.y,
				this->z * other.x - this->x * other.z,
				this->x * other.y - this->y * other.x,
		};
	}

	m::fixed_t Vec3::dot(const Vec3 & other) const {
		return this->x * other.x + this->y * other.y + this->z * other.z;
	}

	Mat4 Mat4::translate(const Vec3 & by) {
		return {1, 0, 0, by.x,
				0, 1, 0, by.y,
		        0, 0, 1, by.z,
				0, 0, 0, 1};
	}

	Mat4 Mat4::perspective(m::fixed_t aspect, m::fixed_t fov, m::fixed_t zn, m::fixed_t zf) {
		m::fixed_t t = m::tan(fov/2) * zn;
		m::fixed_t b = -t;
		m::fixed_t l = b * aspect;
		m::fixed_t r = t * aspect;
		return {(2*zn)/(r-l), 0, (r+l)/(r-l), 0,
				0, (2*zn)/(t-b), (t+b)/(t-b), 0,
				0, 0, -((zf+zn)/(zf-zn)), -((2*zf*zn)/(zf-zn)),
				0, 0, -1, 0};
	}

	Mat4 Mat4::rotate(const Vec3 &u, m::fixed_t t) {
		return {m::cos(t) + u.x*u.x * (1 - m::cos(t)), u.x*u.y*(1-m::cos(t) - u.z*m::sin(t)), u.x*u.z*(1-m::cos(t)) + u.y*m::sin(t), 0,
				u.y*u.z*(1-m::cos(t)) + u.z*m::sin(t), m::cos(t) + u.y*u.y * (1 - m::cos(t)), u.y*u.z*(1-m::cos(t)) - u.x*m::sin(t), 0,
				u.z*u.x*(1-m::cos(t)) - u.y*m::sin(t), u.z*u.y*(1-m::cos(t)) + u.x*m::sin(t), m::cos(t) + u.z*u.z * (1 - m::cos(t)), 0,
				0,                                     0,                                     0,                                     1
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
#ifdef SIM
		return {
			a[0] * o.x + a[1] * o.y + a[2] * o.z + a[3] * o.w,
			b[0] * o.x + b[1] * o.y + b[2] * o.z + b[3] * o.w,
			c[0] * o.x + c[1] * o.y + c[2] * o.z + c[3] * o.w,
			d[0] * o.x + d[1] * o.y + d[2] * o.z + d[3] * o.w,
		};
#else
		Vec4 result;
		int32_t _scratch;

		// Initialize scratch
#define doEntry(structattr, rowname) \
		asm ( \
			"smull %[ResultValue], %[Clobber], %[ROW0], %[OX]\n\t" \
			: [ResultValue] "=r" (result.structattr.value), [Clobber] "=r" (_scratch) \
			: [ROW0] "r" (rowname[0]), [OX] "r" (o.x) \
		); \
		asm ( \
			"smlal %[ResultValue], %[Clobber], %[ROW1], %[OY]\n\t" \
			: [ResultValue] "+r" (result.structattr.value), [Clobber] "+r" (_scratch) \
			: [ROW1] "r" (rowname[1]), [OY] "r" (o.y) \
		); \
		asm ( \
			"smlal %[ResultValue], %[Clobber], %[ROW2], %[OZ]\n\t" \
			: [ResultValue] "+r" (result.structattr.value), [Clobber] "+r" (_scratch) \
			: [ROW2] "r" (rowname[2]), [OZ] "r" (o.z) \
		); \
		asm ( \
			"smlal %[ResultValue], %[Clobber], %[ROW3], %[OW]\n\t" \
			"bfi %[ResultValue], %[Clobber], #0, %[Fac]\n\t" \
			"ror %[ResultValue], %[ResultValue], %[Fac]\n\t" \
			: [ResultValue] "+r" (result.structattr.value), [Clobber] "+r" (_scratch) \
			: [ROW3] "r" (rowname[3]), [OW] "r" (o.w), [Fac] "i" (m::fixed_t::Fac) \
		);

		doEntry(x, a);
		doEntry(y, b);
		doEntry(z, c);
		doEntry(w, d);
#undef doEntry

		return result;
#endif
	}

	void Renderer::draw() {
		int tri_count;
		bool enable_lighting = true;
		{
			srv::ServicerLockGuard g(servicer);
			update_matricies();
			current_lookdir = -(current_look - current_pos).normalize();
			current_tri = 0;
			if (!servicer.slot(slots::MODEL_INFO) || servicer.slot<slots::ModelInfo>(slots::MODEL_INFO)->tri_count == 0) tri_count = default_mesh.tri_count;
			else {
				tri_count = servicer.slot<slots::ModelInfo>(slots::MODEL_INFO)->tri_count;
				enable_lighting = servicer.slot<slots::ModelInfo>(slots::MODEL_INFO)->use_lighting;
			}
		}

		servicer.take_lock();
		const auto& x = servicer.slot<slots::Tri *>(slots::MODEL_DATA);
		servicer.give_lock();

		while (current_tri < tri_count) {
			if (x) {
				if (sizeof(slots::Tri)*current_tri < x.datasize) {
					Tri embiggened; const slots::Tri& shortened = x.data()[current_tri];
					embiggened.r = shortened.r;
					embiggened.g = shortened.g;
					embiggened.b = shortened.b;
					embiggened.p1 = shortened.p1;
					embiggened.p2 = shortened.p2;
					embiggened.p3 = shortened.p3;
					// Lock separately to allow the servicer to respond during a potentially >0.5s frame
					draw_triangle(embiggened, enable_lighting);
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
					m::random(-2, 2),
					m::random(m::fixed_t(1, 2), 2),
					m::random(0, m::fixed_t(3, 2))
				};
				if (rng::get() % 2 == 0) {
					camera_look_target = Vec3{0, 0, 0};
				}
				else {
					camera_look_target = Vec3{-m::fixed_t(1), 0, -m::fixed_t(1)};
				}
			}
			else {
				auto minpos = threed::Vec3(*servicer.slot<slots::Vec3>(slots::MODEL_CAM_MINPOS));
				auto maxpos = threed::Vec3(*servicer.slot<slots::Vec3>(slots::MODEL_CAM_MAXPOS));

				camera_target = Vec3{
					m::random(minpos.x, maxpos.x),
					m::random(minpos.y, maxpos.y),
					m::random(minpos.z, maxpos.z)
				};

				int num_focuses = servicer.slot(slots::MODEL_CAM_FOCUS).datasize / sizeof(slots::Vec3);

				camera_look_target = servicer.slot<slots::Vec3 *>(slots::MODEL_CAM_FOCUS)[rng::get() % num_focuses];
			}
		}

		if (!dispman.interacting()) {
			current_pos = camera_pos + (camera_target - camera_pos) * m::fixed_t(interp_progress, 4500);
			current_look = camera_look + (camera_look_target - camera_look) * m::fixed_t(interp_progress, 4500);
		}

		perpview = Mat4::perspective(2, m::fixed_t(11, 10), m::fixed_t(5, 100), 12) * Mat4::lookat(current_pos, current_look, {0, 1, 0});

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
		m::fixed_t amt = ui::buttons.held(ui::Buttons::MENU) ? -m::fixed_t(4, 10): m::fixed_t(4, 10);
		amt *= ui::buttons.frame_time();
		amt /= 1000;
		if (ui::buttons.held(ui::Buttons::PRV)) target.x += amt;
		if (ui::buttons.held(ui::Buttons::NXT)) target.z += amt;
		if (ui::buttons.held(ui::Buttons::SEL)) target.y += amt;

		Vec3 for_ = (current_look - current_pos).normalize();
		if (im == MOVE_POS) {
			Vec3 left_ = -(Vec3{0, 1, 0}.cross(for_));
			current_pos += for_ * target.x + left_ * target.z;
			current_pos.y += target.y;
			current_look += for_ * target.x + left_ * target.z;
			current_look.y += target.y;
		}
		else {
			for_.y = 0;
			for_ = for_.normalize();
			Vec3 left_ = -(Vec3{0, 1, 0}.cross(for_));
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
		Vec4 a = perpview * Vec4(t.p1, 1);
		Vec4 b = perpview * Vec4(t.p2, 1);
		Vec4 c = perpview * Vec4(t.p3, 1);
		
		if (a.w == 0) return;
		if (b.w == 0) return;
		if (c.w == 0) return;

		a = a / a.w;
		b = b / b.w;
		c = c / c.w;

		a.y = -a.y;
		b.y = -b.y;
		c.y = -c.y;

		if (Vec3(b - a).cross(c - a).z < 0) return;

		int_fast16_t x0 = (((a.x + 1) / 2) * matrix_type::framebuffer_type::width).round();
		int_fast16_t x1 = (((b.x + 1) / 2) * matrix_type::framebuffer_type::width).round();
		int_fast16_t x2 = (((c.x + 1) / 2) * matrix_type::framebuffer_type::width).round();

		int_fast16_t y0 = (((a.y + 1) / 2) * matrix_type::framebuffer_type::height).round();
		int_fast16_t y1 = (((b.y + 1) / 2) * matrix_type::framebuffer_type::height).round();
		int_fast16_t y2 = (((c.y + 1) / 2) * matrix_type::framebuffer_type::height).round();

		uint16_t cr;
        uint16_t cg;
        uint16_t cb;

		if (enable_lighting) {
			// Compute normal
			Vec3 normal = ((t.p2 - t.p1).cross(t.p3 - t.p1)).normalize();
			m::fixed_t facingness = normal.dot(current_lookdir);
			Vec3 centre = (t.p1 + t.p2 + t.p3) / 3;
			m::fixed_t avg = (centre - current_pos).length();
			avg = (1 - std::min(m::fixed_t(1, 4), (avg * avg * m::fixed_t(28, 100))));
			if (facingness < m::fixed_t(1, 4)) facingness = m::fixed_t(1, 4);
			if (facingness > 1) facingness = 1;
			cr = draw::cvt((uint8_t)(m::fixed_t(t.r) * (facingness * avg)).ceil());
			cg = draw::cvt((uint8_t)(m::fixed_t(t.g) * (facingness * avg)).ceil());
			cb = draw::cvt((uint8_t)(m::fixed_t(t.b) * (facingness * avg)).ceil());
		}
		else {
			cr = draw::cvt(t.r);
			cg = draw::cvt(t.g);
			cb = draw::cvt(t.b);
		}

		led::color_t color{cr, cg, cb};

		if (1 > a.z && -1 < a.z && 1 > b.z && -1 < b.z && 1 > c.z && -1 < c.z) {
			// compute bounding box of triangle
			int_fast16_t minx = std::max<int16_t>(0, std::min(std::min(x0, x1), x2));
			int_fast16_t maxx = std::min<int16_t>(matrix.get_inactive_buffer().width-1, std::max(std::max(x0, x1), x2));
			int_fast16_t miny = std::max<int16_t>(0, std::min(std::min(y0, y1), y2));
			int_fast16_t maxy = std::min<int16_t>(matrix.get_inactive_buffer().height-1, std::max(std::max(y0, y1), y2));
			
			// point is inside triangle if barycentric weights are all positive, and barycentric weights can be computed as a delta.

			int w0_dx = y1 - y2, w0_dy = x2 - x1;
			int w1_dx = y2 - y0, w1_dy = x0 - x2;
			int w2_dx = y0 - y1, w2_dy = x1 - x0;

			auto edgeweight = [](int ax, int ay, int bx, int by, int cx, int cy){
				return (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
			};

			int w0_row = edgeweight(x1, y1, x2, y2, minx, miny);
			int w1_row = edgeweight(x2, y2, x0, y0, minx, miny);
			int w2_row = edgeweight(x0, y0, x1, y1, minx, miny);

			for (int_fast16_t y = miny; y <= maxy; ++y) {
				int w0 = w0_row;
				int w1 = w1_row;
				int w2 = w2_row;

				for (int_fast16_t x = minx; x <= maxx; ++x, w0 += w0_dx, w1 += w1_dx, w2 += w2_dx) {
					if ((w0|w1|w2) >= 0) {
						// point is guaranteed to be on screen since min/max are capped in screen.
						// compute depth:

						int sum = (w0 + w1 + w2);
						if (!sum) continue;
						m::fixed_t d = ((a.z * w0) + (b.z * w1) + (c.z * w2)) / sum;
						if ((d * 2047).round() > (int16_t)matrix.get_inactive_buffer().at(x, y).get_spare()) continue; // z-test
						color.set_spare((d * 2047).round());
						matrix.get_inactive_buffer().at(x, y) = color;
					}
				}

				w0_row += w0_dy;
				w1_row += w1_dy;
				w2_row += w2_dy;
			}
		}
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

	void Renderer::refresh() {
		servicer.refresh_grabber(slots::protocol::GrabberID::MODELSERVE);
	}
}
