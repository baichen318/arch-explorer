# ArchExplorer Artifacts Evaluation
	
## Figure 3 reproduction

Once you enter the docker image, execute following commands to reproduce Figure 3.

- Enter the sub-directory
```bash
$ cd /path/to/arch-explorer/artifacts/fig3
```

- Execution *w.* partial benchmarks

    If users do not reconfigure benchmarks in the docker image, users can still conduct experiments with following instructions.

	```bash
	$ ./exp_fig3_partial_benchmarks.sh
	```

- Execution *w.* full benchmarks

    If users reconfigure benchmarks in the docker image, users can conduct experiments with following instructions.

	```bash
	$ ./exp_fig3_full_benchmarks.sh
	```

- Results
```
$ open ./fig3_latex.pdf
```
