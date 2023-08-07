# Author: baichen.bai@alibaba-inc.com


class NotFoundException(Exception):
    def __init__(self, target):
        self.msg = target + " is not found."

    def __str__(self):
        return self.msg


class UnSupportedException(Exception):
    def __init__(self, target):
        self.msg = target

    def __str__(self):
        return self.msg


class EvaluationRuntimeError(Exception):
    def __init__(self, msg):
        super(EvaluationRuntimeError, self).__init__()
        self.msg = msg

    def __str__(self):
        return self.msg
