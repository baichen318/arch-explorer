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

#ifndef REAL_CACHE_H
#define REAL_CACHE_H

#include <deque>

#include "calipers_util.h"

#define MAX_WAYS 16

const uint64_t HIT = 1;
const uint64_t MISS = 0;

typedef struct CACHE_LINE
{
    uint32_t valid;
    uint32_t dirty;
    uint64_t tag;
} CacheLine; 

typedef struct CACHE_SET
{
    CacheLine line[MAX_WAYS];
} CacheSet;

class MyCache
{
  public:
    uint64_t numSets;
    uint64_t numWays;
    uint64_t lineSize;

    CacheSet* sets;
    CacheLine lastEvictedLine; // For checking write-backs

    // Stats
    uint64_t statReadAccess; 
    uint64_t statWriteAccess; 
    uint64_t statReadMiss; 
    uint64_t statWriteMiss; 
    uint64_t statDirtyEvicts; // How many dirty lines were evicted?

    // PLRU replacement
    uint64_t* plruTree;
    uint64_t plruNumLevels;
    uint64_t plruEffecAssoc;

    // Stack hits
    deque<uint64_t>* lruStack; // Try list as well
    uint64_t lruStackHit[16];

    // Find replacement victim
    // Return value should be 0-15 or 16 (bypass)
    uint64_t getVictimInSet(uint64_t set)
    {
        // PLRU replacement
        uint64_t victim = 0;
        uint64_t index = 0;
        uint64_t bit;

        for (uint64_t i = 0; i < plruNumLevels; ++i)
        {
            bit = (plruTree[set] >> index) & 1;
            victim += bit ? (plruEffecAssoc >> (i + 1)) : 0;
            index = bit ? ((index * 2) + 2) : ((index * 2) + 1);
        }

        victim = (victim > (numWays - 1)) ? (numWays - 1) : victim;

        return victim;       
    }
   
    // Called on every cache hit and cache fill
    void updateReplacementState(uint64_t set, uint64_t way)
    {
        uint64_t index = 0;
        uint64_t bit;
        
        for (int64_t i = plruNumLevels - 1; i >= 0; --i)
        {
            bit = (way >> i) & 1;   
            if (bit) 
            {
                plruTree[set] &= ~(1 << index);
            }
            else 
            {
                plruTree[set] |= uint64_t(1) << index;
            }
            
            index = bit ? (index * 2) + 2 : (index * 2) + 1;
        }
    }

    // Initialize replacement state
    void initReplacementState()
    {
        plruTree = new uint64_t[numSets];
        plruNumLevels = 0;
        plruEffecAssoc = 1;
        
        while (plruEffecAssoc < numWays)
        {
            plruEffecAssoc <<= 1;
        }

        uint64_t assoc = numWays;
        while (true)
        {
            assoc /= 2;
            if (!assoc)
            {
                break;
            }
            ++plruNumLevels;
        }

        // LRU stack
        lruStack = new deque<uint64_t>[numSets];
        for (int i = 0; i < 16; ++i)
        {
            lruStackHit[i] = 0;
        }
    }
};

class CacheInternals
{
  private:

    MyCache* cacheNew(uint64_t size, uint64_t assoc, uint64_t line_size)
    {
        if (assoc > MAX_WAYS)
        {
            CALIPERS_ERROR("Max number of cache ways is " << MAX_WAYS);
        }

        MyCache* c = new MyCache;
        c->numWays = assoc;
        c->lineSize = line_size;

        // Determine number of sets, and init the cache
        c->numSets = size / (line_size * assoc);
        c->sets = new CacheSet[c->numSets];

        // Counters
        c->statReadAccess = 0;
        c->statWriteAccess = 0;
        c->statReadMiss = 0;
        c->statWriteMiss = 0;
        c->statDirtyEvicts = 0;
       
        c->initReplacementState();

        return c;
    }

    // Copy victim into lastEvictedLine for tracking write-backs
    // type 0: load, read
    // type 1: store (RFO), full cache line writes
    // type 2: write-back
    // Should fix so that you can't hit the line right after miss.
    // Keep ready-cycle by propagating latency.
    uint32_t cacheAccessInstall(MyCache* c, uint64_t p_addr, uint32_t type)
    {
        uint32_t outcome = MISS;
        uint64_t line_addr = p_addr / c->lineSize;

        uint64_t way = 0; 
        uint64_t ii = 0;
        uint64_t set = (p_addr / c->lineSize) % c->numSets;
      
        if (type == 0)
        {
            ++(c->statReadAccess);
        }
        else
        {
            ++(c->statWriteAccess);
        }

        for (ii = 0; ii < c->numWays; ++ii)
        {
            if (c->sets[set].line[ii].valid && (c->sets[set].line[ii].tag == line_addr))
            {
                outcome = HIT;
                way = ii;
                break;
            }
        }
     
        // LRU stack calculation
        auto lruStackPtr = &(c->lruStack[set]);
        int lruDepthCtr = 0;
        bool lruStackHit = MISS;
        for (auto it = lruStackPtr->begin(); it != lruStackPtr->end(); ++it)
        {
            if (*it == line_addr)
            {
                ++c->lruStackHit[lruDepthCtr];
                if (lruDepthCtr != 0)
                {
                    lruStackPtr->erase(it);
                    lruStackPtr->emplace_front(line_addr);
                }
                lruStackHit = HIT;
                break;
            }
            ++lruDepthCtr;
            if (lruDepthCtr == 16)
            {
                // Missed; erase last element
                lruStackPtr->erase(it);
                break;
            }
        }

        if (lruStackHit == MISS)
        {
            lruStackPtr->emplace_front(line_addr);
        }

        if (outcome == HIT)
        {
            if ((type == 1) || (type == 2))
            {
                c->sets[set].line[ii].dirty = true;
            }
            c->updateReplacementState(set, way);
            return HIT;
        }

        // Got miss, victim and install
        uint64_t victim_way = c->getVictimInSet(set); // Pass next reuse into back
        if (victim_way < c->numWays)
        {
            if (c->sets[set].line[victim_way].dirty)
            {
                ++(c->statDirtyEvicts);
            }

            c->lastEvictedLine.valid = c->sets[set].line[victim_way].valid;
            c->lastEvictedLine.tag = c->sets[set].line[victim_way].tag;
            c->lastEvictedLine.dirty = c->sets[set].line[victim_way].dirty;

            c->sets[set].line[victim_way].valid = true;
            c->sets[set].line[victim_way].tag = line_addr;
            c->sets[set].line[victim_way].dirty = ((type == 1) || (type == 2)) ? true : false;

            c->updateReplacementState(set, victim_way); 
        }

        if (type == 0)
        {
            ++(c->statReadMiss);
        }
        else
        {
            ++(c->statWriteMiss);
        }

        return MISS;
    }


  public:
    uint64_t sInstructionCount;
    MyCache* l1cache;
    MyCache* l2cache;
    MyCache* l3cache;
    uint64_t pCacheConfig;

    CacheInternals(uint64_t cache_config,
                   uint32_t l1_size, uint32_t l1_assoc,
                   uint32_t l2_size, uint32_t l2_assoc)
    {
        sInstructionCount = 0;
        pCacheConfig = cache_config;
        
        switch (pCacheConfig)
        {
            case 0:
                break;
            case 1:
                break;
            case 2:
                l1cache = cacheNew(32 * 1024, 4 , 64); // 32 KB, 4-way
                break;
            case 3:
                l1cache = cacheNew(l1_size, l1_assoc, 64);
                l2cache = cacheNew(l2_size, l2_assoc, 64);
                break;
            case 4:
                l1cache = cacheNew(32 * 1024, 4, 64);       // 32 KB, 4-way
                l2cache = cacheNew(256 * 1024, 8, 64);      // 256 KB, 8-way
                l3cache = cacheNew(2 * 1024 * 1024, 8, 64); // 2048 KB, 8-way
                break;
            default:
                break;
        }
    }

    ~CacheInternals()
    {
        switch (pCacheConfig)
        {
            case 0:
                break;
            case 1:
                break;
            case 2:
                delete l1cache->sets;
                delete l1cache;
                break;
            case 3:
                delete l1cache->sets;
                delete l1cache;
                delete l2cache->sets;
                delete l2cache;
                break;
            case 4:
                delete l1cache->sets;
                delete l1cache;
                delete l2cache->sets;
                delete l2cache;
                delete l3cache->sets;
                delete l3cache;
                break;
            default:
                break;
        }
    }

    uint32_t memoryAccess(uint64_t addr, uint32_t type)
    {
        uint32_t outcome = false;
        uint32_t hit_level = 5;
        
        switch (pCacheConfig) 
        {
            case 0: // Perfect mem
                hit_level = 0;
                break;
            case 1: // Perfect l1
                hit_level = 1;
                break;
            case 2: // l1, perfect l2
                outcome = cacheAccessInstall(l1cache, addr, type);
                if (type == 1) // ASSUME FULL CACHE LINE WRITES. Stores don't propagate.
                {
                    hit_level = 1;
                    break;
                }
                if (outcome != false) // Should stores be single latency or L1 cache latency?
                {
                    hit_level = 1;
                }
                else
                {
                    hit_level = 2;
                }
                break;
            case 3: // l1 + l2, perfect l3
                outcome = cacheAccessInstall(l1cache, addr, type);
                if (type == 1) // ASSUME FULL CACHE LINE WRITES. Stores don't propagate.
                {
                    hit_level = 1;
                    break;
                }
                if (outcome != false)
                {
                    hit_level = 1;
                }
                else
                {
                    outcome = cacheAccessInstall(l2cache, addr, 0);
                    if (outcome != false)
                    {
                        hit_level = 2;
                    }
                    else
                    {
                        hit_level = 3;
                    }
                    if (l1cache->lastEvictedLine.valid && l1cache->lastEvictedLine.dirty)
                    {
                        cacheAccessInstall(l2cache,
                            l1cache->lastEvictedLine.tag * l1cache->lineSize, 2);
                    }
                }
                break;
            case 4: // l1 + l2 + l3, fixed latency mem
                outcome = cacheAccessInstall(l1cache, addr, type);
                if (type == 1) // ASSUME FULL CACHE LINE WRITES. Stores don't propagate.
                {
                    hit_level = 1;
                    break;
                }
                if (outcome != false)
                {
                    hit_level = 1;
                }
                else
                {
                    outcome = cacheAccessInstall(l2cache, addr, 0); // Demand access after l2
                    if (outcome != false)
                    {
                        hit_level = 2;
                    }
                    else
                    {
                        outcome = cacheAccessInstall(l3cache, addr, 0);
                        if (outcome != false)
                        {
                            hit_level = 3;
                        }
                        else
                        {
                            hit_level = 4;
                        }
                        if (l2cache->lastEvictedLine.valid && l2cache->lastEvictedLine.dirty)
                        {
                            cacheAccessInstall(l3cache,
                                l2cache->lastEvictedLine.tag * l2cache->lineSize, 2);
                        }
                    }
                    if (l1cache->lastEvictedLine.valid && l1cache->lastEvictedLine.dirty)
                    {
                        outcome = cacheAccessInstall(l2cache,
                            l1cache->lastEvictedLine.tag * l1cache->lineSize, 2);
                        if ((outcome == false) &&
                            l2cache->lastEvictedLine.valid && l2cache->lastEvictedLine.dirty)
                        {
                            cacheAccessInstall(l3cache,
                                l2cache->lastEvictedLine.tag * l2cache->lineSize, 2);
                        }
                    }
                }
                break;
            default:
                CALIPERS_ERROR("Incorrect cache configuration knob");
        }

        return hit_level;
    }
};

/**
 * An analytical/functional cache model
 */
class RealCache: public Cache
{
  private:
    CacheInternals* cacheInternals;
    uint32_t l1Size;
    uint32_t l1Assoc;
    uint32_t l2Size;
    uint32_t l2Assoc;
    uint32_t l1LoadHitCycles;
    uint32_t l2LoadHitCycles;
    uint32_t l2LoadMissCycles;
    uint32_t l1StoreHitCycles;
    uint32_t l2StoreHitCycles;
    uint32_t l2StoreMissCycles;

  public:
    RealCache(string config)
    {
        vector<string> config_vec = split_string(config, ':');
        if (config_vec.size() != 10)
        {
            CALIPERS_ERROR("Invalid configuration for the real cache");
        }

        l1Size = stof(config_vec[0]);
        l1Assoc = stoi(config_vec[1]);
        l2Size = stoi(config_vec[2]);
        l2Assoc = stof(config_vec[3]);
        l1LoadHitCycles = stof(config_vec[4]);
        l2LoadHitCycles = stoi(config_vec[5]);
        l2LoadMissCycles = stoi(config_vec[6]);
        l1StoreHitCycles = stof(config_vec[7]);
        l2StoreHitCycles = stoi(config_vec[8]);
        l2StoreMissCycles = stoi(config_vec[9]);

        // Two-level cache
        cacheInternals = new CacheInternals(3, l1Size, l1Assoc, l2Size, l2Assoc);
    }

    uint32_t loadCycles(uint64_t base, uint32_t length)
    {
        uint32_t hit_level;
        hit_level = cacheInternals->memoryAccess(base, 0);
        switch (hit_level)
        {
            case 1: return l1LoadHitCycles;
            case 2: return l2LoadHitCycles;
            default: return l2LoadMissCycles;

        }
    }

    uint32_t storeCycles(uint64_t base, uint32_t length)
    {
        uint32_t hit_level;
        hit_level = cacheInternals->memoryAccess(base, 1);
        switch (hit_level)
        {
            case 1: return l1StoreHitCycles;
            case 2: return l2StoreHitCycles;
            default: return l2StoreMissCycles;

        }
    }

    void printStats()
    {
        cout << "*** L1 stats:" << endl;
        cout << "    Read accesses:  " << cacheInternals->l1cache->statReadAccess << endl;
        cout << "    Read misses:    " << cacheInternals->l1cache->statReadMiss << endl;
        cout << "    Write accesses: " << cacheInternals->l1cache->statWriteAccess << endl;
        cout << "    Write misses:   " << cacheInternals->l1cache->statWriteMiss << endl;
        cout << "    Dirty evicts:   " << cacheInternals->l1cache->statDirtyEvicts << endl;

        cout << "*** L2 stats:" << endl;
        cout << "    Read accesses:  " << cacheInternals->l2cache->statReadAccess << endl;
        cout << "    Read misses:    " << cacheInternals->l2cache->statReadMiss << endl;
        cout << "    Write accesses: " << cacheInternals->l2cache->statWriteAccess << endl;
        cout << "    Write misses:   " << cacheInternals->l2cache->statWriteMiss << endl;
        cout << "    Dirty evicts:   " << cacheInternals->l2cache->statDirtyEvicts << endl;
    }
};

#endif // REAL_CACHE_H
