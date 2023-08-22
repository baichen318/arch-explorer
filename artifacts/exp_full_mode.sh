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
	cd ${cur_root}/fig2  && ./exp_fig2_full_mode.sh
	cd ${cur_root}/fig3  && ./exp_fig3_full_mode.sh
	cd ${cur_root}/fig12/spec06 && ./exp_fig12_full_mode.sh
	cd ${cur_root}/fig12/spec17 && ./exp_fig12_full_mode.sh
	cd ${cur_root}/fig13 && ./exp_fig13.sh
	cd ${cur_root}/fig14 && ./exp_fig14_full_mode.sh
	cd ${cur_root}/fig15 && ./exp_fig15_full_mode.sh
}

set_env
main

