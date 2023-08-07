# https://github.com/shotahorii/bareml
# Author: baichen318@gmail.com


import numpy as np
from metrics import rmse
from abc import ABCMeta, abstractmethod


class Estimator(metaclass=ABCMeta):

    @abstractmethod
    def _fit(self, X, y, w):
        """ 
        Actual implementation of fit() function.
        Arguments can be (X,), (X, y) or (X, y, w)
        """
        pass
    
    @abstractmethod
    def _predict(self, X):
        """ 
        Actual implementation of predict() function.
        Argument must be (X,)
        """
        pass

    @abstractmethod
    def score(self, X, y):
        """ Returns evaluation metrics. """
        pass

    def fit(self, *args, **kwargs): 
        """ Train the model. """

        # validate given arguments
        n_args = len(args) + len(kwargs)
        if n_args > 3:
            raise ValueError('fit() takes up to 3 positional arguments but ' + str(n_args) + ' were given')
        if len(args) >= 1 and 'X' in kwargs.keys():
            raise ValueError('fit() got multiple values for argument "X"')
        if len(args) >= 2 and 'y' in kwargs.keys():
            raise ValueError('fit() got multiple values for argument "y"')
        if len(args) == 3 and 'w' in kwargs.keys():
            raise ValueError('fit() got multiple values for argument "w"')
        if not set(kwargs.keys()).issubset(set(('X','y','w'))):
            raise ValueError('fit() got unknown input values')
        
        # assign the given arguments to X, y, and/or w
        inputs = []
        for i, v in enumerate(('X','y','w')):
            if i < len(args):
                inputs.append(args[i])
            elif v in kwargs.keys():
                inputs.append(kwargs[v])

        # validate
        if len(inputs) == 1:
            inputs = [self._validate_X(*inputs)]
        elif len(inputs) == 2:
            inputs = self._validate_Xy(*inputs)
        elif len(inputs) == 3:
            inputs = self._validate_Xyw(*inputs)

        # perform fit
        return self._fit(*inputs) 

    def predict(self, X):
        """ Returns predicted value for input samples. """
        X = self._validate_X(X)
        return self._predict(X)

    def _validate_y(self, y):
        """ Validates input y for training. """
        return y

    def _validate_X(self, X):
        """ Validates input X for training. """
        X = np.array(X)

        if not np.issubdtype(X.dtype, np.number):
            raise ValueError('Data type of X needs to be numeric.')
        if X.ndim not in (1,2):
            raise ValueError('X needs to be a 1d array or 2d array.')
        if np.isnan(X).any():
            raise ValueError('There is at least 1 null element in X.')
        if np.isinf(X).any():
            raise ValueError('There is at least 1 inf/-inf element in X.')

        if X.ndim == 1:
            # convert to column vector 
            # e.g. [1,2,3] -> [[1],[2],[3]]
            X = np.expand_dims(X, axis=1) #X = X[:,None] 

        return X

    def _validate_w(self, w):
        """ Validates input weight for training. """
        if w is None:
            return w
            
        w = np.array(w)

        if not np.issubdtype(w.dtype, np.number):
            raise ValueError('Data type of w needs to be numeric.')
        if w.ndim != 1:
            raise ValueError('w needs to be a 1d array.')
        if np.isnan(w).any():
            raise ValueError('There is at least 1 null element in w.')
        if np.isinf(w).any():
            raise ValueError('There is at least 1 inf/-inf element in w.')
        if (w < 0).any():
            raise ValueError('w cannot be minus.')

        return w

    def _validate_Xy(self, X, y):
        """
        Validates input X and y for training. 
        Every fit() function must call this method 
        at the first line to validate input X and y.
        """
        X = self._validate_X(X)
        y = self._validate_y(y)

        if y is not None and len(X) != len(y):
            raise ValueError('Length of X and y need to be same.')

        return X, y

    def _validate_Xyw(self, X, y, w):
        """
        Validates input X and y for training. 
        Every fit() function must call this method 
        at the first line to validate input X and y.
        """
        X = self._validate_X(X)
        y = self._validate_y(y)
        w = self._validate_w(w)

        if y is not None and len(X) != len(y):
            raise ValueError('Length of X and y need to be same.')
        if w is not None and len(X) != len(w):
            raise ValueError('Length of X and w need to be same.')

        return X, y, w


class Regressor(Estimator):

    def score(self, X, y):
        """ Returns various evaluation metrics. """
        y_pred = self.predict(X)

        rmse_ = rmse(y, y_pred)
        mae_ = mae(y, y_pred)
        r2 = r_squqred(y, y_pred)

        return {'rmse': rmse_, 'mae': mae_, 'r_squared': r2}

    def _validate_y(self, y):
        y = np.array(y)

        if not np.issubdtype(y.dtype, np.number):
            raise ValueError('Data type of y needs to be numeric.')
        if y.ndim != 1:
            raise ValueError('y needs to be a 1d array.')
        if np.isnan(y).any():
            raise ValueError('There is at least 1 null element in y.')
        if np.isinf(y).any():
            raise ValueError('There is at least 1 inf/-inf element in y.')

        return y
