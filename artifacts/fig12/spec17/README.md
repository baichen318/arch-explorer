# ArchExplorer Artifacts Evaluation

## Fig. 12 SPEC CPU2017 reproduction

Assume you are in `arch-explorer/artifacts/fig12/spec17`.

### **Partial mode**

- Execution

	If users do not reconfigure benchmarks in the docker image, users can still conduct experiments with following instructions by skipping benchmarks that require hard-coded paths.

	Different results from the original manuscript are expected.
	```bash
	$ ./exp_fig12_partial_mode.sh
	```

- Results of **partial mode**
```
$ open archranker-partial-spec17.pdf
$ open adaboost-partial-spec17.pdf
$ open boom_explorer-partial-spec17.pdf
$ open archexplorer-partial-spec17.pdf
```
	
### **Full mode**

- Execution

    If users reconfigure benchmarks in the docker image successfully, users can conduct experiments with following instructions.

	```bash
	$ ./exp_fig12_full_mode.sh
	```

- Results of **full mode**
```
$ open archranker-full-spec17.pdf
$ open adaboost-full-spec17.pdf
$ open boom_explorer-full-spec17.pdf
$ open archexplorer-full-spec17.pdf
```