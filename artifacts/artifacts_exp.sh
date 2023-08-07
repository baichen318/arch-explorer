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
	cd ${cur_root}/fig2  && ./exp_fig2.sh
	cd ${cur_root}/fig3  && ./exp_fig3.sh
	cd ${cur_root}/fig12/spec06 && ./exp_fig12_spec06.sh
	cd ${cur_root}/fig12/spec17 && ./exp_fig12_spec17.sh
	cd ${cur_root}/fig13 && ./exp_fig13.sh
	cd ${cur_root}/fig14 && ./exp_fig14.sh
	cd ${cur_root}/fig15 && ./exp_fig15.sh
}

set_env
main

