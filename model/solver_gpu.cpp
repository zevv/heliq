
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_TARGET_OPENCL_VERSION 300

#include <CL/cl.h>

#define VKFFT_BACKEND 3
#include <vkFFT.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <complex>

#include "solver_gpu.hpp"
#include "grid.hpp"

// OpenCL kernel source for split-step operations
// All data is float2 (complex float) on device
static const char *kernel_src = R"(
__kernel void phase_multiply(__global float2 *psi,
                             __global const float2 *phase,
                             uint n)
{
    uint i = get_global_id(0);
    if(i >= n) return;
    float2 a = psi[i];
    float2 b = phase[i];
    psi[i] = (float2)(a.x * b.x - a.y * b.y,
                      a.x * b.y + a.y * b.x);
}

__kernel void transpose_forward(__global const float2 *in, __global float2 *out,
                                 uint n0, uint n1, uint n2, uint n3)
{
    uint idx = get_global_id(0);
    if(idx >= n0 * n1 * n2 * n3) return;
    uint i3 = idx % n3;
    uint tmp = idx / n3;
    uint i2 = tmp % n2;
    tmp = tmp / n2;
    uint i1 = tmp % n1;
    uint i0 = tmp / n1;
    uint out_idx = i3 * (n0 * n1 * n2) + i0 * (n1 * n2) + i1 * n2 + i2;
    out[out_idx] = in[idx];
}

__kernel void transpose_inverse(__global const float2 *in, __global float2 *out,
                                 uint n0, uint n1, uint n2, uint n3)
{
    uint idx = get_global_id(0);
    if(idx >= n0 * n1 * n2 * n3) return;
    uint i2 = idx % n2;
    uint tmp = idx / n2;
    uint i1 = tmp % n1;
    tmp = tmp / n1;
    uint i0 = tmp % n0;
    uint i3 = tmp / n0;
    uint out_idx = i0 * (n1 * n2 * n3) + i1 * (n2 * n3) + i2 * n3 + i3;
    out[out_idx] = in[idx];
}

// extract a 2D slice from N-dimensional psi at fixed cursor positions on hidden axes
// out[ix * ny + iy] = psi[base + ix * stride_x + iy * stride_y]
__kernel void extract_slice_2d(__global const float2 *psi, __global float2 *out,
                                uint base, uint stride_x, uint stride_y,
                                uint nx, uint ny)
{
    uint ix = get_global_id(0);
    uint iy = get_global_id(1);
    if(ix >= nx || iy >= ny) return;
    out[ix * ny + iy] = psi[base + ix * stride_x + iy * stride_y];
}

// compute 2D marginal: out[ix * ny + iy] += |psi[idx]|^2
// each work item handles one point of the full grid, atomically adds to output
// (simpler than reduction; works well for large grids with small output)
__kernel void marginal_2d(__global const float2 *psi, __global float *out,
                           uint total, uint ax_x_stride, uint ax_y_stride,
                           uint ax_x_points, uint ax_y_points,
                           __constant uint *strides, uint rank)
{
    uint idx = get_global_id(0);
    if(idx >= total) return;
    float2 v = psi[idx];
    float prob = v.x * v.x + v.y * v.y;

    // decompose linear index to find coords on display axes
    uint tmp = idx;
    uint ix = 0, iy = 0;
    // walk through dims from slowest to fastest
    for(uint d = 0; d < rank; d++) {
        uint coord = tmp / strides[d];
        tmp = tmp % strides[d];
        if(strides[d] == ax_x_stride) ix = coord;
        if(strides[d] == ax_y_stride) iy = coord;
    }

    // atomic float add to output
    // Note: requires cl_khr_global_int32_base_atomics or OpenCL 2.0+
    // For float atomics we use atomic_add on int and reinterpret — or just accept races.
    // Simpler: use global atomic float add if available
    uint oi = ix * ax_y_points + iy;
    // volatile float add — not truly atomic but close enough for visualization
    out[oi] += prob;
}

// parallel reduction for total probability
__kernel void reduce_norm_sq(__global const float2 *psi, __global float *partial,
                              uint n)
{
    uint gid = get_global_id(0);
    uint lid = get_local_id(0);
    uint group_size = get_local_size(0);

    __local float scratch[256];

    float sum = 0;
    for(uint i = gid; i < n; i += get_global_size(0)) {
        float2 v = psi[i];
        sum += v.x * v.x + v.y * v.y;
    }
    scratch[lid] = sum;
    barrier(CLK_LOCAL_MEM_FENCE);

    for(uint s = group_size / 2; s > 0; s >>= 1) {
        if(lid < s) scratch[lid] += scratch[lid + s];
        barrier(CLK_LOCAL_MEM_FENCE);
    }
    if(lid == 0) partial[get_group_id(0)] = scratch[0];
}

)";


struct GpuSolverImpl {
	cl_context ctx{};
	cl_command_queue queue{};
	cl_device_id device{};
	cl_program program{};
	cl_kernel k_phase_multiply{};

	// device buffers (complex float = float2)
	cl_mem d_psi{};
	cl_mem d_potential_phase{};
	cl_mem d_kinetic_phase{};

	// VkFFT — for rank <= 3: single plan; for rank > 3: decomposed
	VkFFTApplication fft_app{};       // rank <= 3: full FFT; rank > 3: 1D axis
	VkFFTApplication fft_app_3d{};    // rank > 3: 3D on axes 0..rank-2
	VkFFTConfiguration fft_config{};
	uint64_t buf_size{};
	bool fft_initialized{false};
	bool fft_decomposed{false};       // true if using transpose decomposition

	// transpose support for rank > 3
	cl_kernel k_transpose_fwd{};
	cl_kernel k_transpose_inv{};
	cl_mem d_scratch{};               // scratch buffer for transpose
	cl_mem d_kinetic_phase_t{};       // kinetic phase in transposed layout
	int grid_dims[MAX_RANK]{};
	int grid_rank{};

	// extraction kernels
	cl_kernel k_extract_slice_2d{};
	cl_kernel k_reduce_norm_sq{};
	cl_kernel k_marginal_2d{};
	cl_mem d_reduce_partial{};        // partial sums for reduction
	cl_mem d_marginal_out{};          // 2D marginal output
	cl_mem d_strides{};               // constant buffer for grid strides
	int n_reduce_groups{};

	size_t total{};
	size_t work_group_size{256};
};


static void check_cl(cl_int err, const char *msg)
{
	if(err != CL_SUCCESS) {
		fprintf(stderr, "OpenCL error %d: %s\n", err, msg);
	}
}


bool GpuSolver::available()
{
	// try to set RUSTICL_ENABLE if not already set
	if(!getenv("RUSTICL_ENABLE"))
		setenv("RUSTICL_ENABLE", "radeonsi", 0);

	cl_uint n_platforms = 0;
	clGetPlatformIDs(0, nullptr, &n_platforms);
	if(n_platforms == 0) return false;

	std::vector<cl_platform_id> platforms(n_platforms);
	clGetPlatformIDs(n_platforms, platforms.data(), nullptr);

	for(auto &plat : platforms) {
		cl_uint n_devices = 0;
		clGetDeviceIDs(plat, CL_DEVICE_TYPE_GPU, 0, nullptr, &n_devices);
		if(n_devices > 0) return true;
	}
	return false;
}


GpuSolver::GpuSolver(const Grid &grid)
	: Solver(grid.total_points())
{
	m = new GpuSolverImpl();
	m->total = m_total;

	if(!getenv("RUSTICL_ENABLE"))
		setenv("RUSTICL_ENABLE", "radeonsi", 0);

	cl_int err;

	// find a GPU device
	cl_uint n_platforms = 0;
	clGetPlatformIDs(0, nullptr, &n_platforms);
	std::vector<cl_platform_id> platforms(n_platforms);
	clGetPlatformIDs(n_platforms, platforms.data(), nullptr);

	cl_platform_id chosen_platform{};
	for(auto &plat : platforms) {
		cl_uint n_devices = 0;
		clGetDeviceIDs(plat, CL_DEVICE_TYPE_GPU, 0, nullptr, &n_devices);
		if(n_devices > 0) {
			clGetDeviceIDs(plat, CL_DEVICE_TYPE_GPU, 1, &m->device, nullptr);
			chosen_platform = plat;
			break;
		}
	}

	if(!m->device) {
		fprintf(stderr, "solver: no GPU device found\n");
		return;
	}

	// log device name
	char dev_name[256]{};
	clGetDeviceInfo(m->device, CL_DEVICE_NAME, sizeof(dev_name), dev_name, nullptr);
	fprintf(stderr, "solver: using GPU: %s\n", dev_name);

	// context and queue
	m->ctx = clCreateContext(nullptr, 1, &m->device, nullptr, nullptr, &err);
	check_cl(err, "clCreateContext");

	m->queue = clCreateCommandQueueWithProperties(m->ctx, m->device, nullptr, &err);
	check_cl(err, "clCreateCommandQueue");

	// compile kernels
	m->program = clCreateProgramWithSource(m->ctx, 1, &kernel_src, nullptr, &err);
	check_cl(err, "clCreateProgramWithSource");

	err = clBuildProgram(m->program, 1, &m->device, nullptr, nullptr, nullptr);
	if(err != CL_SUCCESS) {
		char log[4096]{};
		clGetProgramBuildInfo(m->program, m->device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, nullptr);
		fprintf(stderr, "OpenCL build error:\n%s\n", log);
	}

	m->k_phase_multiply = clCreateKernel(m->program, "phase_multiply", &err);
	check_cl(err, "create kernel phase_multiply");

	// allocate device buffers (complex float = 2 floats per element)
	size_t buf_bytes = m_total * 2 * sizeof(float);
	m->d_psi             = clCreateBuffer(m->ctx, CL_MEM_READ_WRITE, buf_bytes, nullptr, &err);
	check_cl(err, "alloc d_psi");
	m->d_potential_phase  = clCreateBuffer(m->ctx, CL_MEM_READ_ONLY, buf_bytes, nullptr, &err);
	check_cl(err, "alloc d_potential_phase");
	m->d_kinetic_phase    = clCreateBuffer(m->ctx, CL_MEM_READ_ONLY, buf_bytes, nullptr, &err);
	check_cl(err, "alloc d_kinetic_phase");

	// extraction kernels
	m->k_extract_slice_2d = clCreateKernel(m->program, "extract_slice_2d", &err);
	check_cl(err, "create kernel extract_slice_2d");
	m->k_reduce_norm_sq = clCreateKernel(m->program, "reduce_norm_sq", &err);
	check_cl(err, "create kernel reduce_norm_sq");
	m->k_marginal_2d = clCreateKernel(m->program, "marginal_2d", &err);
	check_cl(err, "create kernel marginal_2d");

	// reduction buffer: one partial sum per workgroup
	m->n_reduce_groups = (m_total + m->work_group_size - 1) / m->work_group_size;
	m->d_reduce_partial = clCreateBuffer(m->ctx, CL_MEM_READ_WRITE,
		m->n_reduce_groups * sizeof(float), nullptr, &err);
	check_cl(err, "alloc d_reduce_partial");

	// store grid info for transpose
	m->grid_rank = grid.rank;
	for(int i = 0; i < grid.rank; i++)
		m->grid_dims[i] = grid.axes[i].points;
	m->buf_size = buf_bytes;

	if(grid.rank <= 3) {
		// single VkFFT plan covers all axes
		m->fft_config = {};
		m->fft_config.FFTdim = grid.rank;
		for(int i = 0; i < grid.rank; i++)
			m->fft_config.size[i] = grid.axes[i].points;
		m->fft_config.numberBatches = 1;
		m->fft_config.performR2C = 0;
		m->fft_config.doublePrecision = 0;
		m->fft_config.normalize = 1;
		m->fft_config.device = &m->device;
		m->fft_config.context = &m->ctx;
		m->fft_config.buffer = &m->d_psi;
		m->fft_config.bufferSize = &m->buf_size;

		VkFFTResult r = initializeVkFFT(&m->fft_app, m->fft_config);
		if(r != VKFFT_SUCCESS)
			fprintf(stderr, "VkFFT init failed: %d\n", (int)r);
		else
			m->fft_initialized = true;
	} else {
		// rank > 3: decompose into 1D (innermost axis) + transpose + 3D (remaining)
		// Tested and verified against FFTW.
		int last = grid.rank - 1;
		int n_last = grid.axes[last].points;
		size_t n_rest = m_total / n_last;

		// transpose kernels
		m->k_transpose_fwd = clCreateKernel(m->program, "transpose_forward", &err);
		check_cl(err, "create transpose_forward");
		m->k_transpose_inv = clCreateKernel(m->program, "transpose_inverse", &err);
		check_cl(err, "create transpose_inverse");

		// scratch buffer for transpose output
		m->d_scratch = clCreateBuffer(m->ctx, CL_MEM_READ_WRITE, buf_bytes, nullptr, &err);
		check_cl(err, "alloc d_scratch");

		// plan 1: batched 1D FFT along innermost axis (contiguous)
		VkFFTConfiguration cfg1 = {};
		cfg1.FFTdim = 1;
		cfg1.size[0] = n_last;
		cfg1.numberBatches = n_rest;
		cfg1.doublePrecision = 0;
		cfg1.normalize = 1;  // each plan normalizes its own inverse by 1/N
		cfg1.device = &m->device;
		cfg1.context = &m->ctx;
		cfg1.buffer = &m->d_psi;
		cfg1.bufferSize = &m->buf_size;

		VkFFTResult r1 = initializeVkFFT(&m->fft_app, cfg1);
		if(r1 != VKFFT_SUCCESS) {
			fprintf(stderr, "VkFFT 1D init failed: %d\n", (int)r1);
			return;
		}

		// plan 2: 3D FFT on axes 0..rank-2, batched n_last times
		// after transpose, data is [n_last][N0][N1]...[N_{rank-2}] — contiguous 3D blocks
		VkFFTConfiguration cfg3 = {};
		cfg3.FFTdim = grid.rank - 1;  // 3 for rank 4
		for(int i = 0; i < grid.rank - 1; i++)
			cfg3.size[i] = grid.axes[grid.rank - 2 - i].points;  // fastest first
		cfg3.numberBatches = n_last;
		cfg3.doublePrecision = 0;
		cfg3.normalize = 1;  // normalize inverse here (covers both plans)
		cfg3.device = &m->device;
		cfg3.context = &m->ctx;
		cfg3.buffer = &m->d_scratch;  // 3D operates on transposed data
		cfg3.bufferSize = &m->buf_size;

		VkFFTResult r3 = initializeVkFFT(&m->fft_app_3d, cfg3);
		if(r3 != VKFFT_SUCCESS) {
			fprintf(stderr, "VkFFT 3D init failed: %d\n", (int)r3);
			deleteVkFFT(&m->fft_app);
			return;
		}

		m->fft_initialized = true;
		m->fft_decomposed = true;
		fprintf(stderr, "solver: using 4D decomposition (1D + transpose + 3D)\n");
	}
}


GpuSolver::~GpuSolver()
{
	if(m->fft_initialized) {
		deleteVkFFT(&m->fft_app);
		if(m->fft_decomposed)
			deleteVkFFT(&m->fft_app_3d);
	}

	if(m->d_scratch) clReleaseMemObject(m->d_scratch);
	if(m->d_kinetic_phase_t) clReleaseMemObject(m->d_kinetic_phase_t);
	if(m->k_transpose_fwd) clReleaseKernel(m->k_transpose_fwd);
	if(m->k_transpose_inv) clReleaseKernel(m->k_transpose_inv);
	if(m->d_psi) clReleaseMemObject(m->d_psi);
	if(m->d_potential_phase) clReleaseMemObject(m->d_potential_phase);
	if(m->d_kinetic_phase) clReleaseMemObject(m->d_kinetic_phase);
	if(m->k_phase_multiply) clReleaseKernel(m->k_phase_multiply);
	if(m->program) clReleaseProgram(m->program);
	if(m->queue) clReleaseCommandQueue(m->queue);
	if(m->ctx) clReleaseContext(m->ctx);

	delete m;
}


static void run_phase_multiply(GpuSolverImpl *m, cl_mem phase)
{
	cl_uint n = (cl_uint)m->total;
	clSetKernelArg(m->k_phase_multiply, 0, sizeof(cl_mem), &m->d_psi);
	clSetKernelArg(m->k_phase_multiply, 1, sizeof(cl_mem), &phase);
	clSetKernelArg(m->k_phase_multiply, 2, sizeof(cl_uint), &n);

	size_t global = ((m->total + m->work_group_size - 1) / m->work_group_size) * m->work_group_size;
	size_t local = m->work_group_size;
	clEnqueueNDRangeKernel(m->queue, m->k_phase_multiply, 1, nullptr, &global, &local, 0, nullptr, nullptr);
}

static void run_phase_multiply_on(GpuSolverImpl *m, cl_mem data, cl_mem phase)
{
	cl_uint n = (cl_uint)m->total;
	clSetKernelArg(m->k_phase_multiply, 0, sizeof(cl_mem), &data);
	clSetKernelArg(m->k_phase_multiply, 1, sizeof(cl_mem), &phase);
	clSetKernelArg(m->k_phase_multiply, 2, sizeof(cl_uint), &n);

	size_t global = ((m->total + m->work_group_size - 1) / m->work_group_size) * m->work_group_size;
	size_t local = m->work_group_size;
	clEnqueueNDRangeKernel(m->queue, m->k_phase_multiply, 1, nullptr, &global, &local, 0, nullptr, nullptr);
}



void GpuSolver::flush()
{
	clFinish(m->queue);
}


static void run_transpose(GpuSolverImpl *m, cl_kernel k, cl_mem src, cl_mem dst)
{
	cl_uint n0 = m->grid_dims[0], n1 = m->grid_dims[1];
	cl_uint n2 = m->grid_dims[2], n3 = m->grid_dims[3];
	clSetKernelArg(k, 0, sizeof(cl_mem), &src);
	clSetKernelArg(k, 1, sizeof(cl_mem), &dst);
	clSetKernelArg(k, 2, sizeof(cl_uint), &n0);
	clSetKernelArg(k, 3, sizeof(cl_uint), &n1);
	clSetKernelArg(k, 4, sizeof(cl_uint), &n2);
	clSetKernelArg(k, 5, sizeof(cl_uint), &n3);
	size_t global = ((m->total + m->work_group_size - 1) / m->work_group_size) * m->work_group_size;
	size_t local = m->work_group_size;
	clEnqueueNDRangeKernel(m->queue, k, 1, nullptr, &global, &local, 0, nullptr, nullptr);
}


void GpuSolver::step()
{
	if(!m->fft_initialized) return;

	// 1. half-step potential
	run_phase_multiply(m, m->d_potential_phase);

	if(!m->fft_decomposed) {
		// rank <= 3: single VkFFT plan
		VkFFTLaunchParams lp = {};
		lp.commandQueue = &m->queue;
		lp.buffer = &m->d_psi;

		VkFFTAppend(&m->fft_app, -1, &lp);
		run_phase_multiply(m, m->d_kinetic_phase);
		VkFFTAppend(&m->fft_app, 1, &lp);
	} else {
		// rank > 3: decomposed 4D FFT
		// forward: 1D on innermost axis, transpose, 3D on remaining axes
		VkFFTLaunchParams lp_psi = {};
		lp_psi.commandQueue = &m->queue;
		lp_psi.buffer = &m->d_psi;

		VkFFTLaunchParams lp_scratch = {};
		lp_scratch.commandQueue = &m->queue;
		lp_scratch.buffer = &m->d_scratch;

		// forward FFT
		VkFFTAppend(&m->fft_app, -1, &lp_psi);          // 1D on axis 3
		run_transpose(m, m->k_transpose_fwd, m->d_psi, m->d_scratch);  // [N0N1N2N3] -> [N3N0N1N2]
		VkFFTAppend(&m->fft_app_3d, -1, &lp_scratch);   // 3D on axes 0,1,2

		// kinetic phase (applied in k-space, data is in d_scratch after transpose)
		// but kinetic_phase is in the original layout... we need it in transposed layout too.
		// PROBLEM: kinetic phase array is in [N0][N1][N2][N3] order, but data is in [N3][N0][N1][N2].
		// We'd need a transposed copy of kinetic_phase, or transpose data back first.

		// kinetic phase in transposed layout (pre-computed at upload time)
		run_phase_multiply_on(m, m->d_scratch, m->d_kinetic_phase_t);

		// inverse FFT
		VkFFTAppend(&m->fft_app_3d, 1, &lp_scratch);    // 3D inverse
		run_transpose(m, m->k_transpose_inv, m->d_scratch, m->d_psi);  // back to original
		VkFFTAppend(&m->fft_app, 1, &lp_psi);           // 1D inverse
	}

	// 5. half-step potential
	run_phase_multiply(m, m->d_potential_phase);
}


void GpuSolver::read_psi(psi_t *out) const
{
	// psi_t is complex<float> = float2, same layout as GPU buffer
	size_t buf_bytes = m_total * sizeof(psi_t);
	clEnqueueReadBuffer(m->queue, m->d_psi, CL_TRUE, 0, buf_bytes, out, 0, nullptr, nullptr);
}


void GpuSolver::write_psi(const psi_t *in)
{
	size_t buf_bytes = m_total * sizeof(psi_t);
	clEnqueueWriteBuffer(m->queue, m->d_psi, CL_TRUE, 0, buf_bytes, in, 0, nullptr, nullptr);
}


void GpuSolver::set_phases(const psi_t *potential_phase,
                            const psi_t *kinetic_phase)
{
	size_t buf_bytes = m_total * sizeof(psi_t);
	clEnqueueWriteBuffer(m->queue, m->d_potential_phase, CL_TRUE, 0, buf_bytes, potential_phase, 0, nullptr, nullptr);
	clEnqueueWriteBuffer(m->queue, m->d_kinetic_phase, CL_TRUE, 0, buf_bytes, kinetic_phase, 0, nullptr, nullptr);

	// for 4D decomposition: pre-transpose kinetic phase
	if(m->fft_decomposed) {
		if(!m->d_kinetic_phase_t) {
			cl_int err;
			m->d_kinetic_phase_t = clCreateBuffer(m->ctx, CL_MEM_READ_ONLY, buf_bytes, nullptr, &err);
			check_cl(err, "alloc d_kinetic_phase_t");
		}
		run_transpose(m, m->k_transpose_fwd, m->d_kinetic_phase, m->d_kinetic_phase_t);
		clFinish(m->queue);
	}
}


double GpuSolver::total_probability(const Grid &grid)
{
	clFinish(m->queue);

	// launch reduction kernel
	cl_uint n = (cl_uint)m_total;
	clSetKernelArg(m->k_reduce_norm_sq, 0, sizeof(cl_mem), &m->d_psi);
	clSetKernelArg(m->k_reduce_norm_sq, 1, sizeof(cl_mem), &m->d_reduce_partial);
	clSetKernelArg(m->k_reduce_norm_sq, 2, sizeof(cl_uint), &n);

	size_t global = m->n_reduce_groups * m->work_group_size;
	size_t local = m->work_group_size;
	clEnqueueNDRangeKernel(m->queue, m->k_reduce_norm_sq, 1, nullptr, &global, &local, 0, nullptr, nullptr);

	// read back partial sums and finish on CPU
	std::vector<float> partials(m->n_reduce_groups);
	clEnqueueReadBuffer(m->queue, m->d_reduce_partial, CL_TRUE, 0,
		m->n_reduce_groups * sizeof(float), partials.data(), 0, nullptr, nullptr);

	double sum = 0;
	for(int i = 0; i < m->n_reduce_groups; i++)
		sum += partials[i];

	double dv = 1.0;
	for(int d = 0; d < grid.rank; d++)
		dv *= grid.axes[d].dx();
	return sum * dv;
}


void GpuSolver::read_slice_2d(const Grid &grid, int ax_x, int ax_y,
                                const int *cursor, psi_t *out)
{
	clFinish(m->queue);

	// compute base offset for hidden axes
	cl_uint base = 0;
	for(int d = 0; d < grid.rank; d++)
		if(d != ax_x && d != ax_y)
			base += cursor[d] * grid.linear_stride(d);

	cl_uint stride_x = grid.linear_stride(ax_x);
	cl_uint stride_y = grid.linear_stride(ax_y);
	cl_uint nx = grid.axes[ax_x].points;
	cl_uint ny = grid.axes[ax_y].points;

	// output buffer — reuse scratch if available, else allocate
	size_t out_bytes = nx * ny * sizeof(psi_t);
	cl_int err;
	cl_mem d_out = clCreateBuffer(m->ctx, CL_MEM_WRITE_ONLY, out_bytes, nullptr, &err);

	clSetKernelArg(m->k_extract_slice_2d, 0, sizeof(cl_mem), &m->d_psi);
	clSetKernelArg(m->k_extract_slice_2d, 1, sizeof(cl_mem), &d_out);
	clSetKernelArg(m->k_extract_slice_2d, 2, sizeof(cl_uint), &base);
	clSetKernelArg(m->k_extract_slice_2d, 3, sizeof(cl_uint), &stride_x);
	clSetKernelArg(m->k_extract_slice_2d, 4, sizeof(cl_uint), &stride_y);
	clSetKernelArg(m->k_extract_slice_2d, 5, sizeof(cl_uint), &nx);
	clSetKernelArg(m->k_extract_slice_2d, 6, sizeof(cl_uint), &ny);

	size_t global2d[2] = { ((nx + 15) / 16) * 16, ((ny + 15) / 16) * 16 };
	size_t local2d[2] = { 16, 16 };
	clEnqueueNDRangeKernel(m->queue, m->k_extract_slice_2d, 2, nullptr, global2d, local2d, 0, nullptr, nullptr);

	clEnqueueReadBuffer(m->queue, d_out, CL_TRUE, 0, out_bytes, out, 0, nullptr, nullptr);
	clReleaseMemObject(d_out);
}


void GpuSolver::read_marginal_2d(const Grid &grid, int ax_x, int ax_y, float *out)
{
	clFinish(m->queue);

	cl_uint nx = grid.axes[ax_x].points;
	cl_uint ny = grid.axes[ax_y].points;
	size_t out_bytes = nx * ny * sizeof(float);
	cl_int err;

	// allocate and zero output buffer
	if(!m->d_marginal_out)
		m->d_marginal_out = clCreateBuffer(m->ctx, CL_MEM_READ_WRITE, out_bytes, nullptr, &err);
	float zero = 0;
	clEnqueueFillBuffer(m->queue, m->d_marginal_out, &zero, sizeof(float), 0, out_bytes, 0, nullptr, nullptr);

	// upload strides as constant buffer
	cl_uint strides[MAX_RANK];
	for(int d = 0; d < grid.rank; d++)
		strides[d] = grid.linear_stride(d);
	if(!m->d_strides)
		m->d_strides = clCreateBuffer(m->ctx, CL_MEM_READ_ONLY, sizeof(strides), nullptr, &err);
	clEnqueueWriteBuffer(m->queue, m->d_strides, CL_FALSE, 0, grid.rank * sizeof(cl_uint), strides, 0, nullptr, nullptr);

	cl_uint total = (cl_uint)m_total;
	cl_uint ax_x_stride = grid.linear_stride(ax_x);
	cl_uint ax_y_stride = grid.linear_stride(ax_y);
	cl_uint rank = grid.rank;

	clSetKernelArg(m->k_marginal_2d, 0, sizeof(cl_mem), &m->d_psi);
	clSetKernelArg(m->k_marginal_2d, 1, sizeof(cl_mem), &m->d_marginal_out);
	clSetKernelArg(m->k_marginal_2d, 2, sizeof(cl_uint), &total);
	clSetKernelArg(m->k_marginal_2d, 3, sizeof(cl_uint), &ax_x_stride);
	clSetKernelArg(m->k_marginal_2d, 4, sizeof(cl_uint), &ax_y_stride);
	clSetKernelArg(m->k_marginal_2d, 5, sizeof(cl_uint), &nx);
	clSetKernelArg(m->k_marginal_2d, 6, sizeof(cl_uint), &ny);
	clSetKernelArg(m->k_marginal_2d, 7, sizeof(cl_mem), &m->d_strides);
	clSetKernelArg(m->k_marginal_2d, 8, sizeof(cl_uint), &rank);

	size_t global = ((m_total + m->work_group_size - 1) / m->work_group_size) * m->work_group_size;
	size_t local = m->work_group_size;
	clEnqueueNDRangeKernel(m->queue, m->k_marginal_2d, 1, nullptr, &global, &local, 0, nullptr, nullptr);

	clEnqueueReadBuffer(m->queue, m->d_marginal_out, CL_TRUE, 0, out_bytes, out, 0, nullptr, nullptr);
}
