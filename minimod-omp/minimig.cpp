#include <stdio.h>
#include <time.h>
#include <string.h> 
#include "constants.h"
#include "data_setup.h"

#define R 4
#define NDIM 8

void target_inner_3d_kernel(
    llint nx, llint ny, llint nz,
    llint x3, llint x4, llint y3, llint y4, llint z3, llint z4,
    llint lx, llint ly, llint lz,
    float hdx_2, float hdy_2, float hdz_2,
    float coef0,
    float coefx_1, float coefx_2, float coefx_3, float coefx_4,
    float coefy_1, float coefy_2, float coefy_3, float coefy_4,
    float coefz_1, float coefz_2, float coefz_3, float coefz_4,
    const float *__restrict__ u, float *__restrict__ v, const float *__restrict__ vp,
    const float *__restrict__ phi, const float *__restrict__ eta
    ) {

  for (llint x = x3; x <= x4 - 1; x += NDIM)
    for (llint y = y3; y <= y4 - 1; y += NDIM)
      for (llint z = z3; z <= z4 - 1; z += NDIM) {
        float s_u[NDIM+2*R][NDIM+2*R][NDIM+2*R];
        for (int i = 0; i < NDIM+2*R; i++)
          for (int j = 0; j < NDIM+2*R; j++)
            for (int k = 0; k < NDIM+2*R; k++)
              s_u[i][j][k] = 0;
        for(llint ui = 0; ui < NDIM; ui++)
          for(llint uj = 0; uj < NDIM; uj++)
            for(llint uk = 0; uk < NDIM; uk++) {

              const int z_side = ui / R;
              const int y_side = uj / R;

              const llint sui = ui+R;
              const llint suj = uj+R;
              const llint suk = uk+R;

              const llint i = x + ui;
              const llint j = y + uj;
              const llint k = z + uk;
              s_u[ui+z_side*NDIM][suj][suk] = u[IDX3_l(i+(z_side*2-1)*R,j,k)];
              s_u[sui][uj+y_side*NDIM][suk] = u[IDX3_l(i,j+(y_side*2-1)*R,k)];
              s_u[sui][suj][uk] = u[IDX3_l(i,j,k-R)];
              s_u[sui][suj][uk+NDIM] = u[IDX3_l(i,j,k+R)];
            }

        for(llint ui = 0; ui < NDIM; ui++)
          for(llint uj = 0; uj < NDIM; uj++)
            for(llint uk = 0; uk < NDIM; uk++) {
              const llint sui = ui+R;
              const llint suj = uj+R;
              const llint suk = uk+R;
              const llint i = x + ui;
              const llint j = y + uj;
              const llint k = z + uk;
              if  (i <= x4-1 && j <= y4-1 && k <= z4-1) {
                float lap = coef0 *  s_u[sui][suj][suk] + 
                  coefx_1 * (s_u[sui+1][suj][suk] + s_u[sui-1][suj][suk]) +
                  coefy_1 * (s_u[sui][suj+1][suk] + s_u[sui][suj-1][suk]) +
                  coefz_1 * (s_u[sui][suj][suk+1] + s_u[sui][suj][suk-1]) +
                  coefx_2 * (s_u[sui+2][suj][suk] + s_u[sui-2][suj][suk]) +
                  coefy_2 * (s_u[sui][suj+2][suk] + s_u[sui][suj-2][suk]) +
                  coefz_2 * (s_u[sui][suj][suk+2] + s_u[sui][suj][suk-2]) +
                  coefx_3 * (s_u[sui+3][suj][suk] + s_u[sui-3][suj][suk]) +
                  coefy_3 * (s_u[sui][suj+3][suk] + s_u[sui][suj-3][suk]) +
                  coefz_3 * (s_u[sui][suj][suk+3] + s_u[sui][suj][suk-3]) +
                  coefx_4 * (s_u[sui+4][suj][suk] + s_u[sui-4][suj][suk]) +
                  coefy_4 * (s_u[sui][suj+4][suk] + s_u[sui][suj-4][suk]) +
                  coefz_4 * (s_u[sui][suj][suk+4] + s_u[sui][suj][suk-4]);
                v[IDX3_l(i,j,k)] = 2.f * s_u[sui][suj][suk] + vp[IDX3(i,j,k)] * lap - v[IDX3_l(i,j,k)];
              }
            }
      }
}

void target_pml_3d_kernel(
    llint nx, llint ny, llint nz,
    llint x3, llint x4, llint y3, llint y4, llint z3, llint z4,
    llint lx, llint ly, llint lz,
    float hdx_2, float hdy_2, float hdz_2,
    float coef0,
    float coefx_1, float coefx_2, float coefx_3, float coefx_4,
    float coefy_1, float coefy_2, float coefy_3, float coefy_4,
    float coefz_1, float coefz_2, float coefz_3, float coefz_4,
    const float *__restrict__ u, float *__restrict__ v, const float *__restrict__ vp,
    float *__restrict__ phi, const float *__restrict__ eta
    ) {
  for (llint x = x3; x <= x4 - 1; x += NDIM)
    for (llint y = y3; y <= y4 - 1; y += NDIM)
      for (llint z = z3; z <= z4 - 1; z += NDIM) {
        float s_u[NDIM+2*R][NDIM+2*R][NDIM+2*R];
        for (int i = 0; i < NDIM+2*R; i++)
          for (int j = 0; j < NDIM+2*R; j++)
            for (int k = 0; k < NDIM+2*R; k++)
              s_u[i][j][k] = 0;
        for(llint ui = 0; ui < NDIM; ui++)
          for(llint uj = 0; uj < NDIM; uj++)
            for(llint uk = 0; uk < NDIM; uk++) {

              const int z_side = ui / R;
              const int y_side = uj / R;

              const llint sui = ui+R;
              const llint suj = uj+R;
              const llint suk = uk+R;
              const llint i = x + ui;
              const llint j = y + uj;
              const llint k = z + uk;

              s_u[ui+z_side*NDIM][suj][suk] = u[IDX3_l(i+(z_side*2-1)*R,j,k)];
              s_u[sui][uj+y_side*NDIM][suk] = u[IDX3_l(i,j+(y_side*2-1)*R,k)];
              s_u[sui][suj][uk] = u[IDX3_l(i,j,k-R)];
              s_u[sui][suj][uk+NDIM] = u[IDX3_l(i,j,k+R)];
            }

        for(llint ui = 0; ui < NDIM; ui++)
          for(llint uj = 0; uj < NDIM; uj++)
            for(llint uk = 0; uk < NDIM; uk++) {
              const llint sui = ui+R;
              const llint suj = uj+R;
              const llint suk = uk+R;
              const llint i = x + ui;
              const llint j = y + uj;
              const llint k = z + uk;

              if  (i <= x4-1 && j <= y4-1 && k <= z4-1) {
                float lap = coef0 * s_u[sui][suj][suk] + 
                  coefx_1 * (s_u[sui+1][suj][suk] + s_u[sui-1][suj][suk]) +
                  coefy_1 * (s_u[sui][suj+1][suk] + s_u[sui][suj-1][suk]) +
                  coefz_1 * (s_u[sui][suj][suk+1] + s_u[sui][suj][suk-1]) +
                  coefx_2 * (s_u[sui+2][suj][suk] + s_u[sui-2][suj][suk]) +
                  coefy_2 * (s_u[sui][suj+2][suk] + s_u[sui][suj-2][suk]) +
                  coefz_2 * (s_u[sui][suj][suk+2] + s_u[sui][suj][suk-2]) +
                  coefx_3 * (s_u[sui+3][suj][suk] + s_u[sui-3][suj][suk]) +
                  coefy_3 * (s_u[sui][suj+3][suk] + s_u[sui][suj-3][suk]) +
                  coefz_3 * (s_u[sui][suj][suk+3] + s_u[sui][suj][suk-3]) +
                  coefx_4 * (s_u[sui+4][suj][suk] + s_u[sui-4][suj][suk]) +
                  coefy_4 * (s_u[sui][suj+4][suk] + s_u[sui][suj-4][suk]) +
                  coefz_4 * (s_u[sui][suj][suk+4] + s_u[sui][suj][suk-4]);

                const float s_eta_c = eta[IDX3_eta1(i,j,k)];

                v[IDX3_l(i,j,k)] = ((2.f*s_eta_c + 2.f - s_eta_c*s_eta_c)*s_u[sui][suj][suk] + 
                    (vp[IDX3(i,j,k)] * (lap + phi[IDX3(i,j,k)]) - v[IDX3_l(i,j,k)])) / 
                  (2.f*s_eta_c+1.f);

                phi[IDX3(i,j,k)] = 
                  (phi[IDX3(i,j,k)] - 
                   ((eta[IDX3_eta1(i+1,j,k)]-eta[IDX3_eta1(i-1,j,k)]) * 
                    (s_u[sui+1][suj][suk]-s_u[sui-1][suj][suk]) * hdx_2 + 
                    (eta[IDX3_eta1(i,j+1,k)]-eta[IDX3_eta1(i,j-1,k)]) *
                    (s_u[sui][suj+1][suk]-s_u[sui][suj-1][suk]) * hdy_2 +
                    (eta[IDX3_eta1(i,j,k+1)]-eta[IDX3_eta1(i,j,k-1)]) *
                    (s_u[sui][suj][suk+1]-s_u[sui][suj][suk-1]) * hdz_2)) / (1.f + s_eta_c);
              }
            }
      }
}

void kernel_add_source_kernel(float *__restrict__ g_u, llint idx, float source) {
  g_u[idx] += source;
}

void target(
    uint nsteps, double *time_kernel,
    llint nx, llint ny, llint nz,
    llint x1, llint x2, llint x3, llint x4, llint x5, llint x6,
    llint y1, llint y2, llint y3, llint y4, llint y5, llint y6,
    llint z1, llint z2, llint z3, llint z4, llint z5, llint z6,
    llint lx, llint ly, llint lz,
    llint sx, llint sy, llint sz,
    float hdx_2, float hdy_2, float hdz_2,
    const float *__restrict__ coefx, const float *__restrict__ coefy, const float *__restrict__ coefz,
    float *__restrict__ p, float *__restrict__ q, const float *__restrict__ vp,
    float *__restrict__ phi, const float *__restrict__ eta, const float *__restrict__ source
     ) {
  struct timespec start, end;

  const llint size_u = (nx + 2 * lx) * (ny + 2 * ly) * (nz + 2 * lz);
  const llint size_v = size_u;
  const llint size_phi = nx*ny*nz;
  const llint size_vp = size_phi;
  const llint size_eta = (nx+2)*(ny+2)*(nz+2);

  // copy pointers
  float* u = p;
  float* v = q;

  const llint xmin = 0; 
  const llint ymin = 0;

  llint xmax = nx;
  llint ymax = ny;

  #ifdef DEBUG
  const uint npo = 100;
  #endif

  clock_gettime(CLOCK_REALTIME, &start);

  for (uint istep = 1; istep <= nsteps; ++istep) {

    target_pml_3d_kernel(nx,ny,nz,
        xmin,xmax,ymin,ymax,z1,z2,
        lx,ly,lz,
        hdx_2, hdy_2, hdz_2,
        coefx[0]+coefy[0]+coefz[0],
        coefx[1], coefx[2], coefx[3], coefx[4],
        coefy[1], coefy[2], coefy[3], coefy[4],
        coefz[1], coefz[2], coefz[3], coefz[4],
        u, v, vp,
        phi, eta);

    target_pml_3d_kernel(nx,ny,nz,
        xmin,xmax,y1,y2,z3,z4,
        lx,ly,lz,
        hdx_2, hdy_2, hdz_2,
        coefx[0]+coefy[0]+coefz[0],
        coefx[1], coefx[2], coefx[3], coefx[4],
        coefy[1], coefy[2], coefy[3], coefy[4],
        coefz[1], coefz[2], coefz[3], coefz[4],
        u, v, vp,
        phi, eta);

    target_pml_3d_kernel(nx,ny,nz,
        x1,x2,y3,y4,z3,z4,
        lx,ly,lz,
        hdx_2, hdy_2, hdz_2,
        coefx[0]+coefy[0]+coefz[0],
        coefx[1], coefx[2], coefx[3], coefx[4],
        coefy[1], coefy[2], coefy[3], coefy[4],
        coefz[1], coefz[2], coefz[3], coefz[4],
        u, v, vp,
        phi, eta);

    target_inner_3d_kernel(nx,ny,nz,
        x3,x4,y3,y4,z3,z4,
        lx,ly,lz,
        hdx_2, hdy_2, hdz_2,
        coefx[0]+coefy[0]+coefz[0],
        coefx[1], coefx[2], coefx[3], coefx[4],
        coefy[1], coefy[2], coefy[3], coefy[4],
        coefz[1], coefz[2], coefz[3], coefz[4],
        u, v, vp,
        phi, eta);

    target_pml_3d_kernel(nx,ny,nz,
        x5,x6,y3,y4,z3,z4,
        lx,ly,lz,
        hdx_2, hdy_2, hdz_2,
        coefx[0]+coefy[0]+coefz[0],
        coefx[1], coefx[2], coefx[3], coefx[4],
        coefy[1], coefy[2], coefy[3], coefy[4],
        coefz[1], coefz[2], coefz[3], coefz[4],
        u, v, vp,
        phi, eta);

    target_pml_3d_kernel(nx,ny,nz,
        xmin,xmax,y5,y6,z3,z4,
        lx,ly,lz,
        hdx_2, hdy_2, hdz_2,
        coefx[0]+coefy[0]+coefz[0],
        coefx[1], coefx[2], coefx[3], coefx[4],
        coefy[1], coefy[2], coefy[3], coefy[4],
        coefz[1], coefz[2], coefz[3], coefz[4],
        u, v, vp,
        phi, eta);

    target_pml_3d_kernel(nx,ny,nz,
        xmin,xmax,ymin,ymax,z5,z6,
        lx,ly,lz,
        hdx_2, hdy_2, hdz_2,
        coefx[0]+coefy[0]+coefz[0],
        coefx[1], coefx[2], coefx[3], coefx[4],
        coefy[1], coefy[2], coefy[3], coefy[4],
        coefz[1], coefz[2], coefz[3], coefz[4],
        u, v, vp,
        phi, eta);

    kernel_add_source_kernel(v, IDX3_l(sx,sy,sz), source[istep]);

    float *t = u;
    u = v;
    v = t;

    #ifdef DEBUG
    // Print out
    if (istep % npo == 0) {
      printf("time step %u / %u\n", istep, nsteps);
    }
    #endif
  }

  clock_gettime(CLOCK_REALTIME, &end);
  *time_kernel = (end.tv_sec  - start.tv_sec) +
                 (double)(end.tv_nsec - start.tv_nsec) / 1.0e9;

  if (nsteps % 2 == 1) memcpy(v, u, size_u * sizeof(float));
}
