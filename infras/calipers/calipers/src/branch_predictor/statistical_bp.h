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

#ifndef STATISTICAL_BP_H
#define STATISTICAL_BP_H

#include <stdint.h>

#include "calipers_util.h"
#include "branch_predictor.h"

using namespace std;

/**
 * A statistical/stochastic branch predictor with fixed accuracy
 */
class StatisticalBp : public BranchPredictor
{
  private:
    float accuracy;

  public:
    StatisticalBp(string config)
    {
        vector<string> config_vec = split_string(config, ':');

        if (config_vec.size() != 2)
        {
            CALIPERS_ERROR("Invalid configuration for the statistical branch predictor");
        }

        accuracy = stof(config_vec[0]);
        predictionCycles = stoi(config_vec[1]);

    }

    bool mispredicted(uint64_t pc)
    {
        int r = rand() % 1000;
        return (r >= 10 * accuracy);
    }
};

#endif // STATISTICAL_BP_H
