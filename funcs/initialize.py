# Author: baichen.bai@alibaba-inc.com


import os
import math
import random
import numpy as np
from utils.utils import mkdir, info
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space


class RandomizedTED(object):
    """
        `Nrted`: <int>
        `mu`: <float>
    """

    def __init__(self, *args):
        super(RandomizedTED, self).__init__()
        self.Nrted = args[0]
        self.mu = args[1]
        self.sig = args[2]

    def f(self, u, v):
        # t = np.linalg.norm(np.array(u,dtype=np.float64)-np.array(v,dtype=np.float64))**2
        # if t > 1:
        #     print('t is ',t)
        # NOTICE: target value should be discards
        u = u[:-2]
        v = v[:-2]
        return pow(
            math.e,
            -np.linalg.norm(
                np.array(u, dtype=np.float64) - np.array(v, dtype=np.float64)
            )**2 / (2 * self.sig**2)
        )

    def f_same(self, K):
        n = len(K)
        F = []
        for i in range(n):
            t = []
            for j in range(n):
                t.append(self.f(K[i], K[j]))
            F.append(t)
        return np.array(F)

    def update_f(self, F, K):
        n = F.shape[0]
        for i in range(len(K)):
            denom = self.f(K[i], K[i]) + self.mu
            for j in range(n):
                for k in range(n):
                    F[j][k] -= (F[j][i] * F[k][i]) / denom

    def select_mi(self, K, F):
        return K[
            np.argmax(
                [np.linalg.norm(F[i]) ** 2 / (self.f(K[i], K[i]) + self.mu) \
                    for i in range(len(K))]
            )
        ]

    def rted(self, vec, m):
        """
            API for Randomized TED
            vec: <np.array>
            m: <int>: number of a batch
        """
        K_ = []
        for i in range(m):
            M_ = random.sample(list(vec), self.Nrted)
            M_ = M_ + K_
            M_ = [tuple(M_[j]) for j in range(len(M_))]
            M_ = list(set(M_))
            F = self.f_same(M_)
            self.update_f(F, M_)
            K_.append(self.select_mi(M_, F))
        return K_


def initialize(configs: dict):
	random.seed(int(configs["initialize"]["seed"]))
	o3cpu_design_space = parse_design_space(configs["design-space"])

	sample = []
	sampler = RandomizedTED(
		configs["initialize"]["Nrted"],
		configs["initialize"]["mu"],
		configs["initialize"]["sig"]
	)
	while len(sample) < configs["initialize"]["size"]:
		info("progress: {}/{}".format(
			len(sample),
			configs["initialize"]["size"])
		)
		group = random.sample(range(o3cpu_design_space.size), configs["initialize"]["group-size"])
		_group = [o3cpu_design_space.idx_to_embedding(j) for j in group]
		for j in sampler.rted(np.array(_group), configs["initialize"]["top-k"]):
			if j not in sample:
				sample.append(j)

	mkdir(os.path.dirname(configs["initialize"]["output"]))
	with open(configs["initialize"]["output"], 'w') as f:
		for s in sample:
			f.write('{}\n'.format(s))
	info("initialization done.")
