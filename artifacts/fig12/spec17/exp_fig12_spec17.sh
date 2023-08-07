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
	`which python3` ${cur_root}/exp_fig12_spec17.py
}

set_env
main

