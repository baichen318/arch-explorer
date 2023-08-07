FLAGS = -O3 -std=c++17
BUILD_BASE = build
SRC_BASE = src
SRC_DIRS = common trace graph memory branch_predictor tools

#-------------------------------------------------------------------------------------------------#

$(foreach src_dir, $(SRC_DIRS), $(eval SRCS += $(wildcard $(SRC_BASE)/$(src_dir)/*.cpp)))
INCS = $(addprefix -I$(SRC_BASE)/, $(SRC_DIRS))
OBJ_DIRS = $(addprefix $(BUILD_BASE)/, $(SRC_DIRS))
OBJS = $(SRCS:$(SRC_BASE)/%.cpp=$(BUILD_BASE)/%.o)
DEPS = $(OBJS:%.o=%.d)

$(BUILD_BASE)/calipers: $(OBJS)
	$(CXX) $(FLAGS) -o $@ $^

$(BUILD_BASE)/%.o: $(SRC_BASE)/%.cpp
	$(CXX) $(FLAGS) $(INCS) -MMD -MP -c -o $@ $<

$(OBJS): | $(OBJ_DIRS)

$(OBJ_DIRS): | $(BUILD_BASE)
	mkdir -p $(OBJ_DIRS)

$(BUILD_BASE):
	mkdir -p $(BUILD_BASE)

.PHONY: clean

clean:
	rm -rf $(BUILD_BASE)

ooo-demo:
	build/calipers demo/trace-OoO.cfg demo/sample2.trace demo/debug.rpt
	sfdp -Kfdp -x -Gsplines=true -Goverlap=scale -Tpdf fig.dot > fig.pdf

run:
	build/calipers demo/trace-OoO.cfg /Volumes/Research/repo/gem5-research/m5out/debug demo/test.rpt
	sfdp -Kfdp -x -Gsplines=true -Goverlap=scale -Tpdf fig.dot > fig.pdf

-include $(DEPS)
