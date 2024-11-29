# RISC-V Simulator cum Tester for CS2323 Computer Architecture

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

--output \<code> <br>
-o \<code>

How to interpret the values in memory when dumping. By default, it interprets as signed ints.
You can use the following values of code:
0 - Signed int
1 - Unsigned int
2 - Hex

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

# Output format:

For every case, Output will be printed on 1 or more lines, depending on the options set.
Each line has either register/memory/cache information
The order is registers first, then memory, then cache (note: cache dumping is not implemented yet)

# A Quick Guide to error messages:

All error messages due to a problem in input code/exception while testing will come in the format:

```
Supervisor: While running testcase <t> (step <s>) An Error Occured:
<some_error_message>
```

If the error does not have the Header that indicates that the problem may be with the code being tested or possibly a bug in the program itself

The numbers \<t> \<s> provide some information as to when the error occured

| t | s | problem |
|---|---|---------|
| 0 | 0 | Problem occurred while trying to assemble input code |
| t | 1 | Problem occurred while trying to read the testcase for testcase t |
| t | 2 | Problem occurred while trying to run code for testcase t |