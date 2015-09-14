
// =================================================================================================
// This file is part of the CLBlast project. The project is licensed under Apache Version 2.0. This
// project loosely follows the Google C++ styleguide and uses a tab-size of two spaces and a max-
// width of 100 characters per line.
//
// Author(s):
//   Cedric Nugteren <www.cedricnugteren.nl>
//
// This file implements the Xaxpy class (see the header for information about the class).
//
// =================================================================================================

#include "internal/routines/level1/xaxpy.h"

#include <string>
#include <vector>

namespace clblast {
// =================================================================================================

// Specific implementations to get the memory-type based on a template argument
template <> const Precision Xaxpy<float>::precision_ = Precision::kSingle;
template <> const Precision Xaxpy<double>::precision_ = Precision::kDouble;
template <> const Precision Xaxpy<float2>::precision_ = Precision::kComplexSingle;
template <> const Precision Xaxpy<double2>::precision_ = Precision::kComplexDouble;

// =================================================================================================

// Constructor: forwards to base class constructor
template <typename T>
Xaxpy<T>::Xaxpy(Queue &queue, Event &event):
    Routine<T>(queue, event, "AXPY", {"Xaxpy"}, precision_) {
  source_string_ =
    #include "../../kernels/level1/level1.opencl"
    #include "../../kernels/level1/xaxpy.opencl"
  ;
}

// =================================================================================================

// The main routine
template <typename T>
StatusCode Xaxpy<T>::DoAxpy(const size_t n, const T alpha,
                            const Buffer<T> &x_buffer, const size_t x_offset, const size_t x_inc,
                            const Buffer<T> &y_buffer, const size_t y_offset, const size_t y_inc) {

  // Makes sure all dimensions are larger than zero
  if (n == 0) { return StatusCode::kInvalidDimension; }

  // Tests the vectors for validity
  auto status = TestVectorX(n, x_buffer, x_offset, x_inc, sizeof(T));
  if (ErrorIn(status)) { return status; }
  status = TestVectorY(n, y_buffer, y_offset, y_inc, sizeof(T));
  if (ErrorIn(status)) { return status; }

  // Determines whether or not the fast-version can be used
  bool use_fast_kernel = (x_offset == 0) && (x_inc == 1) &&
                         (y_offset == 0) && (y_inc == 1) &&
                         IsMultiple(n, db_["WGS"]*db_["WPT"]*db_["VW"]);

  // If possible, run the fast-version of the kernel
  auto kernel_name = (use_fast_kernel) ? "XaxpyFast" : "Xaxpy";

  // Retrieves the Xaxpy kernel from the compiled binary
  try {
    auto& program = GetProgramFromCache();
    auto kernel = Kernel(program, kernel_name);

    // Sets the kernel arguments
    if (use_fast_kernel) {
      kernel.SetArgument(0, static_cast<int>(n));
      kernel.SetArgument(1, alpha);
      kernel.SetArgument(2, x_buffer());
      kernel.SetArgument(3, y_buffer());
    }
    else {
      kernel.SetArgument(0, static_cast<int>(n));
      kernel.SetArgument(1, alpha);
      kernel.SetArgument(2, x_buffer());
      kernel.SetArgument(3, static_cast<int>(x_offset));
      kernel.SetArgument(4, static_cast<int>(x_inc));
      kernel.SetArgument(5, y_buffer());
      kernel.SetArgument(6, static_cast<int>(y_offset));
      kernel.SetArgument(7, static_cast<int>(y_inc));
    }

    // Launches the kernel
    if (use_fast_kernel) {
      auto global = std::vector<size_t>{CeilDiv(n, db_["WPT"]*db_["VW"])};
      auto local = std::vector<size_t>{db_["WGS"]};
      status = RunKernel(kernel, global, local);
    }
    else {
      auto n_ceiled = Ceil(n, db_["WGS"]*db_["WPT"]);
      auto global = std::vector<size_t>{n_ceiled/db_["WPT"]};
      auto local = std::vector<size_t>{db_["WGS"]};
      status = RunKernel(kernel, global, local);
    }
    if (ErrorIn(status)) { return status; }

    // Waits for all kernels to finish
    queue_.Finish();

    // Succesfully finished the computation
    return StatusCode::kSuccess;
  } catch (...) { return StatusCode::kInvalidKernel; }
}

// =================================================================================================

// Compiles the templated class
template class Xaxpy<float>;
template class Xaxpy<double>;
template class Xaxpy<float2>;
template class Xaxpy<double2>;

// =================================================================================================
} // namespace clblast
