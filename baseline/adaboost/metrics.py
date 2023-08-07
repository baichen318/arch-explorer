# https://github.com/shotahorii/bareml
# Author: baichen318@gmail.com


import numpy as np


def variance(y, w=None):
    """ 
    Computes variance of the given list of real numbers.
    
    Parameters
    ----------
    y: np.ndarray (n,)

    w: np.ndarray (n, )
        weights for y

    Returns
    -------
    float
        variance
    """
    # if weights are not given, consider weights are all 1.
    w = np.ones(len(y)) if w is None else w

    mu = np.mean(y)
    #var = np.mean(np.power(y-mu,2)) # non-weighted implementation
    var = np.average(np.power(y - mu,2), weights=w)
    return var


def mean_deviation(y, w=None):
    """ 
    Computes mean deviation of the given list of real numbers.
    
    Parameters
    ----------
    y: np.ndarray (n,)

    w: np.ndarray (n, )
        weights for y

    Returns
    -------
    float
        mean deviation
    """

    # if weights are not given, consider weights are all 1.
    w = np.ones(len(y)) if w is None else w

    mu = np.mean(y)
    #md = np.mean(np.abs(y-mu)) # non-weighted implementation
    md = np.average(np.abs(y - mu), weights=w)
    return md


def squared_errors(y, y_pred):
    """ 
    Computes squared error for each sample.
    
    Parameters
    ----------
    y: np.ndarray (1d array)
        Target variable of regression problems.
        Number of elements is the number of data samples. 
    
    y_pred: np.ndarray (1d array)
        Predicted values for the given target variable. 
        Number of elements is the number of data samples. 

    Returns
    -------
    np.ndarray (1d array)
        squared error for each sample
    """
    return np.power(y - y_pred, 2)


def mse(y, y_pred):
    """ 
    Computes mean squared error.
    
    Parameters
    ----------
    y: np.ndarray (1d array)
        Target variable of regression problems.
        Number of elements is the number of data samples. 
    
    y_pred: np.ndarray (1d array)
        Predicted values for the given target variable. 
        Number of elements is the number of data samples. 

    Returns
    -------
    float
        mean squared error 
    """
    return np.mean(squared_errors(y, y_pred))


def rmse(y, y_pred):
    """ 
    Computes root mean squared error.
    
    Parameters
    ----------
    y: np.ndarray (1d array)
        Target variable of regression problems.
        Number of elements is the number of data samples. 
    
    y_pred: np.ndarray (1d array)
        Predicted values for the given target variable. 
        Number of elements is the number of data samples. 

    Returns
    -------
    float
        root mean squared error 
    """
    return np.sqrt(mse(y, y_pred))


def absolute_errors(y, y_pred):
    """ 
    Computes absolute error for each sample.
    
    Parameters
    ----------
    y: np.ndarray (1d array)
        Target variable of regression problems.
        Number of elements is the number of data samples. 
    
    y_pred: np.ndarray (1d array)
        Predicted values for the given target variable. 
        Number of elements is the number of data samples. 

    Returns
    -------
    np.ndarray (1d array)
        absolute error for each sample
    """
    return np.abs(y - y_pred)


def mae(y, y_pred):
    """ 
    Computes mean absolute error.
    
    Parameters
    ----------
    y: np.ndarray (1d array)
        Target variable of regression problems.
        Number of elements is the number of data samples. 
    
    y_pred: np.ndarray (1d array)
        Predicted values for the given target variable. 
        Number of elements is the number of data samples. 

    Returns
    -------
    float
        mean absolute error 
    """
    return np.mean(absolute_errors(y, y_pred))


def absolute_relative_errors(y, y_pred):
    """ 
    Computes absolute relative error for each sample.
    
    Parameters
    ----------
    y: np.ndarray (1d array)
        Target variable of regression problems.
        Number of elements is the number of data samples. 
    
    y_pred: np.ndarray (1d array)
        Predicted values for the given target variable. 
        Number of elements is the number of data samples. 

    Returns
    -------
    np.ndarray (1d array)
        absolute relative error for each sample 
    """
    return np.abs((y - y_pred) / y)


def rss(y, y_pred):
    """ residual sum of squares """
    return np.sum(squared_errors(y, y_pred))


def r_squqred(y, y_pred):
    denom = np.sum(np.power(y - y.mean(), 2))
    return 1 - rss(y, y_pred) / denom
