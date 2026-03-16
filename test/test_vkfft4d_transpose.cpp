// Test: 4D FFT via VkFFT using transpose decomposition
//
// 1. VkFFT 1D on axis 3 (innermost, contiguous batches) 
// 2. GPU transpose: [N0][N1][N2][N3] -> [N3][N0][N1][N2]
// 3. VkFFT 3D on [N0][N1][N2], batched N3 (now contiguous batches)
// 4. GPU transpose back: [N3][N0][N1][N2] -> [N0][N1][N2][N3]
//
// Compare result against FFTW 4D reference.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex>
#include <vector>
#include <fftw3.h>
#include <CL/cl.h>
#include <vkFFT.h>

static constexpr int N0 = 8, N1 = 8, N2 = 8, N3 = 8;
static constexpr size_t TOTAL = N0 * N1 * N2 * N3;

// OpenCL transpose kernel: [N0][N1][N2][N3] <-> [N3][N0][N1][N2]
// forward: out[i3 * N0*N1*N2 + i0 * N1*N2 + i1 * N2 + i2] = in[i0 * N1*N2*N3 + i1 * N2*N3 + i2 * N3 + i3]
// inverse: swap src/dst
static const char *transpose_src = R"(
__kernel void transpose_forward(__global const float2 *in, __global float2 *out,
                                 uint n0, uint n1, uint n2, uint n3)
{
    uint idx = get_global_id(0);
    uint total = n0 * n1 * n2 * n3;
    if(idx >= total) return;

    // decompose linear index as [i0][i1][i2][i3]
    uint i3 = idx % n3;
    uint tmp = idx / n3;
    uint i2 = tmp % n2;
    tmp = tmp / n2;
    uint i1 = tmp % n1;
    uint i0 = tmp / n1;

    // write as [i3][i0][i1][i2]
    uint out_idx = i3 * (n0 * n1 * n2) + i0 * (n1 * n2) + i1 * n2 + i2;
    out[out_idx] = in[idx];
}

__kernel void transpose_inverse(__global const float2 *in, __global float2 *out,
                                 uint n0, uint n1, uint n2, uint n3)
{
    uint idx = get_global_id(0);
    uint total = n0 * n1 * n2 * n3;
    if(idx >= total) return;

    // decompose linear index as [i3][i0][i1][i2]
    uint i2 = idx % n2;
    uint tmp = idx / n2;
    uint i1 = tmp % n1;
    tmp = tmp / n1;
    uint i0 = tmp % n0;
    uint i3 = tmp / n0;

    // write as [i0][i1][i2][i3]
    uint out_idx = i0 * (n1 * n2 * n3) + i1 * (n2 * n3) + i2 * n3 + i3;
    out[out_idx] = in[idx];
}
)";

static void check_cl(cl_int err, const char *msg) {
    if(err != CL_SUCCESS) { fprintf(stderr, "CL error %d: %s\n", err, msg); exit(1); }
}

int main()
{
    // --- test data ---
    std::vector<std::complex<double>> data_d(TOTAL);
    std::vector<std::complex<float>> data_f(TOTAL);
    srand(42);
    for(size_t i = 0; i < TOTAL; i++) {
        double re = (double)rand() / RAND_MAX - 0.5;
        double im = (double)rand() / RAND_MAX - 0.5;
        data_d[i] = {re, im};
        data_f[i] = {(float)re, (float)im};
    }

    // --- FFTW reference ---
    std::vector<std::complex<double>> ref(data_d);
    int dims[] = {N0, N1, N2, N3};
    fftw_plan plan = fftw_plan_dft(4, dims,
        (fftw_complex*)ref.data(), (fftw_complex*)ref.data(),
        FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    // --- OpenCL setup ---
    if(!getenv("RUSTICL_ENABLE")) setenv("RUSTICL_ENABLE", "radeonsi", 0);

    cl_int err;
    cl_uint n_plat;
    clGetPlatformIDs(0, nullptr, &n_plat);
    std::vector<cl_platform_id> plats(n_plat);
    clGetPlatformIDs(n_plat, plats.data(), nullptr);

    cl_device_id device{};
    for(auto &p : plats) {
        cl_uint nd;
        clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 0, nullptr, &nd);
        if(nd > 0) { clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &device, nullptr); break; }
    }
    if(!device) { fprintf(stderr, "no GPU\n"); return 1; }

    char name[256];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(name), name, nullptr);
    printf("GPU: %s\n", name);

    cl_context ctx = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
    check_cl(err, "ctx");
    cl_command_queue queue = clCreateCommandQueueWithProperties(ctx, device, nullptr, &err);
    check_cl(err, "queue");

    // compile transpose kernels
    cl_program prog = clCreateProgramWithSource(ctx, 1, &transpose_src, nullptr, &err);
    check_cl(err, "program");
    err = clBuildProgram(prog, 1, &device, nullptr, nullptr, nullptr);
    if(err != CL_SUCCESS) {
        char log[4096];
        clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, nullptr);
        fprintf(stderr, "build: %s\n", log);
        return 1;
    }
    cl_kernel k_fwd = clCreateKernel(prog, "transpose_forward", &err);
    check_cl(err, "k_fwd");
    cl_kernel k_inv = clCreateKernel(prog, "transpose_inverse", &err);
    check_cl(err, "k_inv");

    // device buffers
    size_t buf_bytes = TOTAL * 2 * sizeof(float);
    cl_mem d_a = clCreateBuffer(ctx, CL_MEM_READ_WRITE, buf_bytes, nullptr, &err);
    cl_mem d_b = clCreateBuffer(ctx, CL_MEM_READ_WRITE, buf_bytes, nullptr, &err);

    // upload
    clEnqueueWriteBuffer(queue, d_a, CL_TRUE, 0, buf_bytes, data_f.data(), 0, nullptr, nullptr);

    uint64_t vk_buf_size = buf_bytes;

    // --- Step 1: 1D FFT along axis 3 (innermost, contiguous) ---
    {
        VkFFTConfiguration cfg = {};
        cfg.FFTdim = 1;
        cfg.size[0] = N3;
        cfg.numberBatches = N0 * N1 * N2;
        cfg.doublePrecision = 0;
        cfg.normalize = 0;
        cfg.device = &device;
        cfg.context = &ctx;
        cfg.buffer = &d_a;
        cfg.bufferSize = &vk_buf_size;

        VkFFTApplication app = {};
        VkFFTResult r = initializeVkFFT(&app, cfg);
        if(r != VKFFT_SUCCESS) { fprintf(stderr, "step 1 init: %d\n", (int)r); return 1; }

        VkFFTLaunchParams lp = {};
        lp.commandQueue = &queue;
        lp.buffer = &d_a;
        r = VkFFTAppend(&app, -1, &lp);
        clFinish(queue);
        deleteVkFFT(&app);
        printf("step 1 (1D axis 3): %s\n", r == VKFFT_SUCCESS ? "ok" : "FAIL");
        if(r != VKFFT_SUCCESS) return 1;
    }

    // --- Step 2: transpose [N0][N1][N2][N3] -> [N3][N0][N1][N2] ---
    {
        cl_uint n0=N0, n1=N1, n2=N2, n3=N3;
        clSetKernelArg(k_fwd, 0, sizeof(cl_mem), &d_a);
        clSetKernelArg(k_fwd, 1, sizeof(cl_mem), &d_b);
        clSetKernelArg(k_fwd, 2, sizeof(cl_uint), &n0);
        clSetKernelArg(k_fwd, 3, sizeof(cl_uint), &n1);
        clSetKernelArg(k_fwd, 4, sizeof(cl_uint), &n2);
        clSetKernelArg(k_fwd, 5, sizeof(cl_uint), &n3);
        size_t global = TOTAL;
        clEnqueueNDRangeKernel(queue, k_fwd, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);
        clFinish(queue);
        printf("step 2 (transpose fwd): ok\n");
    }

    // --- Step 3: 3D FFT on [N0][N1][N2], batched N3 times (contiguous) ---
    {
        VkFFTConfiguration cfg = {};
        cfg.FFTdim = 3;
        cfg.size[0] = N2;  // fastest varying in the 3D block
        cfg.size[1] = N1;
        cfg.size[2] = N0;
        cfg.numberBatches = N3;
        cfg.doublePrecision = 0;
        cfg.normalize = 0;
        cfg.device = &device;
        cfg.context = &ctx;
        cfg.buffer = &d_b;  // transposed data
        cfg.bufferSize = &vk_buf_size;

        VkFFTApplication app = {};
        VkFFTResult r = initializeVkFFT(&app, cfg);
        if(r != VKFFT_SUCCESS) { fprintf(stderr, "step 3 init: %d\n", (int)r); return 1; }

        VkFFTLaunchParams lp = {};
        lp.commandQueue = &queue;
        lp.buffer = &d_b;
        r = VkFFTAppend(&app, -1, &lp);
        clFinish(queue);
        deleteVkFFT(&app);
        printf("step 3 (3D axes 0,1,2): %s\n", r == VKFFT_SUCCESS ? "ok" : "FAIL");
        if(r != VKFFT_SUCCESS) return 1;
    }

    // --- Step 4: transpose back [N3][N0][N1][N2] -> [N0][N1][N2][N3] ---
    {
        cl_uint n0=N0, n1=N1, n2=N2, n3=N3;
        clSetKernelArg(k_inv, 0, sizeof(cl_mem), &d_b);
        clSetKernelArg(k_inv, 1, sizeof(cl_mem), &d_a);
        clSetKernelArg(k_inv, 2, sizeof(cl_uint), &n0);
        clSetKernelArg(k_inv, 3, sizeof(cl_uint), &n1);
        clSetKernelArg(k_inv, 4, sizeof(cl_uint), &n2);
        clSetKernelArg(k_inv, 5, sizeof(cl_uint), &n3);
        size_t global = TOTAL;
        clEnqueueNDRangeKernel(queue, k_inv, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);
        clFinish(queue);
        printf("step 4 (transpose inv): ok\n");
    }

    // --- readback and compare ---
    std::vector<std::complex<float>> result(TOTAL);
    clEnqueueReadBuffer(queue, d_a, CL_TRUE, 0, buf_bytes, result.data(), 0, nullptr, nullptr);

    double max_err = 0;
    for(size_t i = 0; i < TOTAL; i++) {
        double dr = fabs((double)result[i].real() - ref[i].real());
        double di = fabs((double)result[i].imag() - ref[i].imag());
        if(dr > max_err) max_err = dr;
        if(di > max_err) max_err = di;
    }
    printf("\nmax error vs FFTW: %e\n", max_err);
    printf("%s\n", max_err < 1e-2 ? "PASS" : "FAIL");

    // cleanup
    clReleaseMemObject(d_a);
    clReleaseMemObject(d_b);
    clReleaseKernel(k_fwd);
    clReleaseKernel(k_inv);
    clReleaseProgram(prog);
    clReleaseCommandQueue(queue);
    clReleaseContext(ctx);

    return (max_err < 1e-2) ? 0 : 1;
}
