# Author: baichen.bai@alibaba-inc.com


import os
import abc


class Benchmark(abc.ABC):
    def __init__(self, name):
        super(Benchmark, self).__init__()
        self._macros = {}
        self._name = name

    @property
    def macros(self):
        return self._macros

    @property
    def name(self):
        return self._name

    @abc.abstractmethod
    def __len__(self):
        raise NotImplementedError()

    @abc.abstractmethod
    def __iter__(self):
        raise NotImplementedError()

    @abc.abstractmethod
    def __getitem__(self, item):
        raise NotImplementedError()

    @abc.abstractmethod
    def create_single_benchmark_macros(
        self, macros: dict, name: str
    ):
        """
            Create candidate workloads.
        """
        raise NotImplementedError()

    @abc.abstractmethod
    def create_complete_benchmark_macros(
        self, macros: dict
    ):
        """
            Create complete workloads.
        """
        raise NotImplementedError()
        