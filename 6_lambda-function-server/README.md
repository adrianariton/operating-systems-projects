# Dynamic Library Executor

### Adrian & Alexandru Ariton 323CA

This program is designed to execute functions from dynamic libraries (shared objects) in a controlled environment. It communicates with a client over a socket, receives commands specifying the library, function, and parameters, and executes the requested function. 

## Features

- Executes functions from dynamic libraries based on client commands.
- Limits the runtime of the executed function to prevent indefinite execution.
- Cleans up resources and handles timeouts gracefully.
- The execution is limited to a specified time, and the program handles cleanup even if the function execution exceeds the time limit.
- Terminating signals are handled with the following 2 errors:

```sh
SIGALRM function lasted too long # for timeout or exit(status), where status != 0
```
```sh
SISSEGV end! # for term calls
```

## Disclaimer

1. This was tested on the IOCLA virtual machine.
2. If all tests fail, `make clean && make` is advised in both src/ and tests/ and then `make check` in tests
3. Tests should be also ran individually if they dont all work in a bulk (we've encountered this before, but wethink we managed to fix it)

### Prerequisites

- The program requires a POSIX-compliant operating system.
- Ensure that you have a C compiler installed (e.g., GCC).