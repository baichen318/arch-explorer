# Author: baichen.bai@alibaba-inc.com


## DEBUG options:
# DEBUG_PARSE_ARGS: debug arguments parsing
# DEBUG_INST_STREAM: debug instruction stream
# DEBUG_CRITICAL_PATH: debug critical path

FLAGS = -std=c++17 -O3 -I/opt/homebrew/Cellar/boost/1.80.0/include \
	-L/opt/homebrew/Cellar/boost/1.80.0/lib \
	-lboost_filesystem -lboost_system
BUILD_BASE = build
SRC_BASE = src
SRC_DIRS = main common instruction graph


$(foreach src_dir, $(SRC_DIRS), $(eval SRCS += $(wildcard $(SRC_BASE)/$(src_dir)/*.cc)))
INCS = $(addprefix -I$(SRC_BASE)/, $(SRC_DIRS))
OBJ_DIRS = $(addprefix $(BUILD_BASE)/, $(SRC_DIRS))
OBJS = $(SRCS:$(SRC_BASE)/%.cc=$(BUILD_BASE)/%.o)
DEPS = $(OBJS:%.o=%.d)

$(BUILD_BASE)/deg: $(OBJS)
	$(CXX) $(FLAGS) -o $@ $^

$(BUILD_BASE)/%.o: $(SRC_BASE)/%.cc
	$(CXX) $(FLAGS) $(INCS) -MMD -MP -c -o $@ $<

$(OBJS): | $(OBJ_DIRS)

$(OBJ_DIRS): | $(BUILD_BASE)
	mkdir -p $(OBJ_DIRS)

$(BUILD_BASE):
	mkdir -p $(BUILD_BASE)

.PHONY: clean

clean:
	rm -rf $(BUILD_BASE)

run:
	build/deg -t example/instruction-flow -o example/report -v -s 1 -e 100
	neato -Tpdf example/report.dot > example/report.pdf

-include $(DEPS)
