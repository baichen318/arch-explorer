## Configuration

Two sample configuration files are provided here: `InO.cfg` and `OoO.cfg` (for the in-order and
out-of-order processor models, respectively). The common configuration parameters are:
- `ISA`: Currently, only `RISC-V` is acceptable.
- `Core`: Can be `InO` or `OoO`. These processor models are based on
[gem5](https://www.gem5.org/)'s *MinorCPU* and *DerivO3CPU* models, respectively.
- `Branch_Predictor`: Can be `TraceB` (when branch prediction information is provided in the
trace) or `StatisticalB` (when the *statistical* model is used).
- `Branch_Predictor_Config`: Used for configuring the branch predictor when a model (rather than
the trace) is used.
- `I_Cache`/`D_Cache`: Can be `TraceC` (when load/store information is provided in the
trace) or `IdealC`/`StatisticalC`/`RealC` (when the *ideal*/*statistical*/*real* model is used).
- `I_Cache_Config`/`D_Cache_Config`: Used for configuring the I/D-cache when a model (rather than
the trace) is used.

Further configuration parameters specify other aspects of the core, which may be used in one
model but not in another.

## Trace

Two sample trace files are provided here: `sample1.trace` and `sample2.trace`.
The traces are text-based and include the following for each instruction:
- `@I disassembled_instruction [@A base_address]`: The instruction and the base address of
accessed data in the case of loads/stores
- `@F fetch_ticks`: Clock ticks<sup>\*</sup> spent on fetching this instruction
(required when an I-cache model is not used)
- `@B prediction_correctness`: Correctness of branch prediction (required when a branch prediction
model is not used): 0 if mispredicted; 1 if correctly predicted or the instruction is not a branch
- `@M memory_access_ticks`: Clock ticks<sup>\*</sup> spent on accessing the memory (required for
loads/stores and when a D-cache model is not used)

Note that `sample1.trace` should be used with `InO.cfg`, because this configuration specifies
that branch prediction and load/store information are provided along with the trace.
Also, `sample2.trace` (where all trace lines start with `@I`) should be used with `OoO.cfg`,
because this configuration specifies that particular branch prediction and cache models are used.

<sup>\*</sup> The number of ticks per cycle is defined in
[calipers_defs.h](../src/common/calipers_defs.h).
