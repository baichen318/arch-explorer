# ArchExplorer Artifacts Evaluation
	
## Figure 3 reproduction

Once you enter the docker image, execute following commands to reproduce Figure 3.

- Enter the sub-directory
```bash
$ cd arch-explorer/artifacts/fig3
```

### **Demo mode**

- Execution

    If users do not have tolerance for long runtime, we provide **demo mode** to quickly run through our whole flow.

    Different results from the original manuscript are expected.

	```bash
	$ ./exp_fig3_demo_mode.sh
	```

### **Partial mode**

- Execution

    If users do not reconfigure benchmarks in the docker image, users can still conduct experiments with following instructions by skipping benchmarks that require hard-coded paths.

	Different results from the original manuscript are expected.
	```bash
	$ ./exp_fig3_partial_mode.sh
	```

### **Full mode**

- Execution

    If users reconfigure benchmarks in the docker image successfully, users can conduct experiments with following instructions.

	```bash
	$ ./exp_fig3_full_mode.sh
	```

### Results
```
$ open ./fig3.pdf
```
