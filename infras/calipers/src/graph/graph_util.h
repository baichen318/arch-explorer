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


#ifndef GRAPH_UTIL_H
#define GRAPH_UTIL_H

#include <unordered_map>
#include <set>
#include <vector>

#include "calipers_defs.h"

using namespace std;


/**
 * A utility class for vector-weighted edges
 */
class Vector
{
  // private:
  public:
    int64_t vec[VECTOR_WIDTH];

  public:
    Vector(int64_t* arr, uint32_t width)
    {
        if (width != VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid array width in vector init");
        }
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            vec[i] = arr[i];
        }
    }

    Vector(int64_t* arr, uint32_t width, int64_t offset)
    {
        if (width != VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid array width in vector init with offset");
        }
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            vec[i] = arr[i] - offset;
        }
    }

    Vector(const Vector& v_in, int64_t offset)
    {
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            vec[i] = v_in[i] - offset;
        }
    }

    Vector(int64_t val)
    {
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            vec[i] = val;
        }
    }

    Vector(int64_t val, uint32_t idx)
    {
        if (idx >= VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid index in vector init");
        }
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            vec[i] = INT64_MAX;
        }
        vec[idx] = val;
    }
    Vector() : Vector(0)
    {}

    void operator=(const Vector& v_in)
    {
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            vec[i] = v_in[i];
        }
    }

    int64_t operator[](const uint32_t idx) const
    {
        if (idx >= VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid index for vector element access");
        }
        return vec[idx];
    }

    void update(const Vector& v_in1, const Vector& v_in2)
    {
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            if ((v_in2[i] != INT64_MAX) && (v_in1[i] + v_in2[i] > vec[i]))
            {
                vec[i] = v_in1[i] + v_in2[i];
            }
        }
    }

    void update(const Vector& v_in1, const Vector& v_in2, bool* mask, uint32_t width)
    {
        if (width != VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid output mask width in vector update");
        }
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            if ((v_in2[i] != INT64_MAX) && (v_in1[i] + v_in2[i] >= vec[i]))
            {
                vec[i] = v_in1[i] + v_in2[i];
                mask[i] = true;
            }
            else
            {
                mask[i] = false;
            }
        }
    }

    void maskedSet(const Vector& v_in, bool* mask, uint32_t width)
    {
        if (width != VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid input mask width in vector maskedSet");
        }
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            if (mask[i])
            {
                vec[i] = v_in[i];
            }
        }
    }

    void maskedAdd(const Vector& v_in, bool* mask, uint32_t width)
    {
        if (width != VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid input mask width in vector maskedAdd");
        }
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            if (mask[i])
            {
                vec[i] += v_in[i];
            }
        }
    }

    void largerThan(int64_t val, bool* mask, bool* result, uint32_t width)
    {
        if (width != VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid mask/result width in vector largerThan");
        }
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            if (mask[i] && (vec[i] > val))
            {
                result[i] = true;
            }
            else
            {
                result[i] = false;
            }
        }
    }

    void smallerThanOrEqual(int64_t val, bool* mask, bool* result, uint32_t width)
    {
        if (width != VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid mask/result width in vector smallerThanOrEqual");
        }
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            if (mask[i] && (vec[i] <= val))
            {
                result[i] = true;
            }
            else
            {
                result[i] = false;
            }
        }
    }

    void between(int64_t val1, int64_t val2, bool* mask, bool* result, uint32_t width)
    {
        if (width != VECTOR_WIDTH)
        {
            CALIPERS_ERROR("Invalid mask/result width in vector between");
        }
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            if (mask[i] && (vec[i] > val1) && (vec[i] <= val2))
            {
                result[i] = true;
            }
            else
            {
                result[i] = false;
            }
        }
    }

    string toString()
    {
        string s = "";
        for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
        {
            s.append(to_string(vec[i]) + " ");
        }
        return s;
    }
};

typedef struct VERTEX
{
    int type; // From enum VertexType
    uint64_t instrNum;
    string inst;

    VERTEX(int type, uint64_t instrNum) : type(type), instrNum(instrNum) {
        inst = "";
    }

    VERTEX(int type, uint64_t instrNum, string inst): type(type), instrNum(instrNum), inst(inst) {}

    VERTEX() : type(0), instrNum(0)
    {}
} Vertex;

/**
 * An INT64_MAX entry in the weight vector denotes the corresponding edge
 * does not exist in that specific scenraio. An edge migh exist in
 * one scenario and not exist in another scenraio. This may happen, e.g.,
 * for edges related to branch misprediction and structural hazards.
 */
typedef struct OUTGOING_EDGE
{
    Vertex child;
    Vector weight;

    OUTGOING_EDGE() : child(Vertex(0, 0)), weight(0)
    {}

    OUTGOING_EDGE(Vertex child, Vector& v) : child(child)
    {
        weight = v;
    }

    OUTGOING_EDGE(Vertex child, int64_t val) : child(child), weight(val)
    {}

    OUTGOING_EDGE(Vertex child, int64_t val, uint32_t idx) : child(child), weight(val, idx)
    {}

} OutgoingEdge;


typedef struct INCOMING_EDGE
{
    Vertex parent;
    Vector weight;

    INCOMING_EDGE() : parent(Vertex(0, 0)), weight(0)
    {}

    INCOMING_EDGE(Vertex parent, Vector& v) : parent(parent)
    {
        weight = v;
    }

    INCOMING_EDGE(Vertex parent, int64_t val) : parent(parent), weight(val)
    {}

    INCOMING_EDGE(Vertex parent, int64_t val, uint32_t idx) : parent(parent), weight(val, idx)
    {}

} IncomingEdge;

#endif // GRAPH_UTIL_H
