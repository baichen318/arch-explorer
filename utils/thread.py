# Author: baichen.bai@alibaba-inc.com


from threading import Thread
from utils.exceptions import EvaluationRuntimeError


class WorkerThread(Thread):
    def __init__(self, func, args):
        super(WorkerThread, self).__init__()
        self.func = func
        self.args = args

    def run(self):
        self.output = self.func(*self.args)

    def get_output(self):
        try:
            return self.output
        except Exception as e:
            raise EvaluationRuntimeError(str(e))
