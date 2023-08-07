# ArchExplorer Artifacts Evaluation

## Figure 2 reproduction

Once you enter the docker image, execute the following commands to reproduce Figure 2.

- Enter the sub-directory
```bash
$ cd /path/to/arch-explorer/artifacts/fig2
```

- Execution *w.* partial benchmarks

    If users do not reconfigure benchmarks in the docker image, users can still conduct experiments with the following instructions.

	```bash
	$ ./exp_fig2_partial_benchmarks.sh
	```

- Execution *w.* full benchmarks

    If users reconfigure benchmarks in the docker image, users can conduct experiments with the following instructions.

	```bash
	$ ./exp_fig2_full_benchmarks.sh
	```

- Results
```
$ open ./fig2_latex.pdf
```
