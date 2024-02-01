// SPDX-License-Identifier: BSD-3-Clause

#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#include "ipc.h"
#include "server.h"
#define SERVER_TIMEOUT 50000000
#ifndef OUTPUT_TEMPLATE
#define OUTPUT_TEMPLATE "../checker/output/out-XXXXXX"
#endif

#define OPEN_C(file) \
	open(file, O_RDWR|O_CREAT|O_APPEND, 0600)
#define OPEN_R(file) \
	open(file, O_RDONLY)

void error_out(struct lib l) {
	if (strlen(l.filename) == 0)
		printf("Error: %s %s could not be executed.\n",
			l.libname, l.funcname);
	else 
		printf("Error: %s %s %s could not be executed.\n",
			l.libname, l.funcname, l.filename);
			
}

int shell_open_ca(file, mode) {
	return open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

#define OPEN_CA(file, mode) shell_open_ca(file, mode)


int pids[1005];
int pid_len = 0;

int BACKUPS_FILENO[] = {-1,-1,-1};
int REDIRECTS_FILENO[] = {-1,-1,-1};

static char *output_name(int seed) {
	int seed_digit = seed % 10;
	char *out = strdup(OUTPUT_TEMPLATE);
	int h = 0;
	seed += (rand() % 27103) * 1000;
	for (int i = 0; i < strlen(OUTPUT_TEMPLATE); ++i) {
		if (OUTPUT_TEMPLATE[i] == 'X')
			h ++;
		if (OUTPUT_TEMPLATE[i] == 'X') {
			out[i] = 'A' + (rand() % 25);
			seed /= 10;
			seed_digit = seed % 10;
		}
	} 
	return out;
}


static struct lib lib_backup;
static int new_descriptor_backup;

static void clean_exit_on_sig(int signo) {
    printf("SISSEGV end!\n");

	fflush(stdout);
	// fflush(stderr);

	for (int i = 0; i < 3; ++i)
		if (REDIRECTS_FILENO[i] != -1)
			close(REDIRECTS_FILENO[i]);
	for (int i = 0; i < 3; ++i)
		if (REDIRECTS_FILENO[i] != -1)
			dup2(BACKUPS_FILENO[i], i);

	send_socket(new_descriptor_backup, lib_backup.outputfile, strlen(lib_backup.outputfile));
			
	close_socket(new_descriptor_backup);
	free(lib_backup.libname);
	free(lib_backup.funcname);
	free(lib_backup.filename);

	wait(NULL);
	exit(0);
}

static void signal_alarm_on_abrt(int signo) {
	printf("SIGALRM function lasted too long\n");
	fflush(stdout);
	// fflush(stderr);

	for (int i = 0; i < 3; ++i)
		if (REDIRECTS_FILENO[i] != -1)
			close(REDIRECTS_FILENO[i]);
	for (int i = 0; i < 3; ++i)
		if (REDIRECTS_FILENO[i] != -1)
			dup2(BACKUPS_FILENO[i], i);

	send_socket(new_descriptor_backup, lib_backup.outputfile, strlen(lib_backup.outputfile));
			
	close_socket(new_descriptor_backup);
	free(lib_backup.libname);
	free(lib_backup.funcname);
	free(lib_backup.filename);

	wait(NULL);
	exit(0);
}



static int lib_prehooks(struct lib *lib)
{
	/* TODO: Implement lib_prehooks(). */
	lib_backup = *lib;
	
	signal(SIGINT , clean_exit_on_sig);
    signal(SIGABRT , clean_exit_on_sig);
    signal(SIGILL , clean_exit_on_sig);
    signal(SIGFPE , clean_exit_on_sig);
    signal(SIGSEGV, clean_exit_on_sig); // <-- this one is for segmentation fault
    signal(SIGTERM , clean_exit_on_sig);

	// signal(SIGALRM, signal_alarm_on_abrt);


	return 0;
}

static int lib_load(struct lib *lib)
{
	/* TODO: Implement lib_load(). */
	char *file_out = output_name(getpid());
	lib->outputfile = file_out;
	return 0;
}

static int lib_execute(struct lib *lib)
{
	int err = 0;
	/* TODO: Implement lib_execute(). */
	lib->handle = dlopen(lib->libname, RTLD_NOW|RTLD_GLOBAL);

	BACKUPS_FILENO[0] = dup(STDIN_FILENO);
	BACKUPS_FILENO[1] = dup(STDOUT_FILENO);
	BACKUPS_FILENO[2] = dup(STDERR_FILENO);

	for (int i =0  ; i <3 ; ++i)
		REDIRECTS_FILENO[i] = -1;

	char *outfile = lib->outputfile;
	int ofd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	REDIRECTS_FILENO[1] = dup2(ofd, STDOUT_FILENO);
	// REDIRECTS_FILENO[2] = dup2(ofd, STDOUT_FILENO);

	if (lib->handle == NULL) {
		error_out(*lib);
		err = -1;
	}

	
	if(err >= 0 && strlen(lib->filename) == 0) {
		lambda_func_t run;
		if(strlen(lib->funcname) != 0)
			run = (lambda_func_t)dlsym(lib->handle, lib->funcname);
		else 
			run = (lambda_func_t)dlsym(lib->handle, "run");
		char *error;
		if ((error = dlerror()) != NULL)  {
			error_out(*lib);
			err = -1;
		}
		if (err >= 0) {
			lib->run = run;
			lib_backup = *lib;
			signal(SIGALRM, signal_alarm_on_abrt);
			ualarm(SERVER_TIMEOUT, 0);

			int _child_pid = fork();
			if(_child_pid == 0) {
				lib->run();
				exit(0);
			} else if(_child_pid > 0) {
				int _status;
				waitpid(_child_pid, &_status, 0);
				_child_pid = 0;
				if (_status != 0) {
					signal_alarm_on_abrt(0);
				}
			} else {
				exit(0);
			}
			ualarm(0,0);
		}
	} else if (err >= 0) {
		lambda_param_func_t run;
		run = (lambda_param_func_t)dlsym(lib->handle, lib->funcname);
		char *error;
		if ((error = dlerror()) != NULL)  {
			error_out(*lib);
			err = -1;
		}
		if (err >= 0) {
			lib->p_run = run;
			lib_backup = *lib;
			signal(SIGALRM, signal_alarm_on_abrt);
			ualarm(SERVER_TIMEOUT, 0);

			int _child_pid = fork();
			if(_child_pid == 0) {
				lib->p_run(lib->filename);
				exit(0);
			} else if(_child_pid > 0) {
				int _status;
				waitpid(_child_pid, &_status, 0);
				_child_pid = 0;
				if (_status != 0) {
					signal_alarm_on_abrt(0);
				}
			} else {
				exit(0);
			}
			ualarm(0,0);
			// lib->p_run = run;
			// lib_backup = *lib;
			// lib->p_run(lib->filename);
		}
	}
	fflush(stdout);
	// fflush(stderr);

	for (int i = 0; i < 3; ++i)
		if (REDIRECTS_FILENO[i] != -1)
			close(REDIRECTS_FILENO[i]);
	for (int i = 0; i < 3; ++i)
		if (REDIRECTS_FILENO[i] != -1)
			dup2(BACKUPS_FILENO[i], i);

	return err;
}

static int lib_close(struct lib *lib)
{
	/* TODO: Implement lib_close(). */
	return dlclose(lib->handle);
}


static int lib_posthooks(struct lib *lib)
{
	/* TODO: Implement lib_posthooks(). */
	return 0;
}

static int lib_run(struct lib *lib)
{
	int err;

	err = lib_prehooks(lib);
	if (err)
		return err;

	err = lib_load(lib);
	if (err)
		return err;

	err = lib_execute(lib);
	if (err)
		return err;

	err = lib_close(lib);
	if (err)
		return err;

	return lib_posthooks(lib);
}

static int parse_command(const char *buf, char *name, char *func, char *params)
{
	int ret;

	ret = sscanf(buf, "%s %s %s", name, func, params);
	if (ret < 0)
		return -1;

	return ret;
}

int main(void)
{
	int ret;
	struct lib lib;

	srand(time(NULL));   // Initialization, should only be called once.
	remove(SOCKET_NAME);

	int fd = create_socket();
	bind_socket(fd);
	listen_socket(fd);

	while (1) {
		char buf[BUFSIZE];
		char name[BUFSIZE]; 
		char func[BUFSIZE];
		char params[BUFSIZE];

		memset(buf, 0, BUFSIZE-1);
		memset(name, 0, BUFSIZE-1);
		memset(func, 0, BUFSIZE-1);
		memset(params, 0, BUFSIZE-1);
		int new_descriptor = accept_socket(fd);
		new_descriptor_backup = new_descriptor;
		
		int pid = fork();
		if(pid < 0) {
			return -1;
		} else if(pid == 0) {
			srand(getpid() * 1000);

			recv_socket(new_descriptor, buf, BUFSIZE);
			int ret = parse_command(buf, name, func, params);
			if(ret == -1) {
				close_socket(new_descriptor);
				return 0;
				break;
			}
			lib.libname = strdup(name);
			lib.funcname = strdup(func);
			lib.filename = strdup(params);

			ret = lib_run(&lib);
			send_socket(new_descriptor, lib.outputfile, strlen(lib.outputfile));
			
			close_socket(new_descriptor);
			free(lib.libname);
			free(lib.funcname);
			free(lib.filename);
			return 0;
		} else {
			close_socket(new_descriptor);
		}
	}
	wait(NULL);
	return 0;
}
