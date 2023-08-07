# https://github.com/shotahorii/bareml
# Author: baichen318@gmail.com


import numpy as np
from copy import deepcopy
from regressor import Regressor
from abc import ABCMeta, abstractmethod
from metrics import absolute_relative_errors
from decision_tree import DecisionTreeRegressor


class Ensemble(metaclass=ABCMeta):

    @abstractmethod
    def __init__(self, estimators=[], base_estimator=None):
        self.estimators = estimators
        self.base_estimator = base_estimator

    def _make_estimator(self, append=True):
        estimator = deepcopy(self.base_estimator)
        
        if append:
            self.estimators.append(estimator)
        
        return estimator


class AdaBoostRT(Ensemble, Regressor):
    """
    AdaBoost.RT Regressor 
    Implementation based on the refference [7].
    """

    def __init__(self, threshold=0.05, max_iter=10, estimator=DecisionTreeRegressor(max_depth=1)):
        super().__init__(base_estimator=estimator)
        self.threshold = threshold
        self.max_iter = max_iter
        self.betas = []

    def _fit(self, X, y):

        # init weights
        w = np.full(len(y), 1/len(y))

        for _ in range(self.max_iter):
            print(_)
            reg = self._make_estimator()
            y_pred = reg.fit(X, y, w).predict(X)

            # compute absolute relative error for each sample
            are = absolute_relative_errors(y, y_pred)

            # assign 1 to the samples where the absolute relative error > threshold
            # and assign 0 to the samples where the absolute relative error <= threshold
            y_error = (are > self.threshold).astype(int)

            # calculate weighted error
            epsilon = np.sum(y_error * w) / np.sum(w)

            # calculate alpha: how bad this prediction is.
            beta = epsilon ** 2

            # assign 0 to the samples where the absolute relative error > threshold
            # and assign 1 to the samples where the absolute relative error <= threshold
            y_correct = (are <= self.threshold).astype(int)

            # update weights
            # w * beta for samples correctly predicted, w * 1 for samples wrongly predicted.
            w = w * np.power(beta, y_correct)
            w = w / np.sum(w) # normalisation

            # store m-th beta
            self.betas.append(beta)

        return self

    def _predict(self, X):

        # y_preds.shape is (max_iter, len(X))
        y_preds = np.array([reg.predict(X) for reg in self.estimators])
        
        # weighted majority vote
        y_pred = np.sum(np.log(1.0 / self.betas)[:,None] * y_pred, axis=0) / np.log(1.0/self.betas).sum()

        return y_pred
