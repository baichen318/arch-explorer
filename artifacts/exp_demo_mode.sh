#!/bin/bash
# Author: baichen.bai@alibaba-inc.com


set -e
set -o pipefail
cur_root="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
archexplorer_root="${cur_root}/../"


function set_env() {
	function handler() {
		exit 1
    }
    trap 'handler' SIGINT
}


function main() {
	cd ${cur_root}/fig2  && ./exp_fig2_demo_mode.sh
	cd ${cur_root}/fig3  && ./exp_fig3_demo_mode.sh
}

set_env
main

