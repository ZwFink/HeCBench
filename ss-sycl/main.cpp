/**********************************************************************
  Copyright ©2013 Advanced Micro Devices, Inc. All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

  • Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
  • Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************/

#include <iostream>
#include <algorithm>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <chrono>
#include "common.h"
#include "StringSearch.h"

int verify(uint* resultCount, uint workGroupCount, 
    uint* result, uint searchLenPerWG, 
    std::vector<uint> &cpuResults) 
{
  uint count = resultCount[0];
  for(uint i=1; i<workGroupCount; ++i)
  {
    uint found = resultCount[i];
    if(found > 0)
    {
      memcpy((result + count), (result + (i * searchLenPerWG)),
          found * sizeof(uint));
      count += found;
    }
  }
  std::sort(result, result+count);

  std::cout << "Device: found " << count << " times\n"; 

  // compare the results and see if they match
  bool pass = (count == cpuResults.size());
  pass = pass && std::equal (result, result+count, cpuResults.begin());
  if(pass)
  {
    std::cout << "Passed!\n" << std::endl;
    return 0;
  }
  else
  {
    std::cout << "Failed\n" << std::endl;
    return -1;
  }
}

/**
* @brief Compare two strings with specified length
* @param text       start position on text string
* @param pattern    start position on pattern string
* @param length     Length to compare
* @return 0-failure, 1-success
*/
int compare(const uchar* text, const uchar* pattern, uint length)
{
    for(uint l=0; l<length; ++l)
    {
        if (TOLOWER(text[l]) != pattern[l]) return 0;
    }
    return 1;
}

int main(int argc, char* argv[])
{
  if (argc != 4) {
    printf("Usage: %s <path to file> <substring> <repeat>\n", argv[0]);
    return -1;
  }
  std::string file = std::string(argv[1]); // "StringSearch_Input.txt";
  std::string subStr = std::string(argv[2]);
  int iterations = atoi(argv[3]);

  if(iterations < 1)
  {
    std::cout<<"Error, iterations cannot be 0 or negative. Exiting..\n";
    exit(0);
  }

  // Check input text-file specified.
  if(file.length() == 0)
  {
    std::cout << "\n Error: Input File not specified..." << std::endl;
    return -1;
  }

  // Read the content of the file
  std::ifstream textFile(file.c_str(),
      std::ios::in|std::ios::binary|std::ios::ate);
  if(! textFile.is_open())
  {
    std::cout << "\n Unable to open file: " << file << std::endl;
    return -1;
  }

  uint textLength = (uint)(textFile.tellg());
  uchar* text = (uchar*)malloc(textLength+1);
  memset(text, 0, textLength+1);
  textFile.seekg(0, std::ios::beg);
  if (!textFile.read ((char*)text, textLength))
  {
    std::cout << "\n Reading file failed " << std::endl;
    textFile.close();
    return -1;
  }
  textFile.close();

  uint subStrLength = subStr.length();
  if(subStrLength == 0)
  {
    std::cout << "\nError: Sub-String not specified..." << std::endl;
    return -1;
  }

  if (textLength < subStrLength)
  {
    std::cout << "\nText size less than search pattern (" << textLength
      << " < " << subStrLength << ")" << std::endl;
    return -1;
  }

#ifdef ENABLE_2ND_LEVEL_FILTER
  if(subStrLength != 1 && subStrLength <= 16)
  {
    std::cout << "\nSearch pattern size should be longer than 16" << std::endl;
    return -1;
  }
#endif

  std::cout << "Search Pattern : " << subStr << std::endl;

  // Rreference implementation on host device
  std::vector<uint> cpuResults;

  uint last = subStrLength - 1;
  uint badCharSkip[UCHAR_MAX + 1];

  // Initialize the table with default values
  uint scan = 0;
  for(scan = 0; scan <= UCHAR_MAX; ++scan)
  {
    badCharSkip[scan] = subStrLength;
  }

  // populate the table with analysis on pattern
  for(scan = 0; scan < last; ++scan)
  {
    badCharSkip[toupper(subStr[scan])] = last - scan;
    badCharSkip[tolower(subStr[scan])] = last - scan;
  }

  // search the text
  uint curPos = 0;
  while((textLength - curPos) > last)
  {
    int p=last;
    for(scan=(last+curPos); COMPARE(text[scan], subStr[p--]); scan -= 1)
    {
      if (scan == curPos)
      {
        cpuResults.push_back(curPos);
        break;
      }
    }
    curPos += (scan == curPos) ? 1 : badCharSkip[text[last+curPos]];
  }

  std::cout << "CPU: found " << cpuResults.size() << " times\n"; 

#ifdef USE_GPU
  gpu_selector dev_sel;
#else
  cpu_selector dev_sel;
#endif
  queue q(dev_sel);

  buffer<uchar, 1> textBuf (text, textLength);

  // Move subStr data host to device
  const uchar* ss = (const uchar*) subStr.c_str();
  buffer<uchar, 1> subStrBuf (ss, subStrLength);

  uint totalSearchPos = textLength - subStrLength + 1;
  uint searchLenPerWG = SEARCH_BYTES_PER_WORKITEM * LOCAL_SIZE;
  uint workGroupCount = (totalSearchPos + searchLenPerWG - 1) / searchLenPerWG;

  uint* resultCount = (uint*) malloc(workGroupCount * sizeof(uint));
  uint* result = (uint*) malloc((textLength - subStrLength + 1) * sizeof(uint));

  buffer<uint, 1> resultCountBuf (workGroupCount);
  buffer<uint, 1> resultBuf(textLength - subStrLength + 1);

  range<1> lws (LOCAL_SIZE);
  range<1> gws (LOCAL_SIZE * workGroupCount);

  const uint patternLength = subStrLength;
  const uint maxSearchLength = searchLenPerWG;

  double time = 0.0;

/**
* @brief Naive kernel version of string search.
*        Find all pattern positions in the given text
* @param text               Input Text
* @param textLength         Length of the text
* @param pattern            Pattern string
* @param patternLength      Pattern length
* @param resultBuffer       Result of all matched positions
* @param resultCountPerWG   Result counts per Work-Group
* @param maxSearchLength    Maximum search positions for each work-group
* @param localPattern       local buffer for the search pattern
*/
  if(subStrLength == 1)
  {
    std::cout <<
      "\nRun only Naive-Kernel version of String Search for pattern size = 1" <<
      std::endl;
    std::cout << "\nExecuting String search naive for " <<
      iterations << " iterations" << std::endl;

    auto start = std::chrono::steady_clock::now();

    for(int i = 0; i < iterations; i++)
      q.submit([&] (handler &cgh) {
        auto text = textBuf.get_access<sycl_read>(cgh);
        auto pattern = subStrBuf.get_access<sycl_read>(cgh);
        auto resultBuffer = resultBuf.get_access<sycl_discard_write>(cgh);
        auto resultCountPerWG = resultCountBuf.get_access<sycl_discard_write>(cgh);
        accessor<uchar, 1, sycl_read_write, access::target::local> localPattern(subStrLength, cgh);
        accessor<uint, 1, sycl_read_write, access::target::local> groupSuccessCounter(1, cgh);
        cgh.parallel_for<class ss_naive>(nd_range<1>(gws, lws), [=] (nd_item<1> item) {
          int localIdx = item.get_local_id(0);
          int localSize = item.get_local_range(0);
          int groupIdx = item.get_group(0);

          // Last search idx for all work items
          uint lastSearchIdx = textLength - patternLength + 1;

          // global idx for all work items in a WorkGroup
          uint beginSearchIdx = groupIdx * maxSearchLength;
          uint endSearchIdx = beginSearchIdx + maxSearchLength;
          if(beginSearchIdx > lastSearchIdx) return;
          if(endSearchIdx > lastSearchIdx) endSearchIdx = lastSearchIdx;

          // Copy the pattern from global to local buffer
          for(int idx = localIdx; idx < patternLength; idx+=localSize)
          {
            localPattern[idx] = TOLOWER(pattern[idx]);
          }

          if(localIdx == 0) groupSuccessCounter[0] = 0u;
          item.barrier(access::fence_space::local_space);

          auto groupCnt_atomic_ref = ext::oneapi::atomic_ref<uint, 
                                     ext::oneapi::memory_order::relaxed,
                                     ext::oneapi::memory_scope::work_group,
                                     access::address_space::local_space> (groupSuccessCounter[0]);
          // loop over positions in global buffer
          for(uint stringPos=beginSearchIdx+localIdx; stringPos<endSearchIdx; stringPos+=localSize)
          {
            if (compare(text.get_pointer()+stringPos, localPattern.get_pointer(), patternLength) == 1)
            {
              int count = groupCnt_atomic_ref.fetch_add(1u);
              resultBuffer[beginSearchIdx+count] = stringPos;
            }
          }

          item.barrier(access::fence_space::local_space);
          if(localIdx == 0) resultCountPerWG[groupIdx] = groupSuccessCounter[0];
       });
    });

    q.wait();
    auto end = std::chrono::steady_clock::now();
    time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Read Results Count per workGroup
    q.submit([&] (handler &cgh) {
      auto acc = resultCountBuf.get_access<sycl_read>(cgh);
      cgh.copy(acc, resultCount);
    });
    q.submit([&] (handler &cgh) {
      auto acc = resultBuf.get_access<sycl_read>(cgh);
      cgh.copy(acc, result);
    });
    q.wait();

    verify(resultCount, workGroupCount, result, searchLenPerWG, cpuResults); 
  }

  /*
 @param text               Input Text
 @param textLength         Length of the text
 @param pattern            Pattern string
 @param patternLength      Pattern length
 @param resultBuffer       Result of all matched positions
 @param resultCountPerWG   Result counts per Work-Group
 @param maxSearchLength    Maximum search positions for each work-group
 @param localPattern       local buffer for the search pattern
 @param stack1             local stack for store initial 2-byte match 
 @param stack2             local stack for store initial 10-byte match positions
 */
  if(subStrLength > 1) {
    std::cout << "\nExecuting String search with load balance for " <<
      iterations << " iterations" << std::endl;

    auto start = std::chrono::steady_clock::now();

    for(int i = 0; i < iterations; i++)
      q.submit([&] (handler &cgh) {
          auto text = textBuf.get_access<sycl_read>(cgh);
          auto pattern = subStrBuf.get_access<sycl_read>(cgh);
          auto resultBuffer = resultBuf.get_access<sycl_discard_write>(cgh);
          auto resultCountPerWG = resultCountBuf.get_access<sycl_discard_write>(cgh);
          accessor<uchar, 1, sycl_read_write, access::target::local> localPattern(subStrLength, cgh);
          accessor<uint, 1, sycl_read_write, access::target::local> stack1(LOCAL_SIZE * 2, cgh);
          accessor<uint, 1, sycl_read_write, access::target::local> stack2(LOCAL_SIZE * 2, cgh);
          accessor<uint, 1, sycl_read_write, access::target::local> stack1Counter(1, cgh);
          accessor<uint, 1, sycl_read_write, access::target::local> stack2Counter(1, cgh);
          accessor<uint, 1, sycl_read_write, access::target::local> groupSuccessCounter(1, cgh);
          cgh.parallel_for<class ss_loadbalance>(nd_range<1>(gws, lws), [=] (nd_item<1> item) {
            int localIdx = item.get_local_id(0);
            int localSize = item.get_local_range(0);
            int groupIdx = item.get_group(0);
            
            // Initialize the local variaables
            if(localIdx == 0)
            {
                stack1Counter[0] = 0u;
                stack2Counter[0] = 0u;
                groupSuccessCounter[0] = 0u;
            }
            
            // Last search idx for all work items
            uint lastSearchIdx = textLength - patternLength + 1;
            uint stackSize = 0;
        
            // global idx for all work items in a WorkGroup
            uint beginSearchIdx = groupIdx * maxSearchLength;
            uint endSearchIdx = beginSearchIdx + maxSearchLength;
            if(beginSearchIdx > lastSearchIdx) return;
            if(endSearchIdx > lastSearchIdx) endSearchIdx = lastSearchIdx;
            uint searchLength = endSearchIdx - beginSearchIdx;
        
            // Copy the pattern from global to local buffer
            for(uint idx = localIdx; idx < patternLength; idx+=localSize)
            {
                localPattern[idx] = TOLOWER(pattern[idx]);
            }
        
            item.barrier(access::fence_space::local_space);
        
            uchar first = localPattern[0];
            uchar second = localPattern[1];
            int stringPos = localIdx;
            int stackPos = 0;
            int revStackPos = 0;
        
            auto stack1Cnt_atomic_ref = ext::oneapi::atomic_ref<uint, 
                                        ext::oneapi::memory_order::relaxed,
                                        ext::oneapi::memory_scope::work_group,
                                        access::address_space::local_space> (stack1Counter[0]);
            auto stack2Cnt_atomic_ref = ext::oneapi::atomic_ref<uint, 
                                        ext::oneapi::memory_order::relaxed,
                                        ext::oneapi::memory_scope::work_group,
                                        access::address_space::local_space> (stack2Counter[0]);
            auto groupCnt_atomic_ref  = ext::oneapi::atomic_ref<uint, 
                                        ext::oneapi::memory_order::relaxed,
                                        ext::oneapi::memory_scope::work_group,
                                        access::address_space::local_space> (groupSuccessCounter[0]);
            while (true)    // loop over positions in global buffer
            {
        
              // Level-1 : Quick filter on 2 char match and store the good positions on stack1.
                if(stringPos < searchLength)
                {
                    // Queue the initial match positions. Make sure queue has sufficient positions for each work-item.
                    if ((first == TOLOWER(text[beginSearchIdx+stringPos])) && (second == TOLOWER(text[beginSearchIdx+stringPos+1])))
                    {
                        stackPos = stack1Cnt_atomic_ref.fetch_add(1u);
                        stack1[stackPos] = stringPos;
                    }
                }
        
                stringPos += localSize;     // next search idx
        
                item.barrier(access::fence_space::local_space);
                stackSize = stack1Counter[0];
                item.barrier(access::fence_space::local_space);
                
                // continue until stack1 has sufficient good positions for proceed to next Level
                if((stackSize < localSize) && ((((stringPos)/localSize)*localSize) < searchLength)) continue;
        
        #ifdef ENABLE_2ND_LEVEL_FILTER
              // Level-2 : (Processing the stack1 and filling the stack2) For large patterns roll over
              // another 8-bytes from the positions in stack1 and store the match positions in stack2.
                if(localIdx < stackSize)
                {
                    revStackPos = stack1Cnt_atomic_ref.fetch_sub(1u);
                    int pos = stack1[--revStackPos];
                    bool status = (localPattern[2] == TOLOWER(text[beginSearchIdx+pos+2]));
                    status = status && (localPattern[3] == TOLOWER(text[beginSearchIdx+pos+3]));
                    status = status && (localPattern[4] == TOLOWER(text[beginSearchIdx+pos+4]));
                    status = status && (localPattern[5] == TOLOWER(text[beginSearchIdx+pos+5]));
                    status = status && (localPattern[6] == TOLOWER(text[beginSearchIdx+pos+6]));
                    status = status && (localPattern[7] == TOLOWER(text[beginSearchIdx+pos+7]));
                    status = status && (localPattern[8] == TOLOWER(text[beginSearchIdx+pos+8]));
                    status = status && (localPattern[9] == TOLOWER(text[beginSearchIdx+pos+9]));
        
                    if (status)
                    {
                        stackPos = stack2Cnt_atomic_ref.fetch_add(1u);
                        stack2[stackPos] = pos;
                    }
                }
        
                item.barrier(access::fence_space::local_space);
                stackSize = stack2Counter[0];
                item.barrier(access::fence_space::local_space);
        
                // continue until stack2 has sufficient good positions proceed to next level
                if((stackSize < localSize) && ((((stringPos)/localSize)*localSize) < searchLength)) continue;
        #endif
        
        
              // Level-3 : (Processing stack1/stack2) Check the remaining positions.
                if(localIdx < stackSize)
                {
        #ifdef ENABLE_2ND_LEVEL_FILTER
                    revStackPos = stack2Cnt_atomic_ref.fetch_sub(1u);
                    int pos = stack2[--revStackPos];
                    if (compare(text.get_pointer()+beginSearchIdx+pos+10, localPattern.get_pointer()+10, patternLength-10) == 1)
        #else
                    revStackPos = stack1Cnt_atomic_ref.fetch_sub(1u);
                    int pos = stack1[--revStackPos];
                    if (compare(text.get_pointer()+beginSearchIdx+pos+2, localPattern.get_pointer()+2, patternLength-2) == 1)
        #endif
                    {
                        // Full match found
                        int count = groupCnt_atomic_ref.fetch_add(1u);
                        resultBuffer[beginSearchIdx+count] = beginSearchIdx+pos;
                    }
                }
        
                item.barrier(access::fence_space::local_space);
                if((((stringPos/localSize)*localSize) >= searchLength) && 
                   (stack1Counter[0] <= 0) && (stack2Counter[0] <= 0)) break;
            }
        
            if(localIdx == 0) resultCountPerWG[groupIdx] = groupSuccessCounter[0];
          });
      });

    q.wait();
    auto end = std::chrono::steady_clock::now();
    time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Read Results Count per workGroup
    q.submit([&] (handler &cgh) {
      auto acc = resultCountBuf.get_access<sycl_read>(cgh);
      cgh.copy(acc, resultCount);
    });
    q.submit([&] (handler &cgh) {
      auto acc = resultBuf.get_access<sycl_read>(cgh);
      cgh.copy(acc, result);
    });
    q.wait();

    verify(resultCount, workGroupCount, result, searchLenPerWG, cpuResults); 
  }

  printf("Average kernel execution time: %f (us)\n", (time * 1e-3f) / iterations);

  free(text);
  free(result);
  free(resultCount);
  return 0;
}
