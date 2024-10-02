# RISC-V Simulator for CS2323 Computer Architecture

This project is a simple RISC-V simulator.

### Build instructions
To build this project, the development libraries for ncurses need to be installed using the command:

```bash
sudo apt install libncurses-dev
```

Then build the project by running `make`
The binary is generated in `/bin`

## Usage notes for TA's

Since this simulator has a TUI, It made sense to make alter some details of the commands. While full details are available in the appendix of the report, here is the short version:

Line numbering starts from .text section. This is not difficult to use as argued in the moodle thread asking for change as line numbers are shown on screen.
del-break and break are in the same command, it just toggles the breakpoint

The run command runs live at a pace of about 5 instructions per second.
Stop execution and hold down F8(step command shortcut) to execute it much faster.

The mem command does not take a count argument. This is because the memory is displayed in a scrollable pane and so it doesn't make sense to have a count argument as all lines are always visible anyways.

regs is redundant as registers are always visible

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