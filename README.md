# Pin2020

A tool using Intel's Pin framework to get the instruction trace of a program.

## Motivation
This tool was created to provide execution traces for analysis and benchmarking of different architectural implementations.

## Build
- Built on Ubuntu 19.1
- Pin Version 3.11

## Usage

To Build: 
- Replace the default MyPinTool.cpp file in source/tools/MyPinTool with the one found in this repository.
- The pintool can then be compiled using:
    > /MyPinTool$ make MyPinTool.test
    
To Use:
>../../../pin -t obj-intel64/MyPinTool.so -skip 400000 -num 50000 -reg 0 -opcode 0 -disassemble 1 -- /bin/ls

- Point to the pin executabl
>>../../../pin

- Point to the binary of this pintool
>>obj-intel64/MyPinTool.so

- The number of instruction to skip before printing.
>>-skip value

- The number of instructions to print.
>>-num value

-0 to print the registers as numbers, 1 to print the short sring name of the registers.
>>-reg 0/1

-0 to print the XED opcode integer, 1 to print the short string opcode name.
>>-opcode 0/1

-1 to print the dissasembled instructions (in program order) to instructions.out file.
>>-dissasemble 0/1

-The execution path and arguments to the program to be instrumented.
>>-- execuition/path/to/program

## Output
The examples files given show the output when opcode, disassemble and reg are all set to 1 (true).  
Below is an example of what an instruction output with reg and opcode set to 0 (false) would look like.   
**Note:** dissasemble does not affect the trace output.

>1 0x7f2fdb612f57 7 109 0 3 26 8 54 4 56 4 1 139843522176160 4 1 139843522176164 4 2 56 4 25 4    
*This is not a real instruction and just for deminstration only*

The format is as follows:

execute instruction_pointer instruction_size opcode Num_RegR Reg_name reg_size num_memR mem_addr mem_size num_MemW mem_addr mem_size num_RegW reg_name reg_size
