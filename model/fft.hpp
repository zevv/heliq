#pragma once

#include <complex>

class Fft {
public:
	enum Direction { Forward, Inverse };

	struct Plan;

	virtual ~Fft() = default;
	virtual Plan *plan(int rank, const int *dims) = 0;
	virtual void execute(Plan *plan, std::complex<double> *data, Direction dir) = 0;
	virtual void destroy(Plan *plan) = 0;
};
