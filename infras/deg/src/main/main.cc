/* Author: baichen.bai@alibaba-inc.com */


#include "o3.h"
#include "utils.h"
#include "riscv_instruction.h"


O3Graph* init(std::unordered_map<std::string, std::string> args) {
    return new O3Graph(
        args, new RiscvInstructionStream(args["trace"])
    );
}


int main(int argc, char const *argv[]) {
    auto args = parse_args(argc, argv); 
    init(args)->run();
    return 0;
}
