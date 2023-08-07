# Author: baichen.bai@alibaba-inc.com


import os
import numpy as np
from typing import NoReturn
from utils.utils import load_xlsx, assert_error
from funcs.design.design_space import DesignSpace, Macros, \
    parse_design_space_sheet, parse_components_sheet


class O3CPUMacros(Macros):
    def __init__(self, components_mappings: dict, component_dims: list):
        self.components_mappings = components_mappings
        self.component_dims = component_dims
        super(O3CPUMacros, self).__init__()

    def get_mapping_params(self, vec: list, idx: int) -> list:
        return self.components_mappings[self.components[idx]][vec[idx]]

    def get_mapping_idx(self, params: list, idx: int) -> int:
        for k, v in self.components_mappings[self.components[idx]].items():
            if params == v:
                return k

    def vec_to_embedding(self, vec: list) -> list:
        embedding = []
        for i in range(len(vec)):
            embedding += self.get_mapping_params(vec, i)
        return embedding

    def embedding_to_vec(self, embedding: list) -> list:
        vec = []
        i = j = 0
        while i < self.dims:
            l = len(self.components_mappings[self.components[i]]["description"])
            vec.append(self.get_mapping_idx(embedding[j: j + l], i))
            i += 1
            j += l
        return vec


class O3CPUDesignSpace(DesignSpace, O3CPUMacros):
    def __init__(self, descriptions: dict, components_mappings: dict, size: int):
        """
            descriptions: <class "collections.OrderedDict">
            Example:
            descriptions = {
                "1-wide SonicBOOM": {
                    "Fetch": [4],
                    "Decoder": [1],
                    "ISU": [1, 2, 3],
                    "IFU": [1, 2, 3],
                    "ROB": [1, 2, 3, 4],
                    "PRF": [1, 2, 3],
                    "LSU": [1, 2, 3],
                    "I-Cache/MMU": [1, 2, 3, 4],
                    "D-Cache/MMU": [1, 2, 3, 4]
                }
            }

            components_mappings: <class "collections.OrderedDict">
            Example:
            components_mappings = {
                "Fetch": {
                    "description": ["FetchWidth"],
                    1: [4]
                },
                "Decoder": {
                    "description": ["decoderWidth"],
                    1: [1]
                },
                "ISU": {
                    "description": ["IQT_MEM.dispatchWidth", "IQT_MEM.issueWidth"
                        "IQT_MEM.numEntries", "IQT_INT.dispatchWidth",
                        "IQT_INT.issueWidth", "IQT_INT.numEntries",
                        "IQT_FP.dispatchWidth", "IQT_FP.issueWidth",
                        "IQT_FP.numEntries"
                    ],
                    1: [1, 1, 8, 1, 1, 8, 1, 1, 8],
                    2: [1, 1, 6, 1, 1, 6, 1, 1, 6]
                },
                "IFU": {
                    "description": ["maxBrCount", "numFetchBufferEntries", "ftq.nEntries"]
                    1: [8, 8, 16],
                    2: [6, 6, 14]
                }
            }

            size: the size of the entire design space
        """
        self.descriptions = descriptions
        """
            construct look-up tables
            self.designs: <list>
            ```example
                ["1-wide SonicBOOM", "2-wide SonicBOOM", "3-wide SonicBOOM"]
            ```
        """
        self.designs = list(self.descriptions.keys())
        """
            self.components: <list>
            ```example
                ["Fetch", "Decoder", "ISU", "IFU"]
            ```
        """
        self.components = list(self.descriptions[self.designs[0]].keys())
        self.design_size = self.construct_design_size()
        assert sum(self.design_size) == size, assert_error(
            "size is not matched: {} vs. {}".format(
                self.design_size,
                size
            )
        )
        """
            self.acc_design_size: <list> list of accumulated sizes of each design
            ```example
                self.design_size = [a, b, c, d, e] # accordingly
                self.acc_design_size = [a, a + b, a + b + c, a + b + c + d, a + b + c + d + e]
            ```
        """
        self.acc_design_size = np.cumsum(self.design_size).tolist()
        DesignSpace.__init__(self, size, len(self.components))
        O3CPUMacros.__init__(self, components_mappings, self.construct_component_dims())

    def construct_design_size(self) -> list:
        """
            design_size: <list> list of sizes of each design
            ```example
                [a, b, c, d, e] and np.sum([[a, b, c, d, e]]) = self.size
            ```
        """
        design_size = []
        for k, v in self.descriptions.items():
            _design_size = []
            for _k, _v in v.items():
                _design_size.append(len(_v))
            design_size.append(np.prod(_design_size))
        return design_size

    def construct_component_dims(self) -> list:
        """
            component_dims: <list> list of dimensions of each
                                   component w.r.t. each design
            ```example:
                [
                    [a, b, c, d, e], # => dimensions of components of the 1st design
                    [f, g, h, i, j]  # => dimensions of components of the 2nd design
                ]
            ```
        """
        component_dims = []
        for k, v in self.descriptions.items():
            _component_dims = []
            for _k, _v in v.items():
                _component_dims.append(len(_v))
            component_dims.append(_component_dims)
        return component_dims

    def valid(self, idx: int) -> NoReturn:
        assert idx > 0 and idx <= self.size, \
            assert_error("invalid index.")

    def idx_to_vec(self, idx: int) -> list:
        """
            idx: the index of a microarchitecture
        """
        self.valid(idx)
        idx -= 1
        vec = []
        design = np.where(np.array(self.acc_design_size) > idx)[0][0]
        if design:
            # NOTICE: subtract the offset
            idx -= self.acc_design_size[design - 1]
        for dim in self.component_dims[design]:
            vec.append(idx % dim)
            idx //= dim
        # add the offset
        for i in range(len(vec)):
            vec[i] = self.descriptions[self.designs[design]][self.components[i]][vec[i]]
        return vec

    def vec_to_idx(self, vec: list) -> int:
        """
            vec: the list of a microarchitecture encoding
        """
        idx = 0
        design = 0
        # subtract the offset
        for i in range(len(vec)):
            vec[i] = self.descriptions[self.designs[design]][self.components[i]].index(vec[i])
        for j, k in enumerate(vec):
            idx += int(np.prod(self.component_dims[design][:j])) * k
        if design:
            # NOTICE: add the offset
            idx += self.acc_design_size[design - 1]
        idx += 1
        self.valid(idx)
        return idx

    def idx_to_embedding(self, idx: int) -> list:
        vec = self.idx_to_vec(idx)
        return self.vec_to_embedding(vec)

    def embedding_to_idx(self, embedding: list) -> int:
        vec = self.embedding_to_vec(embedding)
        return self.vec_to_idx(vec)


def parse_o3cpu_design_space(design_space_xlsx: str) -> O3CPUDesignSpace:
    o3cpu_design_space_sheet = load_xlsx(
        design_space_xlsx, sheet_name="O3CPU"
    )
    components_sheet = load_xlsx(
        design_space_xlsx, sheet_name="Components"
    )
    # parse the design space
    descriptions = parse_design_space_sheet(
        o3cpu_design_space_sheet
    )
    # parse components sheet
    components_mappings = parse_components_sheet(
        components_sheet
    )
    return O3CPUDesignSpace(
        descriptions,
        components_mappings,
        int(o3cpu_design_space_sheet.values[0][-1])
    )


def parse_design_space(design_space_root: str) -> O3CPUDesignSpace:
    design_space_xlsx = os.path.abspath(
        os.path.join(
            os.path.dirname(__file__),
            os.path.pardir,
            os.path.pardir,
            os.path.pardir,
            "main",
            "configs",
            design_space_root
        )
    )
    return parse_o3cpu_design_space(
        design_space_xlsx
    )
