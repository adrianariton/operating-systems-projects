// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/eventfd.h>
#include <libaio.h>
#include <errno.h>

#include "aws.h"
#include "utils/util.h"
#include "utils/debug.h"
#include "utils/sock_util.h"
#include "utils/w_epoll.h"

/* server socket file descriptor */
static int listenfd;

/* epoll file descriptor */
static int epollfd;

static io_context_t ctx;

static int aws_on_path_cb(http_parser *p, const char *buf, size_t len)
{
	struct connection *conn = (struct connection *)p->data;

	memcpy(conn->request_path, buf, len);
	conn->request_path[len] = '\0';
	conn->have_path = 1;

	return 0;
}

int connection_send_data(struct connection *conn)
{
	/* May be used as a helper function. */
	/* TODO: Send as much data as possible from the connection send buffer.
	 * Returns the number of bytes sent or -1 if an error occurred
	 */
	if (conn->flush == 0)
		conn->send_len = strlen(conn->send_buffer);
	int bytes = send(conn->sockfd, conn->send_buffer, conn->send_len, 0);

	while (bytes < conn->send_len)
		bytes += send(conn->sockfd, conn->send_buffer + bytes, conn->send_len - bytes, 0);
	return bytes;
}

static void connection_prepare_send_reply_header(struct connection *conn)
{
	/* TODO: Prepare the connection buffer to send the reply header. */
}

static void connection_prepare_send_404(struct connection *conn)
{
	/* TODO: Prepare the connection buffer to send the 404 header. */
}

static enum resource_type connection_get_resource_type(struct connection *conn)
{
	/* TODO: Get resource type depending on request path/filename. Filename should
	 * point to the static or dynamic folder.
	 */
	return RESOURCE_TYPE_NONE;
}


struct connection *connection_create(int sockfd)
{
	/* TODO: Initialize connection structure on given socket. */
	struct connection *conn = malloc(sizeof(struct connection));

	conn->sockfd = sockfd;
	memset(conn->recv_buffer, 0, BUFSIZ);
	memset(conn->send_buffer, 0, BUFSIZ);
	memset(conn->request_path, 0, BUFSIZ);
	conn->file_size = 0;
	conn->file_pos = 0;
	return conn;
}

void connection_remove(struct connection *conn)
{
	close(conn->sockfd);
    // io_destroy(conn->ctx);
	close(conn->fd);
	conn->state = STATE_CONNECTION_CLOSED;
	free(conn);
}

void handle_new_connection(void)
{
	/* TODO: Handle a new connection request on the server socket. */
	socklen_t address_len = sizeof(struct sockaddr_in);
	struct sockaddr_in address;

	/* TODO: Accept new connection. */
	int sockfd = accept(listenfd, (struct sockaddr *) &address, &address_len);
	/* TODO: Set socket to be non-blocking. */
	fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
	/* TODO: Instantiate new connection handler. */

	struct connection *new_conn = connection_create(sockfd);

	/* TODO: Add socket to epoll. */
	w_epoll_add_ptr_in(epollfd, sockfd, new_conn);
	/* TODO: Initialize HTTP_REQUEST parser. */
	http_parser_init(&(new_conn->request_parser), HTTP_REQUEST);
	(&(new_conn->request_parser))->data = new_conn;
	// new_conn->fd = -1;
	/* Initialize io_context */
	io_setup(1, &new_conn->ctx);
}

void receive_data(struct connection *conn)
{
	/* TODO: Receive message on socket.
	 * Store message in recv_buffer in struct connection.
	 */

	char buffer[1024] = {0};
	int len = 0;
	int bytes = recv(conn->sockfd, buffer, BUFSIZ, 0);

	strcpy(conn->recv_buffer + len, buffer);
	len += bytes;
	while (bytes > 0) {
		bytes = recv(conn->sockfd, buffer, BUFSIZ, 0);
		strcpy(conn->recv_buffer + len, buffer);
		len += bytes;
	}
	conn->recv_len = len;
	conn->state = STATE_RECEIVING_DATA;
}

int connection_open_file(struct connection *conn)
{
	char *relative_path = ".";

	char *result = malloc(strlen(conn->request_path) + strlen(relative_path) + 1);

	strcpy(result, relative_path);
	strcat(result, conn->request_path);

	int foundfile_fd = open(result, O_RDWR);

	if (foundfile_fd == -1)
		printf(" < BAD_FD @ %s\n", result);

	conn->fd = foundfile_fd;
	free(result);
	return foundfile_fd;
}

int parse_header(struct connection *conn)
{
	/* TODO: Parse the HTTP header and extract the file path. */
	/* Use mostly null settings except for on_path callback. */
	http_parser_settings settings_on_path = {
		.on_message_begin = 0,
		.on_header_field = 0,
		.on_header_value = 0,
		.on_path = aws_on_path_cb,
		.on_url = 0,
		.on_fragment = 0,
		.on_query_string = 0,
		.on_body = 0,
		.on_headers_complete = 0,
		.on_message_complete = 0
	};
	(&(conn->request_parser))->data = conn;
	http_parser_execute(&(conn->request_parser), &settings_on_path, conn->recv_buffer, conn->recv_len);
	if (strstr(conn->request_path, "dynamic"))
		conn->res_type = RESOURCE_TYPE_DYNAMIC;
	else
		conn->res_type = RESOURCE_TYPE_STATIC;
	return 0;
}

int send_headers(struct connection *conn, int code)
{
	if (code == 200) {
		strcpy(conn->send_buffer, "HTTP/1.1 200 OK\r\n\r\n");
		conn->send_len = strlen("HTTP/1.1 200 OK\r\n\r\n");
		int header_bytes = connection_send_data(conn);
		return header_bytes;
	}

	strcpy(conn->send_buffer, "HTTP/1.1 404 ERROR\r\n\r\n");
	conn->send_len = strlen("HTTP/1.1 404 ERROR\r\n\r\n");

	int header_bytes = connection_send_data(conn);
	return header_bytes;
}

enum connection_state connection_send_static(struct connection *conn)
{
	/* TODO: Send static data using sendfile(2). */
	conn->flush = 0;
	if (conn->fd == -1) {
		send_headers(conn, 404);
		conn->state = STATE_404_SENT;
		return conn->state;
	}
	send_headers(conn, 200);

	struct stat st;

	fstat(conn->fd, &st);
	off_t filelen = st.st_size;
	off_t offset = conn->file_pos;
	int bytes = 0;

	conn->file_size = filelen;

	bytes = sendfile(conn->sockfd, conn->fd,  &offset, conn->file_size);
	conn->file_pos += bytes;

	conn->state = STATE_SENDING_DATA;
	w_epoll_update_ptr_inout(epollfd, conn->sockfd, conn);
	return conn->state;
}

void connection_start_async_io(struct connection *conn)
{
	/* TODO: Start asynchronous operation (read from file).
	 * Use io_submit(2) & friends for reading data asynchronously.
	 */
	conn->piocb[0] = &conn->iocb;
	memset(conn->piocb[0], 0, sizeof(struct iocb));

	io_prep_pread(conn->piocb[0], conn->fd, conn->send_buffer, BUFSIZ, conn->file_pos);

	io_submit(conn->ctx, 1, conn->piocb);
}

void connection_complete_async_io(struct connection *conn)
{
	/* TODO: Complete asynchronous operation; operation returns successfully.
	 * Prepare socket for sending.
	 */
	struct io_event io_evn;
	io_getevents(conn->ctx, 1, 1, &io_evn, NULL);

	conn->send_len = io_evn.res;
}

int connection_send_dynamic(struct connection *conn)
{
	/* TODO: Read data asynchronously.
	 * Returns 0 on success and -1 on error.
	 */
	conn->flush = 1;

	if (conn->fd == -1) {
		printf("fd-1: %s\n", conn->request_path);
		send_headers(conn, 404);
		conn->state = STATE_404_SENT;
		return conn->state;
	}

	struct stat st;

	fstat(conn->fd, &st);
	off_t filelen = st.st_size;
	int bytes = 0;

	conn->file_size = filelen;
	connection_start_async_io(conn);
	connection_complete_async_io(conn);
	bytes = connection_send_data(conn);
	conn->file_pos += bytes;

	conn->state = STATE_SENDING_DATA;
	w_epoll_update_ptr_inout(epollfd, conn->sockfd, conn);
	return conn->state;
}


void handle_input(struct connection *conn)
{
	/* TODO: Handle input information: may be a new message or notification of
	 * completion of an asynchronous I/O operation.
	 */
	receive_data(conn);

	if (w_epoll_update_fd_in(epollfd, conn->sockfd) < 0)
		return;

	switch (conn->state) {
	case STATE_RECEIVING_DATA:
		parse_header(conn);
		conn->state = STATE_REQUEST_RECEIVED;
		break;
	default:
		printf("shouldn't get here %d\n", conn->state);
	}
}

void handle_output(struct connection *conn)
{
	/* TODO: Handle output information: may be a new valid requests or notification of
	 * completion of an asynchronous I/O operation or invalid requests.
	 */
	connection_open_file(conn);
	off_t offset = conn->file_pos;
	int bytes = 0;

	switch (conn->state) {
	case STATE_REQUEST_RECEIVED:
		conn->state = STATE_SENDING_DATA;

		if (conn->res_type == RESOURCE_TYPE_STATIC)
			connection_send_static(conn);
		else
			connection_send_dynamic(conn);

		if (conn->state == STATE_404_SENT)
			conn->state = STATE_404_SENT;
		else
			conn->state = STATE_SENDING_DATA;
		w_epoll_update_ptr_out(epollfd, conn->sockfd, conn);
		close(conn->fd);
		break;
	case STATE_SENDING_DATA:
		if (conn->res_type == RESOURCE_TYPE_STATIC) {
			bytes = sendfile(conn->sockfd, conn->fd,  &offset, conn->file_size);
			conn->file_pos += bytes;
		} else {
			connection_send_dynamic(conn);
		}
		if (conn->file_pos >= conn->file_size)
			conn->state = STATE_DATA_SENT;

		w_epoll_update_ptr_out(epollfd, conn->sockfd, conn);
		close(conn->fd);

		break;
	case STATE_DATA_SENT:
		w_epoll_remove_ptr(epollfd, conn->sockfd, conn);
		connection_remove(conn);

		break;
	case STATE_404_SENT:
		printf("STATE_404\n");
		w_epoll_remove_ptr(epollfd, conn->sockfd, conn);
		connection_remove(conn);
		break;
	default:
		ERR("Unexpected state\n");
		exit(1);
	}
}

void handle_client(uint32_t event, struct connection *conn)
{
	/* TODO: Handle new client. There can be input and output connections.
	 * Take care of what happened at the end of a connection.
	 */
}

void logconn(struct connection *conn)
{
	return;
}

int main(void)
{
	/* TODO: Initialize multiplexing. */
	epollfd = w_epoll_create();

	/* TODO: Create server socket. */
	listenfd = tcp_create_listener(AWS_LISTEN_PORT, DEFAULT_LISTEN_BACKLOG);
	/* TODO: Add server socket to epoll object*/
	w_epoll_add_fd_in(epollfd, listenfd);
	/* Uncomment the following line for debugging. */
	dlog(LOG_INFO, "Server waiting for connections on port %d\n", AWS_LISTEN_PORT);

	/* server main loop */
	while (1) {
		struct epoll_event rev;

		/* TODO: Wait for events. */
		w_epoll_wait_infinite(epollfd, &rev);
		if (rev.data.fd == listenfd) {
			handle_new_connection();
		} else {
			struct connection *conn = rev.data.ptr;

			if (rev.events & EPOLLIN) {
				conn->state = STATE_RECEIVING_DATA;
				handle_input(conn);
				handle_output(conn);
			} else if (rev.events & EPOLLOUT) {
				handle_output(conn);
			}
		}

	}

	return 0;
}

