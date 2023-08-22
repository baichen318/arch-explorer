#!/bin/bash
# Author: baichen.bai@alibaba-inc.com


set -e
set -o pipefail
cur_root="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
archexplorer_root="${cur_root}/../../"

function set_env() {
	function handler() {
		exit 1
    }
    trap 'handler' SIGINT
}


function main() {
	`which python3` ${cur_root}/exp_fig15.py \
		-c ${cur_root}/configs/exp-fig15-partial-spec06.yml \
		-c ${cur_root}/configs/exp-fig15-partial-spec17.yml
}

set_env
main

