#!/bin/bash
# Author: baichen.bai@alibaba-inc.com


set -e
set -o pipefail
cur_root="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function set_env() {
	function handler() {
		exit 1
    }
    trap 'handler' SIGINT
}


function main() {
	`which python3` ${cur_root}/exp_fig2.py -c ${cur_root}/configs/exp-fig2-partial-benchmarks.yml
}
	
set_env
main

