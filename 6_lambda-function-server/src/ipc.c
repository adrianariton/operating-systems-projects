// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ipc.h"

// https://www.educative.io/answers/unix-socket-programming-in-c
int create_socket(void)
{
	/* TODO: Implement create_socket(). */
	int socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);

	if (socket_fd == -1)
		printf("error create\n");
	return socket_fd;
}

int listen_socket(int fd)
{
	return listen(fd, 3);
}

int bind_socket(int fd)
{
	/* TODO: Implement connect_socket(). */
	struct sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, SOCKET_NAME);
	int slen = sizeof(server_addr);
	ssize_t result = bind(fd, (struct sockaddr *) &server_addr, slen);

	if (result == -1)
		printf("error connect\n");
	return result;
}

int connect_socket(int fd)
{
	/* TODO: Implement connect_socket(). */
	struct sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, SOCKET_NAME);
	int slen = sizeof(server_addr);
	ssize_t result = connect(fd, (struct sockaddr *) &server_addr, slen);

	if (result == -1)
		printf("error connect\n");
	return result;
}

ssize_t send_socket(int fd, const char *buf, size_t len)
{
	/* TODO: Implement send_socket(). */
	int flags = 0;
	ssize_t err = send(fd, buf, len, flags);
	return err;
}

ssize_t accept_socket(int fd) 
{
	struct sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, SOCKET_NAME);
	socklen_t slen = sizeof(server_addr);
	ssize_t result = accept(fd, (struct sockaddr *) &server_addr, &slen);

	if (result == -1)
		printf("error connect\n");
	return result;
} 

ssize_t recv_socket(int fd, char *buf, size_t len)
{
	/* TODO: Implement recv_socket(). */
	int flags = 0;
	ssize_t err = recv(fd, buf, len, flags);
	return err;
}

void close_socket(int fd)
{
	/* TODO: Implement close_socket(). */
	close(fd);
}

// void bind_socket(int fd, const char *file_path) {
// 	struct sockaddr_un server_addr;
// 	server_addr.sun_family = AF_UNIX;
// 	strcpy(server_addr.sun_path, file_path);
// 	int slen = sizeof(server_addr);
// 	bind(server_socket, (struct sockaddr *) &server_addr, slen);
// }
