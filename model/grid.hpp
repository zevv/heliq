#pragma once

#include <assert.h>
#include <stddef.h>
#include <complex>

// wavefunction and potential data type — float32 throughout
// (GPU operates in float32; no precision benefit from double on readback)
using psi_t = std::complex<float>;

constexpr int MAX_RANK = 8;

struct Axis {
	int points{};
	double min{};          // meters
	double max{};          // meters
	bool spatial{true};    // false for internal dof (spin)
	char label[8]{};       // e.g. "P1.x", "x"

	double dx() const { return (max - min) / points; }
};

struct Grid {
	int rank{};
	Axis axes[MAX_RANK]{};
private:
	int stride[MAX_RANK]{};
public:

	void compute_strides() {
		stride[rank - 1] = 1;
		for(int i = rank - 2; i >= 0; i--) {
			stride[i] = stride[i + 1] * axes[i + 1].points;
		}
	}

	size_t total_points() const {
		size_t n = 1;
		for(int i = 0; i < rank; i++) n *= axes[i].points;
		return n;
	}

	size_t linear_index(const int *coords) const {
		size_t idx = 0;
		for(int i = 0; i < rank; i++) {
			assert(coords[i] >= 0 && coords[i] < axes[i].points);
			idx += coords[i] * stride[i];
		}
		return idx;
	}

	void coords_from_index(size_t idx, int *coords) const {
		for(int d = 0; d < rank; d++) {
			coords[d] = (int)(idx / stride[d]);
			idx %= stride[d];
		}
	}

	// coordinate-aware iteration over all grid points.
	// callback receives (linear_index, coords[], positions[])
	template<typename F>
	void each(F &&fn) const {
		int coords[MAX_RANK] = {};
		double pos[MAX_RANK];
		size_t total = total_points();
		for(size_t idx = 0; idx < total; idx++) {
			for(int d = 0; d < rank; d++)
				pos[d] = axes[d].min + coords[d] * axes[d].dx();
			fn(idx, coords, pos);
			// increment coordinates, last axis fastest (row-major)
			for(int d = rank - 1; d >= 0; d--) {
				if(++coords[d] < axes[d].points) break;
				coords[d] = 0;
			}
		}
	}

	// --- safe N-dimensional data access ---

	// 1D strided view along one axis, all other axes fixed at cursor
	template<typename T>
	struct AxisView {
		T *base;
		int n;
		size_t stride;

		T& operator[](int i) const { return base[i * stride]; }

		struct Iter {
			T *p; size_t s;
			T& operator*() const { return *p; }
			Iter& operator++() { p += s; return *this; }
			bool operator!=(const Iter &o) const { return p != o.p; }
		};
		Iter begin() const { return {base, stride}; }
		Iter end()   const { return {base + (size_t)n * stride, stride}; }
	};

	// Get a 1D view along `axis`, other axes fixed at cursor[]
	template<typename T>
	AxisView<T> axis_view(int axis, const int *cursor, T *data) const {
		size_t base = 0;
		for(int d = 0; d < rank; d++)
			if(d != axis) base += (size_t)cursor[d] * stride[d];
		return { data + base, axes[axis].points, (size_t)stride[axis] };
	}

	// 2D view over two axes, all other axes fixed at cursor
	template<typename T>
	struct SliceView2D {
		T *base;
		int nx, ny;
		size_t sx, sy;

		T& at(int x, int y) const { return base[x * sx + y * sy]; }

		// iterate all points in the slice: callback(ix, iy, value&)
		template<typename F>
		void each(F &&fn) const {
			for(int x = 0; x < nx; x++)
				for(int y = 0; y < ny; y++)
					fn(x, y, base[x * sx + y * sy]);
		}
	};

	// Get a 2D view over (axis_x, axis_y), other axes fixed at cursor[]
	template<typename T>
	SliceView2D<T> slice_view(int axis_x, int axis_y, const int *cursor, T *data) const {
		size_t base = 0;
		for(int d = 0; d < rank; d++)
			if(d != axis_x && d != axis_y)
				base += (size_t)cursor[d] * stride[d];
		return {
			data + base,
			axes[axis_x].points, axes[axis_y].points,
			(size_t)stride[axis_x], (size_t)stride[axis_y]
		};
	}
};
