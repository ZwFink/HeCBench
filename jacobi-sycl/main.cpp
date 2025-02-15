// Copyright (c) 2021, NVIDIA CORPORATION.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <ctime>
#include "common.h"

// A multiple of thread block size
#define N 2048

#define IDX(i, j) ((i) + (j) * N)

void initialize_data (float* f) {
  // Set up simple sinusoidal boundary conditions
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {

      if (i == 0 || i == N-1) {
        f[IDX(i,j)] = sinf(j * 2 * M_PI / (N - 1));
      }
      else if (j == 0 || j == N-1) {
        f[IDX(i,j)] = sinf(i * 2 * M_PI / (N - 1));
      }
      else {
        f[IDX(i,j)] = 0.0f;
      }
    }
  }
}

int main () {
  // Begin wall timing
  std::clock_t start_time = std::clock();

  // Reserve space for the scalar field and the "old" copy of the data
  float* f = (float*) aligned_alloc(64, N * N * sizeof(float));
  float* f_old = (float*) aligned_alloc(64, N * N * sizeof(float));

  // Initialize data (we'll do this on both f and f_old, so that we don't
  // have to worry about the boundary points later)
  initialize_data(f);
  initialize_data(f_old);

#ifdef USE_GPU
  gpu_selector dev_sel;
#else
  cpu_selector dev_sel;
#endif
  queue q(dev_sel);

  buffer<float, 1> d_f (N * N);
  q.submit([&] (handler &cgh) {
    auto acc = d_f.get_access<sycl_discard_write>(cgh);
    cgh.copy(f, acc);
  });

  buffer<float, 1> d_f_old (N * N);
  q.submit([&] (handler &cgh) {
    auto acc = d_f_old.get_access<sycl_discard_write>(cgh);
    cgh.copy(f_old, acc);
  });

  buffer<float, 1> d_error (1);

  // Initialize error to a large number
  float error = std::numeric_limits<float>::max();
  const float tolerance = 1.e-5f;

  // Iterate until we're converged (but set a cap on the maximum number of
  // iterations to avoid any possible hangs)
  const int max_iters = 10000;
  int num_iters = 0;

  range<2> gws (N, N);
  range<2> lws (16, 16);

  q.wait();
  auto start = std::chrono::steady_clock::now();

  while (error > tolerance && num_iters < max_iters) {
    // Initialize error to zero (we'll add to it the following step)
    q.submit([&] (handler &cgh) {
      auto err = d_error.get_access<sycl_discard_write>(cgh);
      cgh.fill(err, 0.f);
    });

    // Perform a Jacobi relaxation step
    q.submit([&] (handler &cgh) {
      auto f = d_f.get_access<sycl_read_write>(cgh);
      auto f_old = d_f.get_access<sycl_read>(cgh);
      auto error = d_error.get_access<sycl_read_write>(cgh);
      accessor<float, 2, sycl_read_write, access::target::local> f_old_tile({18,18}, cgh);
      accessor<float, 1, sycl_read_write, access::target::local> reduction_array(16, cgh);
      cgh.parallel_for<class step>(nd_range<2>(gws, lws), [=] (nd_item<2> item) {
        int i = item.get_global_id(1);
        int j = item.get_global_id(0);

        int tx = item.get_local_id(1);
        int ty = item.get_local_id(0);

        // First read in the "interior" data, one value per thread
        // Note the offset by 1, to reserve space for the "left"/"bottom" halo

        f_old_tile[ty+1][tx+1] = f_old[IDX(i,j)];

        // Now read in the halo data; we'll pick the "closest" thread
        // to each element. When we do this, make sure we don't fall
        // off the end of the global memory array. Note that this
        // code does not fill the corners, as they are not used in
        // this stencil.

        if (tx == 0 && i >= 1) {
          f_old_tile[ty+1][tx+0] = f_old[IDX(i-1,j)];
        }
        if (tx == 15 && i <= N-2) {
          f_old_tile[ty+1][tx+2] = f_old[IDX(i+1,j)];
        }
        if (ty == 0 && j >= 1) {
          f_old_tile[ty+0][tx+1] = f_old[IDX(i,j-1)];
        }
        if (ty == 15 && j <= N-2) {
          f_old_tile[ty+2][tx+1] = f_old[IDX(i,j+1)];
        }

        // Synchronize all threads
        item.barrier(access::fence_space::local_space);

        float err = 0.0f;

        if (j >= 1 && j <= N-2) {
          if (i >= 1 && i <= N-2) {
            // Perform the read from shared memory
            f[IDX(i,j)] = 0.25f * (f_old_tile[ty+1][tx+2] + 
                                   f_old_tile[ty+1][tx+0] + 
                                   f_old_tile[ty+2][tx+1] + 
                                   f_old_tile[ty+0][tx+1]);
            float df = f[IDX(i,j)] - f_old_tile[ty+1][tx+1];
            err = df * df;
          }
        }

        // Sum over threads in the warp
        // For simplicity, we do this outside the above conditional
        // so that all threads participate
        auto sg = item.get_sub_group();
        for (int offset = 8; offset > 0; offset /= 2) {
          err += sg.shuffle_down(err, offset);
        }

        // If we're thread 0 in the warp, update our value to shared memory
        // Note that we're assuming exactly a 16x16 block and that the warp ID
        // is equivalent to ty. For the general case, we would have to
        // write more careful code.
        if (tx == 0) {
          reduction_array[ty] = err;
        }

        // Synchronize the block before reading any values from smem
        item.barrier(access::fence_space::local_space);

        // Using the first warp in the block, reduce over the partial sums
        // in the shared memory array.
        if (ty == 0) {
          err = reduction_array[tx];
          for (int offset = 8; offset > 0; offset /= 2) {
            err += sg.shuffle_down(err, offset);
          }
          if (tx == 0) {
            auto atomic_obj_ref = ext::oneapi::atomic_ref<float, 
                     ext::oneapi::memory_order::relaxed, 
                     ext::oneapi::memory_scope::device, 
                     access::address_space::global_space> (error[0]);
            atomic_obj_ref.fetch_add(err);
          }
        }
      });
    });
      
    // Swap the old data and the new data
    // We're doing this explicitly for pedagogical purposes, even though
    // in this specific application a std::swap would have been OK
    q.submit([&] (handler &cgh) {
      auto f = d_f.get_access<sycl_read>(cgh);
      auto f_old = d_f.get_access<sycl_write>(cgh);
      cgh.parallel_for<class swap_data>(nd_range<2>(gws, lws), [=] (nd_item<2> item) {
        int i = item.get_global_id(1);
        int j = item.get_global_id(0);

        if (j >= 1 && j <= N-2) {
          if (i >= 1 && i <= N-2) {
            f_old[IDX(i,j)] = f[IDX(i,j)];
          }
        }
      });
    });

    q.submit([&] (handler &cgh) {
      auto acc = d_error.get_access<sycl_read>(cgh);
      cgh.copy(acc, &error);
    }).wait();

    // Normalize the L2-norm of the error by the number of data points
    // and then take the square root
    error = sqrtf(error / (N * N));

    // Periodically print out the current error
    if (num_iters % 1000 == 0) {
      std::cout << "Error after iteration " << num_iters << " = " << error << std::endl;
    }

    // Increment the iteration count
    ++num_iters;
  }

  q.wait();
  auto end = std::chrono::steady_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  std::cout << "Average execution time per iteration: " << (time * 1e-9f) / num_iters << " (s)\n";

  // If we took fewer than max_iters steps and the error is below the tolerance,
  // we succeeded. Otherwise, we failed.

  if (error <= tolerance && num_iters < max_iters) {
    std::cout << "PASS" << std::endl;
  }
  else {
    std::cout << "FAIL" << std::endl;
    return -1;
  }

  // CLean up memory allocations
  free(f);
  free(f_old);

  // End wall timing
  double duration = (std::clock() - start_time) / (double) CLOCKS_PER_SEC;
  std::cout << "Total elapsed time: " << std::setprecision(4) << duration << " seconds" << std::endl;

  return 0;
}
