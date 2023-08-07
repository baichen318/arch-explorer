# Author: baichen318@gmail.com


import os
import re
import torch
import random
import numpy as np
from torch import Tensor
from abc import ABC, abstractmethod
from utils.utils import if_exist, assert_error
from typing import List, Optional, Tuple, NoReturn
from algo.dataset import load_dataset, ndarray_to_tensor
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space


seed = int(2023)
torch.manual_seed(2023)
torch.cuda.manual_seed(2023)
random.seed(2023)
np.random.seed(2023)
arch_explorer_root = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__),
        os.path.pardir,
        os.path.pardir,
        os.path.pardir
    )
)
pat = re.compile(r"\d+\.\d+")


class BaseProblem(torch.nn.Module, ABC):
    """
        base class for construction a problem.
    """

    dim: int
    _bounds: List[Tuple[float, float]]
    _check_grad_at_opt: bool = True

    def __init__(self, noise_std: Optional[float] = None, negate: bool = False) -> None:
        """
            base class for construction a problem.

        args:
            noise_std: standard deviation of the observation noise.
            negate: if True, negate the function.
        """
        super().__init__()
        self.noise_std = noise_std
        self.negate = negate
        self.register_buffer(
            "bounds", torch.tensor(self._bounds, dtype=torch.float).transpose(-1, -2)
        )

    def forward(self, X: Tensor, noise: bool = True) -> Tensor:
        """
            evaluate the function on a set of points.

        args:
            X: a `batch_shape x d`-dim tensor of point(s) at which to evaluate the
                function.
            noise: if `True`, add observation noise as specified by `noise_std`.

        returns:
            a `batch_shape`-dim tensor of function evaluations.
        """
        batch = X.ndimension() > 1
        X = X if batch else X.unsqueeze(0)
        f = self.evaluate_true(X=X)
        if noise and self.noise_std is not None:
            f += self.noise_std * torch.randn_like(f)
        if self.negate:
            f = -f
        return f if batch else f.squeeze(0)

    @abstractmethod
    def evaluate_true(self, X: Tensor) -> Tensor:
        """
            evaluate the function (w/o observation noise) on a set of points.
        """
        raise NotImplementedError


class MultiObjectiveProblem(BaseProblem):
    """
        base class for a multi-objective problem.
    """

    num_objectives: int
    _ref_point: List[float]
    _max_hv: float

    def __init__(self, noise_std: Optional[float] = None, negate: bool = False) -> None:
        """
            base constructor for multi-objective test functions.

        args:
            noise_std: standard deviation of the observation noise.
            negate: if True, negate the objectives.
        """
        super().__init__(noise_std=noise_std, negate=negate)
        ref_point = torch.tensor(self._ref_point, dtype=torch.float)
        if negate:
            ref_point *= -1
        self.register_buffer("ref_point", ref_point)

    @property
    def max_hv(self) -> float:
        try:
            return self._max_hv
        except AttributeError:
            raise NotImplementedError(
                error_message("problem {} does not specify maximal hypervolume".format(
                    self.__class__.__name__)
                )
            )

    def gen_pareto_front(self, n: int) -> Tensor:
        """
            generate `n` pareto optimal points.
        """
        raise NotImplementedError


class DesignSpaceProblem(MultiObjectiveProblem):
    def __init__(self, configs: dict, settings: dict):
        self.configs = configs
        self.settings = settings
        if settings["mode"] == "offline":
            self.load_dataset()
        else:
            assert settings["mode"] == "online", \
                assert_error("working mode should set online.")
            self.design_space = parse_design_space(
                self.configs["design-space"]
            )
            self.generate_design_space()

        self._ref_point = torch.tensor([0.0, -1.0, -15.0])
        self._bounds = torch.tensor([(5.0, 0.0, 0.0)])
        super().__init__()

    def load_dataset(self) -> NoReturn:
        x, y = load_dataset(
            os.path.join(
                arch_explorer_root,
                self.configs["dataset"]["output"]
            )
        )
        self.total_x = ndarray_to_tensor(x)
        self.total_y = ndarray_to_tensor(y[:, :-1])
        self.time = ndarray_to_tensor(y[:, -1])
        self.x = self.total_x.clone()
        self.y = self.total_y.clone()
        self.n_dim = self.x.shape[-1]
        self.n_sample = self.x.shape[0]

    def generate_design_space(self) -> NoReturn:
        design_space = random.sample(
            range(self.design_space.size), k=100
        )
        x = []
        for i in range(len(design_space)):
            x.append(self.design_space.idx_to_embedding(design_space[i]))
        self.total_x = ndarray_to_tensor(np.array(x))
        self.x = self.total_x.clone()
        self.n_dim = self.x.shape[-1]
        self.n_sample = self.x.shape[0]

    def get_simulation_results(self, simulator, idx):
        ipc, cpi, power, area = [], [], [], []
        for k, v in simulator.benchmark.macros.items():
            rpt = os.path.join(
                arch_explorer_root,
                "temp",
                "gem5-{}".format(idx),
                k,
                "ppa.rpt"
            )
            if if_exist(rpt, quiet=False):
                with open(rpt, 'r') as fin:
                    result = fin.read()
                    result = re.findall(pat, result)
                    _ipc = float(result[0])
                    _cpi = float(result[1])
                    _power = float(result[2])
                    _area = float(result[3])
                    ipc.append(_ipc)
                    cpi.append(_cpi)
                    power.append(_power)
                    area.append(_area)
        if len(ipc) == 0:
            return 0, 0, 0, 0
        ipc = np.average(ipc)
        cpi = np.average(cpi)
        power = np.average(power)
        area = np.average(area)
        return ipc, cpi, power, area

    def evaluate_true(self, x: torch.Tensor) -> torch.Tensor:
        if self.configs["mode"] == "offline":
            _, indices = torch.topk(
                ((self.x.t() == x.unsqueeze(-1)).all(dim=1)).int(),
                1,
                1
            )
            return self.y[indices].to(torch.float32).squeeze()
        else:
            y = []
            for embedding in x:
                embedding = embedding.numpy().tolist()
                embedding = [int(i) for i in embedding]
                simulator = O3CPUSimulation(
                    design_space=self.design_space,
                    configs=self.configs["simulation"],
                )
                simulator.simulate(embedding)
                ipc, _, power, area = self.get_simulation_results(
                    simulator,
                    self.design_space.embedding_to_idx(embedding)
                )
                # NOTICE: we assume maximization
                y.append([ipc, -power, -area])
            y = np.array(y)
            y = torch.Tensor(y)
            return y

    def remove_sampled_data(self, x: torch.Tensor) -> NoReturn:
        if self.configs["mode"] == "offline":
            sampled = torch.zeros(
                self.x.size()[0],
                dtype=torch.bool
            )[:, np.newaxis]
            _, indices = torch.topk(
                ((self.x.t() == x.unsqueeze(-1)).all(dim=1)).int(),
                1,
                1
            )
            mask = sampled.index_fill_(0, indices.squeeze(), True).squeeze()
            self.x = self.x[mask[:] == False]
            self.y = self.y[mask[:] == False]


def create_problem(configs: dict, settings: dict) -> DesignSpaceProblem:
    return DesignSpaceProblem(configs, settings)
