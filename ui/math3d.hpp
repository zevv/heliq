#pragma once

#include <math.h>

struct vec3 {
	double x{}, y{}, z{};

	vec3 operator+(vec3 b) const { return {x+b.x, y+b.y, z+b.z}; }
	vec3 operator-(vec3 b) const { return {x-b.x, y-b.y, z-b.z}; }
	vec3 operator*(double s) const { return {x*s, y*s, z*s}; }
	double length() const { return sqrt(x*x + y*y + z*z); }
	vec3 normalized() const { double l = length(); return {x/l, y/l, z/l}; }
};

struct mat4 {
	double m[16]{};  // column-major

	static mat4 identity() {
		mat4 r{};
		r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0;
		return r;
	}

	static mat4 ortho(double size, double aspect, double near, double far) {
		mat4 r{};
		r.m[0]  = 1.0 / (size * aspect);
		r.m[5]  = 1.0 / size;
		r.m[10] = -2.0 / (far - near);
		r.m[14] = -(far + near) / (far - near);
		r.m[15] = 1.0;
		return r;
	}

	static mat4 perspective(double fov_rad, double aspect, double near, double far) {
		double f = 1.0 / tan(fov_rad * 0.5);
		mat4 r{};
		r.m[0]  = f / aspect;
		r.m[5]  = f;
		r.m[10] = (far + near) / (near - far);
		r.m[11] = -1.0;
		r.m[14] = (2.0 * far * near) / (near - far);
		return r;
	}

	static mat4 look_at(vec3 eye, vec3 center, vec3 up) {
		vec3 f = (center - eye).normalized();
		vec3 s = {f.y*up.z - f.z*up.y, f.z*up.x - f.x*up.z, f.x*up.y - f.y*up.x};
		s = s.normalized();
		vec3 u = {s.y*f.z - s.z*f.y, s.z*f.x - s.x*f.z, s.x*f.y - s.y*f.x};
		mat4 r = identity();
		r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;  r.m[12] = -(s.x*eye.x + s.y*eye.y + s.z*eye.z);
		r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;  r.m[13] = -(u.x*eye.x + u.y*eye.y + u.z*eye.z);
		r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] =  (f.x*eye.x + f.y*eye.y + f.z*eye.z);
		return r;
	}

	static mat4 rotate_x(double a) {
		mat4 r = identity();
		r.m[5] = cos(a);  r.m[9]  = -sin(a);
		r.m[6] = sin(a);  r.m[10] =  cos(a);
		return r;
	}

	static mat4 rotate_y(double a) {
		mat4 r = identity();
		r.m[0] =  cos(a);  r.m[8] = sin(a);
		r.m[2] = -sin(a);  r.m[10] = cos(a);
		return r;
	}

	mat4 operator*(const mat4 &b) const {
		mat4 r{};
		for(int c = 0; c < 4; c++)
			for(int row = 0; row < 4; row++)
				for(int k = 0; k < 4; k++)
					r.m[c*4+row] += m[k*4+row] * b.m[c*4+k];
		return r;
	}

	// transform point, return {x, y, z} after perspective divide
	vec3 transform(vec3 p) const {
		double x = m[0]*p.x + m[4]*p.y + m[8]*p.z  + m[12];
		double y = m[1]*p.x + m[5]*p.y + m[9]*p.z  + m[13];
		double z = m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14];
		double w = m[3]*p.x + m[7]*p.y + m[11]*p.z + m[15];
		if(w != 0) { x /= w; y /= w; z /= w; }
		return {x, y, z};
	}

	mat4 inverse() const {
		const double *a = m;
		mat4 r{};
		double t[6];
		t[0] = a[0]*a[5] - a[1]*a[4];
		t[1] = a[0]*a[6] - a[2]*a[4];
		t[2] = a[0]*a[7] - a[3]*a[4];
		t[3] = a[1]*a[6] - a[2]*a[5];
		t[4] = a[1]*a[7] - a[3]*a[5];
		t[5] = a[2]*a[7] - a[3]*a[6];
		double s[6];
		s[0] = a[8]*a[13] - a[9]*a[12];
		s[1] = a[8]*a[14] - a[10]*a[12];
		s[2] = a[8]*a[15] - a[11]*a[12];
		s[3] = a[9]*a[14] - a[10]*a[13];
		s[4] = a[9]*a[15] - a[11]*a[13];
		s[5] = a[10]*a[15] - a[11]*a[14];
		double det = t[0]*s[5] - t[1]*s[4] + t[2]*s[3] + t[3]*s[2] - t[4]*s[1] + t[5]*s[0];
		if(fabs(det) < 1e-30) return identity();
		double id = 1.0 / det;
		r.m[0]  = ( a[5]*s[5] - a[6]*s[4] + a[7]*s[3]) * id;
		r.m[1]  = (-a[1]*s[5] + a[2]*s[4] - a[3]*s[3]) * id;
		r.m[2]  = ( a[13]*t[5] - a[14]*t[4] + a[15]*t[3]) * id;
		r.m[3]  = (-a[9]*t[5] + a[10]*t[4] - a[11]*t[3]) * id;
		r.m[4]  = (-a[4]*s[5] + a[6]*s[2] - a[7]*s[1]) * id;
		r.m[5]  = ( a[0]*s[5] - a[2]*s[2] + a[3]*s[1]) * id;
		r.m[6]  = (-a[12]*t[5] + a[14]*t[2] - a[15]*t[1]) * id;
		r.m[7]  = ( a[8]*t[5] - a[10]*t[2] + a[11]*t[1]) * id;
		r.m[8]  = ( a[4]*s[4] - a[5]*s[2] + a[7]*s[0]) * id;
		r.m[9]  = (-a[0]*s[4] + a[1]*s[2] - a[3]*s[0]) * id;
		r.m[10] = ( a[12]*t[4] - a[13]*t[2] + a[15]*t[0]) * id;
		r.m[11] = (-a[8]*t[4] + a[9]*t[2] - a[11]*t[0]) * id;
		r.m[12] = (-a[4]*s[3] + a[5]*s[1] - a[6]*s[0]) * id;
		r.m[13] = ( a[0]*s[3] - a[1]*s[1] + a[2]*s[0]) * id;
		r.m[14] = (-a[12]*t[3] + a[13]*t[1] - a[14]*t[0]) * id;
		r.m[15] = ( a[8]*t[3] - a[9]*t[1] + a[10]*t[0]) * id;
		return r;
	}
};
