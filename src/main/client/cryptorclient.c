#include "cryptorclient.h"
#include "socket.h"
#include "error.h"
#include "protocol.h"
#include "logging.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void close_and_exit(Socket sock);
static int read_response(Socket sock);

int cryptor_send_command(Socket sock, const char *cmd, unsigned int seed, const char *path) {
    if(strcmp(cmd, LSTF) == 0 || strcmp(cmd, LSTR) == 0) {
        //send command
        if(send(sock, cmd, 4, 0) == -1) {
            perr_sock("Error: send_command");
            close_and_exit(sock);
        }
    } else if(strcmp(cmd, ENCR) == 0 || strcmp(cmd, DECR) == 0) {
        if(strlen(path) > MAX_PROT_PATH) {
            elog("Path too long");
            close_and_exit(sock);
        }
        char cmdline[MAX_CMDLINE_LEN + 1]; //+1 for the null teminator
        memset(cmdline, 0, sizeof(cmdline));
        snprintf(cmdline, MAX_CMDLINE_LEN + 1, "%s %u %s", cmd, seed, path);
        if(send(sock, cmdline, sizeof(cmdline) - 1, 0) == -1) {
            perr_sock("Error: send_command");
            close_and_exit(sock);
        }
    } else {
        elog("Error: cryptor_send_command: unknown command");
        close_and_exit(sock);
    }

    return read_response(sock);
}

/*
 * Returns further server output in the StringBuffer sb
 */
void cryptor_read_more(Socket server, StringBuffer *sb) {
    char buff[512];
    ssize_t bytes_recv;
    while((bytes_recv = recv(server, buff, sizeof(buff), 0)) > 0) {
        sbuf_append(sb, buff, bytes_recv);
        if(sbuf_endswith(sb, "\r\n\r\n")) break; //\r\n\r\n signals the end of the output as per protocol spec.
    }
    if(bytes_recv <= 0) perr_sock("Error");
}

/*
 * Prints further server output to stdout
 */
void cryptor_print_more(Socket server) {
    char buff[512 + 1];
    //the last 4 bytes received from the server (used to test \r\n\r\n for output end)
    char last[5];
    memset(last, '\0', 5);

    ssize_t bytes_recv;
    while((bytes_recv = recv(server, buff, sizeof(buff) - 1, 0)) > 0) {
        //print the received output to stdout
        buff[bytes_recv] = '\0';
        printf("%s", buff);

        //append the last bytes received to last while shifting to the left
        for(int i = 4; i > 0; i--) {
            int lst = bytes_recv - i;
            if(lst < 0) continue;
            for(int j = 1; j < 4; j++) last[j - 1] = last[j];
            last[3] = buff[lst];
        }
        if(strcmp(last, "\r\n\r\n") == 0) {
            printf("%s\n", "break"); //TODO: remove
            break; //\r\n\r\n signals the end of the output as per protocol spec.
        }
    }
    if(bytes_recv < 0) perr_sock("Error");
}

Socket init_connection(unsigned long addr, u_short port) {
    struct sockaddr_in server;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = addr;
    server.sin_port = port;

    Socket sock = socket(AF_INET, SOCK_STREAM, 0);
    if(!is_socket_valid(sock)) {
        perr_sock("Error: creating socket");
        close_and_exit(sock);
    }
    if(connect(sock, (struct sockaddr *) &server, sizeof(server))) {
        perr_sock("Error: bind");
        close_and_exit(sock);
    }
    return sock;
}

static int read_response(Socket sock) {
    //read response code
    char resp[4];
    int received;
    memset(resp, '\0', sizeof(resp));
    if((received = recv(sock, resp, sizeof(resp) - 1, MSG_WAITALL)) == -1) {
        perr_sock("Error: send_command");
        close_and_exit(sock);
    }
    if(received == 0) {
        elog("Socket closed by foreign host");
        close_and_exit(sock);
    }

    return (int) strtol(resp, NULL, 0);
}

static void close_and_exit(Socket sock) {
	socket_close(sock);
	socket_cleanup();
	exit(1);
}
