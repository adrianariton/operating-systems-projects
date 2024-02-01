// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

#define OPEN_C(file) \
	open(file, O_WRONLY|O_CREAT|O_APPEND, 0600)
#define OPEN_R(file) \
	open(file, O_RDONLY)

int shell_open_ca(char *file, int mode)
{
	if (mode & IO_REGULAR)
		return open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (mode & IO_OUT_APPEND)
		return open(file, O_RDWR|O_CREAT|O_APPEND, 0600);

	return open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

int shell_open_cae(char *file, int mode)
{
	if (mode & IO_REGULAR)
		return open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (mode & IO_ERR_APPEND)
		return open(file, O_WRONLY|O_CREAT|O_APPEND, 0600);
	return open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

#define OPEN_CA(file, mode) shell_open_ca(file, mode)
#define OPEN_CAE(file, mode) shell_open_cae(file, mode)

#define SAFE_EQUALS(str1, str2) \
	(str1 != NULL && str2 != NULL && strcmp(str1, str2) == 0)

#define shell_redirect_salloc(s, REDIRECTS_FILENO, BACKUPS_FILENO)\
	int REDIRECTS_FILENO[3] = {-1, -1, -1};\
	int BACKUPS_FILENO[3] = {-1, -1, -1};\
	shell_redirect(s, REDIRECTS_FILENO, BACKUPS_FILENO)

#define shell_redirect_malloc(s, REDIRECTS_FILENO, BACKUPS_FILENO)\
	int *REDIRECTS_FILENO = (int *)malloc(3 * sizeof(int));\
	int *BACKUPS_FILENO = (int *)malloc(3 * sizeof(int));\
	for (int i = 0; i < 3; ++i) {\
		REDIRECTS_FILENO[i] = -1;\
		BACKUPS_FILENO[i] = -1;\
	} shell_redirect(s, REDIRECTS_FILENO, BACKUPS_FILENO)
/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	int result = chdir(get_word(dir));
	return result;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	exit(0);
	return 0;
}

static void shell_redirect(simple_command_t *s, int *REDIRECTS_FILENO, int *BACKUPS_FILENO)
{
	BACKUPS_FILENO[0] = dup(STDIN_FILENO);
	BACKUPS_FILENO[1] = dup(STDOUT_FILENO);
	BACKUPS_FILENO[2] = dup(STDERR_FILENO);

	char *infile, *outfile, *errfile;
	int fdin = -1, fdout = -1, fderr = -1;
	int mode = s->io_flags;

	infile = get_word(s->in);

	if (infile != NULL) {
		fdin = OPEN_R(infile);
		REDIRECTS_FILENO[0] = dup2(fdin, STDIN_FILENO);
	}
	outfile = get_word(s->out);

	if (outfile != NULL) {
		fdout = OPEN_CA(outfile, mode);
		REDIRECTS_FILENO[1] = dup2(fdout, STDOUT_FILENO);
	}
	errfile = get_word(s->err);

	if (errfile != NULL) {
		if (SAFE_EQUALS(errfile, outfile)) {
			REDIRECTS_FILENO[2] = dup2(fdout, STDERR_FILENO);
		} else {
			fderr = OPEN_CAE(errfile, mode);
			REDIRECTS_FILENO[2] = dup2(fderr, STDERR_FILENO);
		}
	}
}

static void shell_revert(int *REDIRECTS_FILENO, int *BACKUPS_FILENO, bool flush)
{
	for (int i = 2; i >= 0; --i)
		dup2(BACKUPS_FILENO[i], i);
}

#define shell_revert_malloc(REDIRECTS_FILENO, BACKUPS_FILENO, flush)\
	shell_revert(REDIRECTS_FILENO, BACKUPS_FILENO, flush);\
	free(REDIRECTS_FILENO);\
	free(BACKUPS_FILENO)

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */

	/* TODO: If builtin command, execute the command. */
	char *command_verb = get_word(s->verb);

	if (command_verb == NULL || strlen(command_verb) == 0)
		return 0;
	int internal = 0;

	if (strcmp(command_verb, "exit") == 0 ||
			strcmp(command_verb, "quit") == 0) {
		internal = 1;
		shell_exit();
		return 0;
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	if (strchr(command_verb, '=') != NULL) {
		// is variable assignment
		char *rhs = strchr(command_verb, '=');

		(*rhs) = '\0';
		rhs++;

		char *lhs = command_verb;

		return setenv(lhs, rhs, 1);
	}

	if (strcmp(command_verb, "cd") == 0) {
		internal = 1;
		shell_redirect_malloc(s, REDIRECTS_FILENO, BACKUPS_FILENO);
		int status = shell_cd(s->params);

		shell_revert_malloc(REDIRECTS_FILENO, BACKUPS_FILENO, true);
		return status;
	}

	if (strcmp(command_verb, "pwd") == 0) {

		internal = 1;
		char cwd[1024];

		getcwd(cwd, sizeof(cwd));
		pid_t pid = fork();

		if (pid == 0) {
			shell_redirect_malloc(s, REDIRECTS_FILENO1, BACKUPS_FILENO1);
			printf("%s\n", cwd);
			shell_revert_malloc(REDIRECTS_FILENO1, BACKUPS_FILENO1, true);
		} else {
			int status;

			waitpid(pid, &status, 0);
			return status;
		}
		return 0;
	}

	pid_t pid = fork();

	if (pid < 0) {
		printf("FAILED\n");
	} else if (pid == 0) {
		int suc = 0;

		shell_redirect_malloc(s, REDIRECTS_FILENO2, BACKUPS_FILENO2);

		if (internal == 0) {
			int size = 0;
			char **argv = get_argv(s, &size);

			if (execvp(command_verb, argv) == -1) {
				// unknown command
				printf("Execution failed for \'%s\'\n", command_verb);
				suc = -1;
				free(argv);


				shell_revert_malloc(REDIRECTS_FILENO2, BACKUPS_FILENO2, true);
				exit(-1);
			}
			free(argv);
		}

		shell_revert_malloc(REDIRECTS_FILENO2, BACKUPS_FILENO2, true);
		exit(suc);
	} else {
		int status;

		waitpid(pid, &status, 0);
		return status;
	}

	return 0; /* TODO: Replace with actual exit status. */
}

int __parse_command(cmdargs *cm)
{
	return parse_command(cm->c, cm->level, cm->father);
}

cmdargs buildargs(command_t *cmd, int level,
		command_t *father)
{
	cmdargs cm1;

	cm1.c = cmd;
	cm1.level = level;
	cm1.father = father;
	return cm1;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	pid_t pid = fork();

	if (pid < 0) {
		return false;
	} else if (pid == 0) {
		int ecode1 = parse_command(cmd1, level + 1, father);

		exit(ecode1);
	} else if (pid > 0) {
		int ecode2 = parse_command(cmd2, level + 1, father);
		int status;

		waitpid(pid, &status, 0);
		return ecode2 | status;
	}

	return 0;
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	int pipe1to2[2];

	pipe(pipe1to2);
	int bck = dup(STDOUT_FILENO);
	int bcki = dup(STDIN_FILENO);
	pid_t pid = fork();

	if (pid > 0) {
		dup2(pipe1to2[0], STDIN_FILENO);
		close(pipe1to2[0]);
		close(pipe1to2[1]);

		int ecode2 = parse_command(cmd2, level + 1, father);

		dup2(bck, STDOUT_FILENO);
		dup2(bcki, STDIN_FILENO);

		int status;

		waitpid(pid, &status, 0);
		return ecode2;
	} else if (pid == 0) {
		dup2(pipe1to2[1], STDOUT_FILENO);
		close(pipe1to2[1]);
		close(pipe1to2[0]);

		parse_command(cmd1, level + 1, father);

		dup2(bck, STDOUT_FILENO);
		dup2(bcki, STDIN_FILENO);

		exit(0);
	}

	return 0; /* TODO: Replace with actual exit status. */
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	if (c->op == OP_NONE) {
		int status = parse_simple(c->scmd, level+1, c);

		return status;
	}
	int ecode1, ecode2;

	switch (c->op) {
	case OP_SEQUENTIAL:
		ecode1 = parse_command(c->cmd1, level + 1, c);
		ecode2 = parse_command(c->cmd2, level + 1, c);
		return ecode1 | ecode2;

	case OP_PARALLEL:
		return run_in_parallel(c->cmd1, c->cmd2, level + 1, father);

	case OP_CONDITIONAL_NZERO:
		ecode1 = parse_command(c->cmd1, level + 1, c);
		if (ecode1 != 0)
			return parse_command(c->cmd2, level + 1, c);
		return ecode1;

	case OP_CONDITIONAL_ZERO:
		ecode1 = parse_command(c->cmd1, level + 1, c);

		if (ecode1 == 0)
			return parse_command(c->cmd2, level + 1, c);
		return ecode1;

	case OP_PIPE:
		return run_on_pipe(c->cmd1, c->cmd2, level + 1, c);

	default:
		return SHELL_EXIT;
	}

	return 0; /* TODO: Replace with actual exit code of command. */
}
