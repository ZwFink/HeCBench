#include <chrono>
#include "common.h"
#include "kernel.h"

float fitness_function(float x[])
{
  float res = 0.f;
  float y1 = F(x[0]);
  float yn = F(x[DIM-1]);

  res += sycl::pow(sycl::sin(phi*y1), 2.f) + sycl::pow(yn-1, 2.f);

  for(int i = 0; i < DIM-1; i++)
  {
    float y = F(x[i]);
    float yp = F(x[i+1]);
    res += sycl::pow(y-1.f, 2.f) * (1.f + 10.f * sycl::pow(sycl::sin(phi*yp), 2.f));
  }

  return res;
}

void kernelUpdateParticle(nd_item<1> &item,
                          float *__restrict positions,
                          float *__restrict velocities,
                          const float *__restrict pBests,
                          const float *__restrict gBest,
                          const int p,
                          const float rp,
                          const float rg)
{
  int i=item.get_global_id(0);
  if (i >= p*DIM) return;

  velocities[i]=OMEGA*velocities[i]+
                c1*rp*(pBests[i]-positions[i])+
                c2*rg*(gBest[i%DIM]-positions[i]);
  positions[i]+=velocities[i];
}

void kernelUpdatePBest(nd_item<1> &item,
                       const float *__restrict positions,
                             float *__restrict pBests,
                             float *__restrict gBest,
                       const int p)
{
  int i=item.get_global_id(0);
  if (i >= p) return;
  i = i*DIM;

  float tempParticle1[DIM];
  float tempParticle2[DIM];

  for(int j=0;j<DIM;j++)
  {
    tempParticle1[j]=positions[i+j];
    tempParticle2[j]=pBests[i+j];
  }

  if(fitness_function(tempParticle1)<fitness_function(tempParticle2))
  {
    for(int j=0;j<DIM;j++)
      pBests[i+j]=tempParticle1[j];

    if(fitness_function(tempParticle1)<130.f) //fitness_function(gBest))
    {
      for(int j=0;j<DIM;j++) {
        auto ao = ext::oneapi::atomic_ref<float, 
            ext::oneapi::memory_order::relaxed,
            ext::oneapi::memory_scope::device,
            access::address_space::global_space> (gBest[j]);
        ao.fetch_add(tempParticle1[j]);
      }
    }
  }
}

extern "C" void gpu_pso(int p, int r,
                        float *positions,float *velocities,float *pBests,float *gBest)
{
  int size = p*DIM;
  size_t size_byte = sizeof(float) * size;
  size_t res_size_byte = sizeof(float) * DIM;

#ifdef USE_GPU
  gpu_selector dev_sel;
#else
  cpu_selector dev_sel;
#endif
  queue q(dev_sel, property::queue::in_order());

  float *devPos = malloc_device<float>(size, q);
  float *devVel = malloc_device<float>(size, q);
  float *devPBest = malloc_device<float>(size, q);
  float *devGBest = malloc_device<float>(DIM, q);

  const int threadNum = 256;
  range<1> lws (threadNum);
  range<1> gws1 ((size+threadNum-1)/threadNum*threadNum);
  range<1> gws2 ((p+threadNum-1)/threadNum*threadNum);

  q.memcpy(devPos,positions,size_byte);
  q.memcpy(devVel,velocities,size_byte);
  q.memcpy(devPBest,pBests,size_byte);
  q.memcpy(devGBest,gBest,res_size_byte);

  q.wait();
  auto start = std::chrono::steady_clock::now();

  for(int iter=0;iter<r;iter++)
  {
    float rp=getRandomClamped(iter);
    float rg=getRandomClamped(r-iter);
    q.submit([&] (handler &cgh) {
      cgh.parallel_for<class k1>(nd_range<1>(gws1, lws), [=] (nd_item<1> item) {
        kernelUpdateParticle(item, devPos,devVel,devPBest,devGBest,
                             p,rp,rg);
      });
    });

    q.submit([&] (handler &cgh) {
      cgh.parallel_for<class k2>(nd_range<1>(gws2, lws), [=] (nd_item<1> item) {
        kernelUpdatePBest(item, devPos,devPBest,devGBest,p);
      });
    });
  }

  q.wait();
  auto end = std::chrono::steady_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  printf("Average kernel execution time %f (us)\n", time * 1e-3f / r);
  
  q.memcpy(gBest,devGBest,res_size_byte);
  q.memcpy(pBests,devPBest,size_byte);
  q.wait();

  free(devPos, q);
  free(devVel, q);
  free(devPBest, q);
  free(devGBest, q);
}
