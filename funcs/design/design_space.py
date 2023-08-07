# Author: baichen.bai@alibaba-inc.com


import os
import abc
import numpy as np
from collections import OrderedDict


class DesignSpace(abc.ABC):
	def __init__(self, size: int, dims: int):
		"""
			size: total size of the design space
			dims: dimension of a microarchitecture embedding
		"""
		self.size = size
		self.dims = dims

	@abc.abstractmethod
	def idx_to_vec(self, idx: int) -> list:
		"""
			transfer from an index to a vector
		"""
		raise NotImplementedError()

	@abc.abstractmethod
	def vec_to_idx(self, vec: list) -> int:
		"""
			transfer from a vector to an index
			vec: vector index in the design space table
		"""
		raise NotImplementedError()


class Macros(abc.ABC):
	def __init__(self):
		self.macros = {}
		self.macros["arch-explorer-root"] = os.path.abspath(
			os.path.join(
				os.path.dirname(__file__),
				os.path.pardir,
				os.path.pardir
			)
		)

	@abc.abstractmethod
	def get_mapping_params(self, vec: list, idx: int) -> int:
		raise NotImplementedError()


def parse_design_space_sheet(design_space_sheet: object) -> dict:
	descriptions = OrderedDict()
	head = design_space_sheet.columns.tolist()

	# parse design space
	for row in design_space_sheet.values:
		# extract designs
		descriptions[row[0]] = OrderedDict()
		# extract components
		for col in range(1, len(head) - 1):
			descriptions[row[0]][head[col]] = []
		# extract candidate values
		for col in range(1, len(head) - 1):
			try:
				# multiple candidate values
				for item in list(map(lambda x: int(x), row[col].split(','))):
					descriptions[row[0]][head[col]].append(item)
			except AttributeError:
				# single candidate value
				descriptions[row[0]][head[col]].append(row[col])
	return descriptions


def parse_components_sheet(components_sheet: object) -> dict:
	"""
		construct look-up tables
		mappings: <list> [name, width, idx]
		```example
			mappings = [("ISU", 10, 0), ("IFU", 4, 10), ("ROB", 2, 14)]
		```
	"""

	components_mappings = OrderedDict()
	head = components_sheet.columns.tolist()
	mappings = []
	for i in range(len(head)):
		if not head[i].startswith("Unnamed"):
			if i == 0:
				name, width, idx = head[i], 1, i
			else:
				mappings.append((name, width, idx))
				name, width, idx = head[i], 1, i
		else:
			width += 1
	mappings.append((name, width, idx))

	for name, width, idx in mappings:
		# extract components
		components_mappings[name] = OrderedDict()
		# extract descriptions
		components_mappings[name]["description"] = []
		for i in range(idx + 1, idx + width):
			components_mappings[name]["description"].append(components_sheet.values[0][i])
		# extract candidate values
		# get the number of rows (a trick to test <class "float"> of nan)
		nrow = np.where(
			components_sheet[name].values == components_sheet[name].values
		)[0][-1]
		for i in range(1, nrow + 1):
			components_mappings[name][int(i)] = list(components_sheet.values[i][idx + 1: idx + width])
	return components_mappings
