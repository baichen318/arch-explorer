# ArchExplorer Artifacts Evaluation

## Fig. 14 reproduction

Once you enter the docker image, execute following commands to reproduce Figure 14.

- Enter the sub-directory
```bash
$ cd arch-explorer/artifacts/fig14
```

### **Partial mode**

- Execution

	If users do not reconfigure benchmarks in the docker image, users can still conduct experiments with following instructions by skipping benchmarks that require hard-coded paths.

	Different results from the original manuscript are expected.
	```bash
	$ ./exp_fig14_partial_mode.sh
	```

- Results of **partial mode**
```
$ open fig14_partial_mode.pdf
```

### **Full mode**

- Execution

    If users reconfigure benchmarks in the docker image successfully, users can conduct experiments with following instructions.

	```bash
	$ ./exp_fig14_full_mode.sh
	```

- Results of **full mode**
```
$ open fig14_full_mode.pdf
```
