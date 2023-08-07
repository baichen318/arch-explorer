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


#ifndef SCOREBOARD_SIMPLE_H
#define SCOREBOARD_SIMPLE_H

#include "calipers_defs.h"


/**
 * A class for keeping track of the users of different execution units
 */
class ScoreboardSimple
{
  private:
    typedef struct RESOURCE_INSTANCE
    {
        uint32_t count;
        uint32_t totalCycles;
        uint32_t sourceIndependentCycles; // Cycles possible to proceed without source operands
        uint32_t nextIssueCycles; // Cycles before the next operation can be issued
    } ResourceInstance;


    // The record of the units of a particular resrouce type
    typedef struct RESOURCE_RECORD
    {
        uint64_t* users; // Instruction number of the last user of each unit
        uint32_t next; // Index of the unit to be used by the next instruction
        uint64_t** prevUsers; // Instruction numbers of the previous users of each unit
        uint32_t* pipelineHead; // Index of the pipeline head (in prevUsers) for each unit

    } ResourceRecord;

    unordered_map<int, ResourceInstance> resources; // Key: Resource type (from enum Resource)
    unordered_map<int, ResourceRecord> records; // Key: Resource type (from enum Resource)

  public:
    void initResource(int type, uint32_t count, uint32_t total_cycles,
                      uint32_t source_independent_cycles, uint32_t next_issue_cycles)
    {
        if (resources.count(type) != 0)
        {
            CALIPERS_ERROR("Resource already initialized")
        }

        resources[type].count = count;
        resources[type].totalCycles = total_cycles;
        resources[type].sourceIndependentCycles = source_independent_cycles;
        resources[type].nextIssueCycles = next_issue_cycles;

        for (uint32_t i = 0; i < count; ++i)
        {
            records[type].users = new uint64_t[count];
            records[type].prevUsers = new uint64_t*[count];
            for (uint32_t j = 0; j < count; ++j)
            {
                records[type].prevUsers[j] = new uint64_t[total_cycles];
            }
            records[type].pipelineHead = new uint32_t[count];
        }

        initRecords();
    }

    void initRecords()
    {
        for (auto it = records.begin(); it != records.end(); ++it)
        {
            int type = it->first;
            records[type].next = 0;
            for (uint32_t j = 0; j < resources[type].count; ++j)
            {
                records[type].users[j] = UINT64_MAX;
                records[type].pipelineHead[j] = 0;
                for (uint32_t k = 0; k < resources[type].totalCycles; ++k)
                {
                    records[type].prevUsers[j][k] = UINT64_MAX;
                }
            }
        }
    }

    void scheduleResource(int type, uint64_t instrNum,
                          uint32_t& instance, uint64_t& previous_instr,
                          uint32_t& wait_cycles, uint64_t& head_of_pipeline)
    {
        instance = records[type].next;
        uint32_t pipeline_idx = records[type].pipelineHead[instance];
        previous_instr = records[type].users[instance];
        wait_cycles = resources[type].nextIssueCycles;
        head_of_pipeline = records[type].prevUsers[instance][pipeline_idx];

        records[type].users[instance] = instrNum;
        records[type].next = (instance + 1) % resources[type].count;
        records[type].prevUsers[instance][pipeline_idx] = instrNum;
        records[type].pipelineHead[instance] = (pipeline_idx + 1) % resources[type].totalCycles;

    }

    uint32_t resourceCount(int type)
    {
        return resources[type].count;
    }

    uint32_t resourceTotalCycles(int type)
    {
        return resources[type].totalCycles;
    }

    uint32_t resourceSourceIndependentCycles(int type)
    {
        return resources[type].sourceIndependentCycles;
    }

    uint32_t resourceNextIssueCycles(int type)
    {
        return resources[type].nextIssueCycles;
    }

    ~ScoreboardSimple()
    {
        for (auto it = records.begin(); it != records.end(); ++it)
        {
            int type = it->first;
            delete[] records[type].users;
            for (uint32_t j = 0; j < resources[type].count; ++j)
            {
                delete[] records[type].prevUsers[j];
            }
            delete[] records[type].prevUsers;
            delete[] records[type].pipelineHead;
        }
    }

};

#endif // SCOREBOARD_SIMPLE_H
