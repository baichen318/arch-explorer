#!/bin/bash
# Author: baichen.bai@alibaba-inc.com

set -x
arch_explorer_root=$(cd "$(dirname "$0")";pwd)
arch_explorer_root=${arch_explorer_root}/..
infras_root=${arch_explorer_root}/infras
calipers_root=${infras_root}/calipers
gem5_research_root=${infras_root}/gem5-research
gem5_research_pool_root=${infras_root}/gem5-research-pool-root
mcpat_research_root=${infras_root}/mcpat-research
deg_root=${infras_root}/deg
os="`uname`"


function get_cpu_count() {
	case ${os} in
		Darwin)
			cpu_count=`sysctl hw.physicalcpu | cut -d ' ' -f 2`
			;;
		Linux)
			cpu_count=`cat /proc/cpuinfo| grep "processor"| wc -l`
			;;
			*)
			echo "[INFO]: OS: ${os} is not supported."
			exit 1
	esac
}


function set_calipers() {
	cd ${calipers_root} && make -j`nproc --all`
}


function set_gem5_research_root() {
	# gem5-research
	cd ${gem5_research_root} && scons build/RISCV/gem5.opt -j`nproc --all`
}


function set_mcpat_research() {
	# mcpat-research
	cd ${mcpat_research_root} && make -j`nproc --all`
}


function set_deg() {
	# deg
	cd ${deg_root} && make -j`nproc --all`
}


function set_infras() {
	# set_calipers
	set_gem5_research_root
	set_mcpat_research
	# set_deg
}


set_infras
