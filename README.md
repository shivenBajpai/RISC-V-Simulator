# RISC-V Simulator-Testor for CS2323 Computer Architecture

Build the project by running `make`
The binary is generated in `\bin`

# Command Line options:

### Required:

--input \<filename> <br>
-i \<filename>

The file with the code to be run

--tests \<num_cases> \<test_cases_file> <br>
-t \<num_cases> \<test_cases_file>

The number of test cases, and a file with each test case, as a single line that is interpreted as the data segment. 

### Optional

--cycles \<max-cycle-count> <br>
-c \<max-cycle-count>

Maximum number of cycles to run before stopping, for the purpose of catching infinite loops. If not specified, program will be run infinitely.

--regs <br>
-r

If specified, the registers will be dumped in the output

--mem \<start-address> \<end-address> <br> 
-m \<start-address> \<end-address>

If specified, the values in memory between the specified addresses will be dumped in the output
Memory data will be interpreted as a series of signed dwords
Might be funky if addresses are not dword aligned

