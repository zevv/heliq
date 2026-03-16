
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

	// VkFFT
	VkFFTApplication fft_app{};
	VkFFTConfiguration fft_config{};
	uint64_t buf_size{};
	bool fft_initialized{false};

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

	// setup VkFFT (supports up to 3D; rank > 3 handled by CPU fallback in solver.cpp)
	m->fft_config = {};
	m->fft_config.FFTdim = grid.rank;
	for(int i = 0; i < grid.rank; i++)
		m->fft_config.size[i] = grid.axes[i].points;
	m->fft_config.numberBatches = 1;
	m->fft_config.performR2C = 0;
	m->fft_config.doublePrecision = 0;  // float
	m->fft_config.normalize = 1;        // VkFFT normalizes inverse FFT
	m->fft_config.device = &m->device;
	m->fft_config.context = &m->ctx;
	m->fft_config.buffer = &m->d_psi;
	m->buf_size = buf_bytes;
	m->fft_config.bufferSize = &m->buf_size;

	VkFFTResult fft_res = initializeVkFFT(&m->fft_app, m->fft_config);
	if(fft_res != VKFFT_SUCCESS) {
		fprintf(stderr, "VkFFT init failed: %d\n", (int)fft_res);
	} else {
		m->fft_initialized = true;
	}
}


GpuSolver::~GpuSolver()
{
	if(m->fft_initialized)
		deleteVkFFT(&m->fft_app);

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



void GpuSolver::flush()
{
	clFinish(m->queue);
}


void GpuSolver::step()
{
	if(!m->fft_initialized) return;

	VkFFTLaunchParams lp = {};
	lp.commandQueue = &m->queue;
	lp.buffer = &m->d_psi;

	// 1. half-step potential
	run_phase_multiply(m, m->d_potential_phase);

	// 2. FFT forward
	VkFFTResult r1 = VkFFTAppend(&m->fft_app, -1, &lp);
	if(r1 != VKFFT_SUCCESS) fprintf(stderr, "VkFFT forward failed: %d\n", (int)r1);

	// 3. full-step kinetic
	run_phase_multiply(m, m->d_kinetic_phase);

	// 4. FFT inverse
	VkFFTResult r2 = VkFFTAppend(&m->fft_app, 1, &lp);
	if(r2 != VKFFT_SUCCESS) fprintf(stderr, "VkFFT inverse failed: %d\n", (int)r2);
	// 5. half-step potential
	run_phase_multiply(m, m->d_potential_phase);
}


// Convert complex<double> array to interleaved float pairs
static void double_to_float(const std::complex<double> *in, float *out, size_t n)
{
	for(size_t i = 0; i < n; i++) {
		out[i * 2]     = (float)in[i].real();
		out[i * 2 + 1] = (float)in[i].imag();
	}
}

// Convert interleaved float pairs to complex<double>
static void float_to_double(const float *in, std::complex<double> *out, size_t n)
{
	for(size_t i = 0; i < n; i++) {
		out[i] = std::complex<double>(in[i * 2], in[i * 2 + 1]);
	}
}


void GpuSolver::read_psi(std::complex<double> *out) const
{
	size_t buf_bytes = m_total * 2 * sizeof(float);
	std::vector<float> tmp(m_total * 2);
	clEnqueueReadBuffer(m->queue, m->d_psi, CL_TRUE, 0, buf_bytes, tmp.data(), 0, nullptr, nullptr);
	float_to_double(tmp.data(), out, m_total);
}


void GpuSolver::write_psi(const std::complex<double> *in)
{
	size_t buf_bytes = m_total * 2 * sizeof(float);
	std::vector<float> tmp(m_total * 2);
	double_to_float(in, tmp.data(), m_total);
	clEnqueueWriteBuffer(m->queue, m->d_psi, CL_TRUE, 0, buf_bytes, tmp.data(), 0, nullptr, nullptr);
}


void GpuSolver::set_phases(const std::complex<double> *potential_phase,
                            const std::complex<double> *kinetic_phase)
{
	size_t buf_bytes = m_total * 2 * sizeof(float);
	std::vector<float> tmp(m_total * 2);

	double_to_float(potential_phase, tmp.data(), m_total);
	clEnqueueWriteBuffer(m->queue, m->d_potential_phase, CL_TRUE, 0, buf_bytes, tmp.data(), 0, nullptr, nullptr);

	double_to_float(kinetic_phase, tmp.data(), m_total);
	clEnqueueWriteBuffer(m->queue, m->d_kinetic_phase, CL_TRUE, 0, buf_bytes, tmp.data(), 0, nullptr, nullptr);
}
