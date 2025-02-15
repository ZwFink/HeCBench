#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <chrono>
#include <omp.h>

void k0 (const float *__restrict a, float *__restrict o, const int n) {
  #pragma omp target teams distribute parallel for thread_limit(256)
  for (int t = 0; t < n; t++) {
    float x = a[t];
    o[t] = coshf(x)/sinhf(x) - 1.f/x;
  }
}

void k1 (const float *__restrict a, float *__restrict o, const int n) {
  #pragma omp target teams distribute parallel for thread_limit(256)
  for (int t = 0; t < n; t++) {
    float x = a[t];
    o[t] = (expf(x) + expf(-x)) / (expf(x) - expf(-x)) - 1.f/x;
  }
}

void k2 (const float *__restrict a, float *__restrict o, const int n) {
  #pragma omp target teams distribute parallel for thread_limit(256)
  for (int t = 0; t < n; t++) {
    float x = a[t];
    o[t] = (expf(2*x) + 1.f) / (expf(2*x) - 1.f) - 1.f/x;
  }
}

void k3 (const float *__restrict a, float *__restrict o, const int n) {
  #pragma omp target teams distribute parallel for thread_limit(256)
  for (int t = 0; t < n; t++) {
    float x = a[t];
    o[t] = 1.f / tanhf(x) - 1.f/x;
  }
}

void k4 (const float *__restrict a, float *__restrict o, const int n) {
  #pragma omp target teams distribute parallel for thread_limit(256)
  for (int t = 0; t < n; t++) {
    float x = a[t];
    float x2 = x * x;
    float x4 = x2 * x2;
    float x6 = x4 * x2;
    o[t] = x * (1.f/3.f - 1.f/45.f * x2 + 2.f/945.f * x4 - 1.f/4725.f * x6);
  }
}

/*
Copyright (c) 2018-2021, Norbert Juffa
  All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright 
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

void k5 (const float *__restrict a, float *__restrict o, const int n) {
  #pragma omp target teams distribute parallel for thread_limit(256)
  for (int t = 0; t < n; t++) {
    float x = a[t];
    float s, r;
    s = x * x;
    r =              7.70960469e-8f;
    r = fmaf (r, s, -1.65101926e-6f);
    r = fmaf (r, s,  2.03457112e-5f);
    r = fmaf (r, s, -2.10521728e-4f);
    r = fmaf (r, s,  2.11580913e-3f);
    r = fmaf (r, s, -2.22220998e-2f);
    r = fmaf (r, s,  8.33333284e-2f);
    r = fmaf (r, x,  0.25f * x);
    o[t] = r;
  }
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage %s <n> <repeat>\n", argv[0]);
    return 1;
  }

  const int n = atoi(argv[1]); 
  const int repeat = atoi(argv[2]); 
  const size_t size = sizeof(float) * n;

  float *a, *o0, *o1, *o2, *o3, *o4, *o5;

  a = (float*) malloc (size);
  // the range [-1.8, -0.00001)
  for (int i = 0; i < n; i++) {
    a[i] = -1.8f + i * (1.79999f / n);
  }

  o0 = (float*) malloc (size);
  o1 = (float*) malloc (size);
  o2 = (float*) malloc (size);
  o3 = (float*) malloc (size);
  o4 = (float*) malloc (size);
  o5 = (float*) malloc (size);

  #pragma omp target data map (to: a[0:n]) \
                          map (from: o0[0:n], o1[0:n], o2[0:n], \
                                     o3[0:n], o4[0:n], o5[0:n])
  {
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; i++) {
      k0(a, o0, n);
    }
    auto end = std::chrono::steady_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("Average execution time of k0: %f (s)\n", (time * 1e-9f) / repeat);

    start = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; i++) {
      k1(a, o1, n);
    }
    end = std::chrono::steady_clock::now();
    time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("Average execution time of k1: %f (s)\n", (time * 1e-9f) / repeat);

    start = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; i++) {
      k2(a, o2, n);
    }
    end = std::chrono::steady_clock::now();
    time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("Average execution time of k2: %f (s)\n", (time * 1e-9f) / repeat);

    start = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; i++) {
      k3(a, o3, n);
    }
    end = std::chrono::steady_clock::now();
    time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("Average execution time of k3: %f (s)\n", (time * 1e-9f) / repeat);

    start = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; i++) {
      k4(a, o4, n);
    }
    end = std::chrono::steady_clock::now();
    time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("Average execution time of k4: %f (s)\n", (time * 1e-9f) / repeat);

    start = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; i++) {
      k5(a, o5, n);
    }
    end = std::chrono::steady_clock::now();
    time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("Average execution time of k5: %f (s)\n", (time * 1e-9f) / repeat);
  }
    
  float s01 = 0, s02 = 0, s03 = 0, s04 = 0, s05 = 0;
  float s12 = 0, s13 = 0, s14 = 0, s15 = 0;
  float s23 = 0, s24 = 0, s25 = 0;
  float s34 = 0, s35 = 0;
  float s45 = 0;

  for (int j = 0; j < n; j++) {
    s01 += (o0[j] - o1[j]) * (o0[j] - o1[j]);
    s02 += (o0[j] - o2[j]) * (o0[j] - o2[j]);
    s03 += (o0[j] - o3[j]) * (o0[j] - o3[j]);
    s04 += (o0[j] - o4[j]) * (o0[j] - o4[j]);
    s05 += (o0[j] - o5[j]) * (o0[j] - o5[j]);

    s12 += (o1[j] - o2[j]) * (o1[j] - o2[j]);
    s13 += (o1[j] - o3[j]) * (o1[j] - o3[j]);
    s14 += (o1[j] - o4[j]) * (o1[j] - o4[j]);
    s15 += (o1[j] - o5[j]) * (o1[j] - o5[j]);

    s23 += (o2[j] - o3[j]) * (o2[j] - o3[j]);
    s24 += (o2[j] - o4[j]) * (o2[j] - o4[j]);
    s25 += (o2[j] - o5[j]) * (o2[j] - o5[j]);

    s34 += (o3[j] - o4[j]) * (o3[j] - o4[j]);
    s35 += (o3[j] - o5[j]) * (o3[j] - o5[j]);

    s45 += (o4[j] - o5[j]) * (o4[j] - o5[j]);
  }

  printf("Squared error statistics for six kernels :\n");
  printf("%f (k0-k1) %f (k0-k2) %f (k0-k3) %f (k0-k4) %f (k0-k5)\n",
         s01, s02, s03, s04, s05);
  printf("%f (k1-k2) %f (k1-k3) %f (k1-k4) %f (k1-k5)\n",
         s12, s13, s14, s15);
  printf("%f (k2-k3) %f (k2-k4) %f (k2-k5)\n", s23, s24, s25);
  printf("%f (k3-k4) %f (k3-k5) \n", s34, s35);
  printf("%f (k4-k5) \n", s45);

  free(a);
  free(o0);
  free(o1);
  free(o2);
  free(o3);
  free(o4);
  free(o5);
  return 0;
}
