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


#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include "calipers_defs.h"


/**
 * A class for keeping track of the users of different execution units and queues
 */
class Scoreboard
{
  private:
    typedef struct RESOURCE_INSTANCE
    {
        uint32_t count;
        uint32_t latency;
        bool pipelined;
        uint32_t nextAvailable;
        uint64_t* assignedInstrNum;
        int* assignedOp; // For resources that do a mixture of operations
    } ResourceInstance;


    typedef struct QUEUE_INSTANCE
    {
        uint32_t size;
        uint64_t* latency;
        uint32_t nextAvailable;
        uint64_t* assignedInstrNum;
    } QueueInstance;

    unordered_map<int, ResourceInstance> resources;
    // Key: Resource type (from enum Resource)

    unordered_map<int, int> mixedOperationResource;
    // Key: Operation type (from enum Resource)
    // Value: Resource type (from enum Resource)

    unordered_map<int, pair<uint32_t, bool>> mixedOperationSpec;
    // Key: Operation type (from enum Resource)
    // Value.first: Operation latency
    // Value.second: Pipelined?

    unordered_map<int, QueueInstance> queues;
    // Key: Queue type (from enum QueueResource)

  public:
    void setMixedOperation(int operation_type, int resource_type,
                           uint32_t latency, bool pipelined)
    {
        if (resources.count(resource_type) == 0)
        {
            CALIPERS_ERROR("Resource for the mixed operation not initialized yet");
        }

        if (resources[resource_type].assignedOp == NULL)
        {
            resources[resource_type].assignedOp = new int[resources[resource_type].count];
        }

        mixedOperationResource[operation_type] = resource_type;
        mixedOperationSpec[operation_type].first = latency;
        mixedOperationSpec[operation_type].second = pipelined;
    }

    void initResource(int resource_type, uint32_t count,
                      uint32_t latency, bool pipelined)
    {
        if (resources.count(resource_type) != 0)
        {
            CALIPERS_ERROR("Resource already initialized");
        }

        resources[resource_type].count = count;
        resources[resource_type].latency = latency;
        resources[resource_type].pipelined = pipelined;
        resources[resource_type].assignedInstrNum = new uint64_t[count];
        resources[resource_type].assignedOp = NULL;
        resetResource(resource_type);
    }

    void resetResource(int resource_type)
    {
        resources[resource_type].nextAvailable = 0;
        for (uint32_t i = 0; i < resources[resource_type].count; ++i)
        {
            resources[resource_type].assignedInstrNum[i] = UINT64_MAX;
        }
    }

    void initQueue(int type, uint32_t size)
    {
        if (queues.count(type) != 0)
        {
            CALIPERS_ERROR("Queue already initialized");
        }

        queues[type].size = size;
        queues[type].latency = new uint64_t[size];
        queues[type].assignedInstrNum = new uint64_t[size];
        resetQueue(type);
    }

    void resetQueue(int type)
    {
        queues[type].nextAvailable = 0;
        for (uint32_t i = 0; i < queues[type].size; ++i)
        {
            queues[type].assignedInstrNum[i] = UINT64_MAX;
            queues[type].latency[i] = 0;
        }        
    }

    void scheduleResource(int operation_type, uint64_t instr_num,
                          uint64_t& previous_instr, uint32_t& wait_cycles)
    {
        int resource_type;
        uint32_t sample_num;
        uint32_t latency;
        bool pipelined;

        if (mixedOperationResource.count(operation_type) == 0)
        {
            resource_type = operation_type;
            sample_num = resources[resource_type].nextAvailable;
            latency = resources[resource_type].latency;
            pipelined = resources[resource_type].pipelined;
        }
        else
        {
            resource_type = mixedOperationResource[operation_type];
            sample_num = resources[resource_type].nextAvailable;
            int previous_operation_type = resources[resource_type].assignedOp[sample_num];
            latency = mixedOperationSpec[previous_operation_type].first;
            pipelined = mixedOperationSpec[previous_operation_type].second;
            resources[resource_type].assignedOp[sample_num] = operation_type;
        }

        previous_instr = resources[resource_type].assignedInstrNum[sample_num];
        wait_cycles = pipelined ? 1 : latency;

        resources[resource_type].assignedInstrNum[sample_num] = instr_num;
        resources[resource_type].nextAvailable = (sample_num + 1) %
                                                 resources[resource_type].count;
    }

    void scheduleQueue(int type, uint64_t instr_num, uint32_t latency,
                       uint64_t& previous_instr, uint32_t& wait_cycles)
    {
        uint32_t entry = queues[type].nextAvailable;
        previous_instr = queues[type].assignedInstrNum[entry];
        wait_cycles = queues[type].latency[entry];

        queues[type].nextAvailable = (entry + 1) % queues[type].size;
        queues[type].assignedInstrNum[entry] = instr_num;
        queues[type].latency[entry] = latency;
    }

    uint32_t getResourceCount(int type)
    {
        return resources[type].count;
    }

    uint32_t getResourceLatency(int type)
    {
        if (mixedOperationResource.count(type) == 0)
        {
            return resources[type].latency;
        }
        else
        {
            return mixedOperationSpec[type].first;
        }
    }

    uint32_t getQueueSize(int type)
    {
        return queues[type].size;
    }

    ~Scoreboard()
    {
        for (auto it = resources.begin(); it != resources.end(); ++it)
        {
            int type = it->first;
            delete[] resources[type].assignedInstrNum;
            if (resources[type].assignedOp != NULL)
            {
                delete[] resources[type].assignedOp;
            }
        }

        for (auto it = queues.begin(); it != queues.end(); ++it)
        {
            int type = it->first;
            delete[] queues[type].latency;
            delete[] queues[type].assignedInstrNum;
        }
    }
};

#endif // SCOREBOARD_H
