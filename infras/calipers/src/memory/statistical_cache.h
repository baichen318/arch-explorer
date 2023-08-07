/**
 * Copyright (c) Microsoft Corporation.
 * 
 * MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/* Author: baichen.bai@alibaba-inc.com */


#ifndef STATISTICAL_CACHE_H
#define STATISTICAL_CACHE_H

#include <stdint.h>

#include "calipers_util.h"
#include "cache.h"

/**
 * A statistical/stochastic cache with fixed load/store hit rate and hit/miss cycles
 */
class StatisticalCache: public Cache
{
  private:
    float loadHitRate;
    uint32_t loadHitCycles;
    uint32_t loadMissCycles;

    float storeHitRate;
    uint32_t storeHitCycles;
    uint32_t storeMissCycles;

  public:
    StatisticalCache(string config)
    {
        vector<string> config_vec = split_string(config, ':');
        if (config_vec.size() != 6)
        {
            CALIPERS_ERROR("Invalid configuration for the statistical cache");
        }

        loadHitRate = stof(config_vec[0]);
        loadHitCycles = stoi(config_vec[1]);
        loadMissCycles = stoi(config_vec[2]);

        storeHitRate = stof(config_vec[3]);
        storeHitCycles = stoi(config_vec[4]);
        storeMissCycles = stoi(config_vec[5]);
    }

    uint32_t loadCycles(uint64_t base, uint32_t length)
    {
        int r = rand() % 1000;
        if (r >= 10 * loadHitRate)
        {
            return loadMissCycles;
        }
        else
        {
            return loadHitCycles;
        }
    }

    uint32_t storeCycles(uint64_t base, uint32_t length)
    {
        int r = rand() % 1000;
        if (r >= 10 * storeHitRate)
        {
            return storeMissCycles;
        }
        else
        {
            return storeHitCycles;
        }
    }
};

#endif // STATISTICAL_CACHE_H
