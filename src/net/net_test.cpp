#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#define SERVER_PORT 20202
#define DEFAULT_BACKLOG 1

void on_sigint_received(uv_signal_t *handle, int sig);
void on_new_connection(uv_stream_t *server, int status);
void on_server_accept(uv_handle_t *handle, size_t suggested_size,
                      uv_buf_t *buf);
void close_loop(uv_loop_t *loop);
void close_uv_loop(uv_loop_t *loop);
void on_server_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *rbuf);
void on_connection_close(uv_handle_t *handle);
void on_server_write(uv_write_t *req, int status);
void on_idle_tick(uv_idle_t *handle) {
    // nothing
}
uv_loop_t *main_loop = nullptr;
int main() {
    main_loop = uv_default_loop();
    // if (main_loop == NULL) return 1;
    printf("using default loop\n");

    // set up handlers
    printf("setting the handlers...\n");
    // sigint handler
    uv_signal_t sigint;  // = (uv_signal_t*)malloc(sizeof(uv_signal_t));
    uv_signal_init(main_loop, &sigint);
    uv_signal_start(&sigint, on_sigint_received, SIGINT);
    // uv_idle_t idle;
    // uv_idle_init(main_loop, &idle);
    // uv_idle_start(&idle, on_idle_tick);
    // tcp server
    uv_tcp_t server;  //= (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(main_loop, &server);
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", SERVER_PORT, &addr);
    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);
    int r =
        uv_listen((uv_stream_t *)&server, DEFAULT_BACKLOG, on_new_connection);
    if (r) {
        fprintf(stderr, "listen error %s (%d)\n", uv_strerror(r), r);
        return 1;
    }

    printf("running the loop!\n");
    int exit_code = uv_run(main_loop, UV_RUN_DEFAULT);
    // int exit_code = uv_run(main_loop, UV_RUN_NOWAIT);

    printf("the loop is over. now quitting\n");

    close_uv_loop(main_loop);
    // free(loop); // dont free the default loop
    main_loop = nullptr;
    return exit_code;
}

void sigint_handler(uv_signal_t *handle, int sig) {
    printf("\nCaught signal %d", sig);
    int result = uv_loop_close(handle->loop);
    if (result == UV_EBUSY) {
        close_uv_loop(handle->loop);
        // free(loop); // dont free the default loop
    }
}

void _on_uv_walk(uv_handle_t *handle, void *arg) { uv_close(handle, NULL); }
void close_uv_loop(uv_loop_t *loop) {
    // close all handlers first
    uv_stop(loop);
    uv_walk(loop, _on_uv_walk, NULL);
    uv_run(loop, UV_RUN_DEFAULT);  // run loop one time
    // close the loop
    uv_loop_close(loop);
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "SERVER: connection accept error %s (%d)\n",
                uv_strerror(status), status);
        return;
    }
    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(server->loop, client);
    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        printf("сидим не рыпаемся\n");
        uv_read_start((uv_stream_t *)client, on_server_accept, on_server_read);
    } else {
        uv_close(reinterpret_cast<uv_handle_t *>(client), on_connection_close);
    }
}
void on_connection_close(uv_handle_t *handle) {
    printf("неприятный человек попался\n");
    free(handle);
}
void on_server_accept(uv_handle_t *handle, size_t suggested_size,
                      uv_buf_t *buf) {
    buf->base = static_cast<char *>(malloc(suggested_size));
    buf->len = suggested_size;
}

/// @details реально умная штука. в on_server_write (вызывается из uv_write)
/// надо ручками очистить память, выделенную под буфер ранее в on_server_read,
/// но wbuf будет недоступен из on_server_write, поэтому сверхразумы создали эту
/// структуру. Таким образом, в uv_write передаётся не req, а req+wbuf, и
/// очищается так же req+wbuf;
struct write_req_t {
    uv_write_t req;
    uv_buf_t wbuf;
};
/// @brief
/// @param client client stream handle
/// @param n rbuf length
/// @param rbuf read buffer
void on_server_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *rbuf) {
    // if OK
    if (nread > 0) {
        write_req_t *req = (write_req_t *)malloc(sizeof(write_req_t));
        printf("data: %.*s", nread, rbuf->base);
        req->wbuf = uv_buf_init(rbuf->base, nread);
        uv_write((uv_write_t *)req, client, &req->wbuf, 1, on_server_write);
        return;
    }
    // if ERROR
    if (nread < 0) {
        printf("nread < 0\n");
        if (nread != UV_EOF)
            fprintf(stderr, "read error %s (%ld)\n", uv_err_name(nread), nread);
        uv_close((uv_handle_t *)client, on_connection_close);
    }
    // if nread <= 0
    free(rbuf->base);
}

/// @brief
/// @param req @warning req type should be write_req_t*, which is actually
/// uv_write_t+uv_buf_t
/// @param status
void on_server_write(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "server write error %s (%d)\n", uv_strerror(status),
                status);
    }
    free(reinterpret_cast<write_req_t *>(req)->wbuf.base);
    free((write_req_t *)req);
}

void on_sigint_received(uv_signal_t *handle, int sig) {
    printf("received signal %d\n", sig);
    close_uv_loop(handle->loop);
}