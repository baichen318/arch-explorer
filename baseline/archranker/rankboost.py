# from https://github.com/pcoving/KDDCup/blob/master/train.py
import numpy as np
from sklearn.tree import DecisionTreeClassifier
from sklearn.ensemble._base import BaseEnsemble
from sklearn.base import BaseEstimator
from sklearn.tree import DecisionTreeRegressor

#from itertools import tee, izip
from itertools import tee
import sys

class Stump(BaseEstimator):

    def __init__(self):
        self.featureidx = None
        self.splitval = None
        self.above = None

    # just a little helper function
    def pairwise(self, iterable):
        a, b = tee(iterable)
        next(b, None)
        return zip(a, b)

    def fit(self, X, y, X_argsorted=None, sample_weight=None, npositive=None):

        mysample_weight = np.copy(sample_weight)
        mysample_weight[npositive:] *= -1

        mysample_weight_sum = np.sum(mysample_weight)

        # want to choose split that maximizes E+[h(x+)] - E-[h(x-)]
        max_val = 0.0
        for fidx in range(X.shape[1]):
            val = 0.0
            # iterate over pairs of sorted X values
            for xidx, xidx2 in self.pairwise(X_argsorted[:,fidx]):
                val += mysample_weight[xidx]
                # can only split on boundaries between values
                if X[xidx, fidx] != X[xidx2, fidx]:
                    if val > max_val:
                        max_val = val
                        self.featureidx = fidx
                        self.splitval = X[xidx, fidx]
                        self.above = False
                    if (mysample_weight_sum-val) > max_val:
                        max_val = mysample_weight_sum-val
                        self.featureidx = fidx
                        self.splitval = X[xidx, fidx]
                        self.above = True


    def predict(self, X):
        if self.above:
            y = [ int(x[self.featureidx] > self.splitval) for x in X ]
        else:
            y = [ int(x[self.featureidx] <= self.splitval) for x in X ]

        return np.asarray(y)

class BipartiteRankBoost(BaseEnsemble):

    def __init__(self,
                 n_estimators=50,
                 learning_rate=1.,
                 verbose=0,
                 base_estimator=Stump()):

        if base_estimator==None:
            base_estimator=Stump()
        super(BipartiteRankBoost, self).__init__(
            #base_estimator=DecisionTreeClassifier(max_depth=3,
            #                                      min_samples_split=2,
            #                                      min_samples_leaf=1,
            #                                      max_features=None),
            #base_estimator=DecisionTreeClassifier(max_depth=1),
            base_estimator=Stump(),
            n_estimators=n_estimators,
            estimator_params=tuple())
        self.base_estimator = base_estimator
        self.classes_ = [0,1]
        self.base_estimator_ = base_estimator
        self.estimator_weights_ = None
        self.learning_rate = learning_rate
        self.verbose = verbose


    def fit(self, X, y):

        X, y = np.array(X), np.array(y)

        # (0,1) class labels hard-coded for now..
        assert np.max(y) == 1
        assert np.min(y) == 0

        # first sort on y, positive examples come first...
        sortidx = y.argsort()[::-1]
        X, y = X[sortidx], y[sortidx]

        npositive = sum([1 for val in y if val == 1])

        # Initialize weights to 1 / n_samples
        sample_weight = np.empty(X.shape[0], dtype=np.float)
        sample_weight[:npositive] = 1. / npositive
        sample_weight[npositive:] = 1. / (X.shape[0]-npositive)

        self.estimators_ = []
        self.estimator_weights_ =  np.zeros(self.n_estimators, dtype=np.float)

        # Create argsorted X for fast tree induction
        X_argsorted = np.asfortranarray(np.argsort(X.T, axis=1).astype(np.int32).T)

        #for iboost in xrange(self.n_estimators):
        for iboost in list(range(self.n_estimators)):
            sample_weight, estimator_weight = self._boost(iboost,
                                                          X, y,
                                                          sample_weight,
                                                          X_argsorted=X_argsorted,
                                                          npositive=npositive)

            self.estimator_weights_[iboost] = estimator_weight

            # renormalize
            sample_weight[:npositive] /= np.sum(sample_weight[:npositive])
            sample_weight[npositive:] /= np.sum(sample_weight[npositive:])

        return self

    def _boost(self, iboost, X, y, sample_weight, X_argsorted, npositive):

        estimator = self._make_estimator()

        if self.verbose == 2:
            #print('building stump', iboost+1, 'out of', self.n_estimators)
            print("building stump {} out of {}".format(iboost+1, self.n_estimators))
        elif self.verbose == 1:
            sys.stdout.write('.')
            sys.stdout.flush()
        estimator.fit(X, y, sample_weight=sample_weight,
                      X_argsorted=X_argsorted, npositive=npositive)

        y_predict = estimator.predict(X)

        positive_error = np.average(y_predict[:npositive], weights=sample_weight[:npositive], axis=0)
        negative_error = np.average(y_predict[npositive:], weights=sample_weight[npositive:], axis=0)

        estimator_weight = self.learning_rate*np.log(positive_error/negative_error)/2. #set alphat

        sample_weight[:npositive] *= np.exp(-estimator_weight*y_predict[:npositive])
        sample_weight[npositive:] *= np.exp(estimator_weight*y_predict[npositive:])

        return sample_weight, estimator_weight

    def predict_proba(self, X):

        # not actually probability - no need to normalize!

        X = np.array(X)

        y_predict = np.zeros(X.shape[0])

        for weight, estimator in zip(self.estimator_weights_, self.estimators_):
            y_predict += estimator.predict(X)*weight
        y_predict /= sum(self.estimator_weights_)
        # to make it compatible with the predict_proba of other classifieres,
        # output two identical columns...
        #return np.asarray([y_predict, y_predict]).T
        return np.asarray(y_predict)
