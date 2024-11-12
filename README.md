# RISC-V Simulator for CS2323 Computer Architecture

This project is a Terminal-based RISC-V Simulator.
Within the TUI you can load in files and run them, continuously or stepwise.
It supports common features like breakpoints, stack-traces, being able to see memory state, etc.
It also has a builtin cache-simulator

See the appendix of the report in `/report/report.pdf` for detailed usage instructions

### Build instructions
To build this project, the development libraries for ncurses need to be installed:

On Ubuntu you can install it via:

```bash
sudo apt install libncurses-dev
```

Then build the project by running `make`
The binary is generated in `/bin`

## Command Line Options

`--smc`
`--self-modifying-code`
Allows the code to overwrite the text section. Note that the code shown will not update and not be accurate.
If this switch is not present, Trying to overwrite text section will fail.

A more detailed report on the design and features of this simulator is present in `report.pdf` in `/report`