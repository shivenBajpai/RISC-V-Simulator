# RISC-V Simulator for CS2323 Computer Architecture

This project is a simple RISC-V simulator.

### Build instructions
To build this project, the development libraries for ncurses need to be installed using the command:

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

## File Structure

```
/
+-- bin
+-- build
+-- src
|   +-- assembler                       (files from previous Lab assignment)
|   |   +-- assembler.c
|   |   +-- assembler.h
|   |   +-- index.c
|   |   +-- index.h
|   |   +-- translator.c
|   |   +-- translator.h
|   |   +-- vec.c
|   |   +-- vec.h
|   +-- backend                         (implementation of the simulator)
|   |   +-- backend.c
|   |   +-- backend.h
|   |   +-- memory.c
|   |   +-- memory.h
|   |   +-- stacktrace.c
|   |   +-- stacktrace.h
|   +-- frontend                        (ncurses frontend)
|   |   +-- frontend.c
|   |   +-- frontend.h
|   +-- main.c                          (main loop, initialization, memory management)
|   +-- globals.c                       (some globals)
|   +-- globals.h
+-- report
|   +-- report.tex
|   +-- report.pdf
+-- .gitignore
+-- LICENSE
+-- Makefile
+-- README.md
```