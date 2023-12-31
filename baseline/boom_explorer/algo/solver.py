# Author: baichen318@gmail.com


import os
import tqdm
import torch
import gpytorch
import numpy as np
from typing import NoReturn
from algo.microal import micro_al
from algo.dataset import rescale_dataset
from algo.model import initialize_dkl_gp
from algo.visualize import plot_pareto_set
from algo.problem import DesignSpaceProblem
from utils.utils import assert_error, info, mkdir, write_txt
from algo.metric import calc_adrs, get_pareto_frontier, get_pareto_optimal_solutions
from botorch.acquisition.multi_objective.analytic import ExpectedHypervolumeImprovement
from botorch.utils.multi_objective.box_decompositions.non_dominated import NondominatedPartitioning


class BOOMExplorerSolver(object):
    def __init__(self, problem: object):
        super(BOOMExplorerSolver, self).__init__()
        self.problem = problem
        self.output_path = self.problem.settings["report"]["path"]
        mkdir(self.output_path)
        
    def initialize(self) -> NoReturn:
        # Microarctecture-aware active learning
        self.visited_x, self.visited_y = micro_al(self.problem)
        # self.adrs = []
        # self.adrs.append(calc_adrs(
        #         get_pareto_frontier(self.problem.total_y, reverse=False),
        #         get_pareto_frontier(self.visited_y, reverse=False)
        #     )
        # )

    def set_optimizer(self) -> torch.optim.Adam:
        parameters = [
            {"params": self.model.mlp.parameters()},
            {"params": self.model.gp.covar_module.parameters()},
            {"params": self.model.gp.mean_module.parameters()},
            {"params": self.model.gp.likelihood.parameters()}
        ]
        return torch.optim.Adam(
            parameters, lr=self.problem.settings["dkl-gp"]["learning-rate"]
        )

    def fit_dkl_gp(self) -> NoReturn:
        self.model = initialize_dkl_gp(
            self.visited_x,
            self.visited_y,
            self.problem.settings["dkl-gp"]["mlp-output-dim"]
        )
        self.model.set_train()
        optimizer = self.set_optimizer()

        iterator = tqdm.trange(
            self.problem.settings["dkl-gp"]["max-traininig-epoch"],
            desc="Training DKL-GP"
        )
        mll = gpytorch.mlls.ExactMarginalLogLikelihood(
            self.model.gp.likelihood,
            self.model.gp
        )
        y = self.model.transform_ylayout(self.visited_y).squeeze(1)
        for i in iterator:
            optimizer.zero_grad()
            _y = self.model.train(self.visited_x)
            loss = -mll(_y, y)
            loss.backward()
            optimizer.step()
            iterator.set_postfix(loss=loss.item())
        self.model.set_eval()

    def eipv_suggest(self, batch: int = 1) -> NoReturn:
        partitioning = NondominatedPartitioning(
            ref_point=self.problem._ref_point.to(self.model.device),
            Y=self.visited_y.to(self.model.device)
        )

        acq_func = ExpectedHypervolumeImprovement(
            model=self.model.gp,
            ref_point=self.problem._ref_point.tolist(),
            partitioning=partitioning
        ).to(self.model.device)

        # randomly generate the design space
        self.problem.generate_design_space()
        acq_val = acq_func(
            self.model.forward_mlp(
                self.problem.x.to(torch.float).to(self.model.device)
            ).unsqueeze(1).to(self.model.device)
        ).to(self.model.device)
        top_acq_val, indices = torch.topk(acq_val, k=batch)
        new_x = self.problem.x[indices].to(torch.float32).reshape(-1, self.problem.n_dim)
        self.visited_x = torch.cat((self.visited_x, new_x), 0)
        y = self.problem.evaluate_true(new_x)
        self.visited_y = torch.cat((
                self.visited_y,
                self.problem.evaluate_true(new_x)
            ),
            0
        )

        # progressively record results
        write_txt(
            os.path.join(
                self.output_path,
                "part-x.rpt"
            ),
            np.array(self.visited_x),
            fmt="%i"
        )
        write_txt(
            os.path.join(
                self.output_path,
                "part-y.rpt"
            ),
            np.array(self.visited_y),
            fmt="%f"
        )

    def report(self):
        # gt = get_pareto_frontier(self.problem.total_y, reverse=False)
        pred = get_pareto_frontier(self.visited_y, reverse=False)
        # self.adrs.append(calc_adrs(gt, pred))
        optimal_solution = get_pareto_optimal_solutions(self.visited_x, self.visited_y)
        # info("pareto set: {}, size: {}, ADRS: {}".format(
        #         str(rescale_dataset(pred, perf_idx=-2, power_idx=-1)),
        #         len(pred),
        #         self.adrs[-1]
        #     )
        # )
        # p = self.problem.settings["report"]["path"]
        # mkdir(p)

        # plot_pareto_set(
        #     rescale_dataset(pred),
        #     gt=rescale_dataset(gt),
        #     design_space=self.problem.configs["dataset"]["path"],
        #     output=os.path.join(p, "report.pdf")
        # )
        # write_txt(
        #     os.path.join(
        #         p,
        #         "adrs.rpt"
        #     ),
        #     np.array(self.adrs),
        #     fmt="%f"
        # )
        self.visited_y[:, 1] = -self.visited_y[:, 1]
        self.visited_y[:, 2] = -self.visited_y[:, 2]
        pred[:, 1] = -pred[:, 1]
        pred[:, 2] = -pred[:, 2]
        write_txt(
            os.path.join(
                self.output_path,
                "x.rpt"
            ),
            np.array(self.visited_x),
            fmt="%i"
        )
        write_txt(
            os.path.join(
                self.output_path,
                "y.rpt"
            ),
            np.array(self.visited_y),
            fmt="%f"
        )
        write_txt(
            os.path.join(
                self.output_path,
                "pareto-frontier.rpt"
            ),
            np.array(pred),
            fmt="%f"
        )
        write_txt(
            os.path.join(
                self.output_path,
                "pareto-optimal-solutions.rpt"
            ),
            np.array(optimal_solution),
            fmt="%f"
        )
        self.model.save(
            os.path.join(
                self.output_path,
                "dkl-gp.mdl"
            )
        )


def create_solver(problem: DesignSpaceProblem) ->BOOMExplorerSolver:
    return BOOMExplorerSolver(problem)
