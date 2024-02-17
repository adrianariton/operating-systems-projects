# Operating Systems Related Projects
Check out [the official course website](https://cs-pub-ro.github.io/operating-systems/) @ Polytehnica University of Bucharest

1. Mini Libc
2. Memory Allocator
3. Thread Pool
4. Mini Shell using syscalls
5. Async Web Server for file retrieval using HTTP requests
6. Lambda Server that executes function in dynamic libraries on request

## Running 1-4

Check out the readme in each of the folders.
Hint: Use make check to test if everything works fine!

## Running Web Server

- Go to 5_web-server-async
- Run make command from src or tests and the start server with ./aws
- Request file as such (from a different terminal window):
```sh
echo -ne "GET /static/small00.dat HTTP/1.1\r\n\r\n" | \
        nc -q 1 localhost "8888" > small00.dat 2> /dev/null
```

## Running Lambda Server

- Go to 6_lambda-function-server
- Run make command from src and the start server with ./server
```sh
## Starting the server
user@os:.../6_lambda-function-loader/src$ make
user@os:.../6_lambda-function-loader/src$ ./server
```
- Request function as such:
```sh
## Client
#
# Open another terminal and instruct the client to send a library to the server.
# Please remember the calling convention for client
# ./client <libname> [<function> [<file>]]
#
# Note: When <function> is not provided, the server will execute the `run` function.
user@os:.../6_lambda-function-loader/$ cd tests
user@os:.../6_lambda-function-loader/tests$ make
user@os:.../6_lambda-function-loader/tests$ ./client $(realpath libbasic.so)
Output file: /<root-project-folder>/6_lambda-function-loader/tests/output/out-bSJdTv

user@os:.../6_lambda-function-loader/tests$ cat /<root-project-folder>/6_lambda-function-loader/tests/output/out-bSJdTv
run

# Execute the "function" function from libbasic.so.
# The function does not exist, so an error is printed.
user@os:.../6_lambda-function-loader/tests$ ./client $(realpath libbasic.so) function
Output file: /<root-project-folder>/6_lambda-function-loader/tests/output/out-qOcoAA

user@os:.../6_lambda-function-loader/tests$ cat /home/so/output/out-qOcoAA
Error: /<root-project-folder>/6_lambda-function-loader/tests/libbasic.so function could not be executed.

# Execute the "cat" function from libbasic.so with the argument being the full path of file "Makefile"
user@os:.../6_lambda-function-loader/tests$ ./client $(realpath libbasic.so) cat $(realpath Makefile)
Output file: /<root-project-folder>/6_lambda-function-loader/tests/output/out-y732bN

user@os:.../6_lambda-function-loader/tests$ cat /<root-project-folder>/6_lambda-function-loader/tests/output/out-y732bN
CC=gcc
[...]
```
