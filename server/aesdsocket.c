#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"

int server_socket = -1;
int client_socket = -1;
volatile sig_atomic_t g_exit_flag = 0;

void sig_handler(int sig_no) {
  if (sig_no == SIGINT || sig_no == SIGTERM) {
    syslog(LOG_INFO, "Caught signal, exiting");
    g_exit_flag = 1;

    if (client_socket != -1)
      close(client_socket);
    if (server_socket != -1)
      close(server_socket);

    if (unlink(DATA_FILE) < 0 && errno != ENOENT) {
      syslog(LOG_ERR, "Failed to delete data file %s: %s", DATA_FILE,
             strerror(errno));
    }
  }
}

int main(int argc, char *argv[]) {
  // open syslog connection
  // --------------------------------------------------------------------------
  openlog("aesdsocket", LOG_PID, LOG_USER);

  // check for daemon mode argument (-d)
  // --------------------------------------------------------------------------
  int daemon_mode = 0;
  if (argc > 1 && strcmp(argv[1], "-d") == 0) {
    daemon_mode = 1;
  }

  // register signal handlers
  // --------------------------------------------------------------------------
  struct sigaction sig_action;
  sig_action.sa_handler = sig_handler;
  sigemptyset(&sig_action.sa_mask);
  sig_action.sa_flags = 0;
  if (sigaction(SIGINT, &sig_action, NULL) < 0 ||
      sigaction(SIGTERM, &sig_action, NULL) < 0) {
    syslog(LOG_ERR, "Failed to setup signal handlers: %s", strerror(errno));
    closelog();
    return EXIT_FAILURE;
  }

  // create stream socket
  // --------------------------------------------------------------------------
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
    closelog();
    return EXIT_FAILURE;
  }

  // set SO_REUSEADDR to allow immediate reuse of the port
  // --------------------------------------------------------------------------
  int opt = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    syslog(LOG_ERR, "Failed to set socketopt: %s", strerror(errno));
    close(server_socket);
    closelog();
    return EXIT_FAILURE;
  }

  // bind socket to the port
  // --------------------------------------------------------------------------
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
    close(server_socket);
    closelog();
    return EXIT_FAILURE;
  }

  // daemon mode
  // --------------------------------------------------------------------------
  if (daemon_mode) {
    pid_t pid = fork();
    if (pid < 0) {
      syslog(LOG_ERR, "Failed to fork daemon: %s", strerror(errno));
      close(server_socket);
      closelog();
      return EXIT_FAILURE;
    }
    if (pid > 0) {
      // parent process exits successfully
      closelog();
      exit(0);
    }

    // child daemon process continues here
    if (setsid() < 0) {
      syslog(LOG_ERR, "Failed setsid: %s", strerror(errno));
      close(server_socket);
      closelog();
      return EXIT_FAILURE;
    }

    if (chdir("/") < 0) {
      syslog(LOG_ERR, "Failed chdir: %s", strerror(errno));
      close(server_socket);
      closelog();
      return EXIT_FAILURE;
    }

    // redirect stdin, stdout, and stderr to /dev/null
    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd >= 0) {
      dup2(null_fd, STDIN_FILENO);
      dup2(null_fd, STDOUT_FILENO);
      dup2(null_fd, STDERR_FILENO);
      if (null_fd > STDERR_FILENO) {
        close(null_fd);
      }
    }
  }

  // listen for the connections
  // --------------------------------------------------------------------------
  if (listen(server_socket, 10) < 0) {
    syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
    close(server_socket);
    closelog();
    return EXIT_FAILURE;
  }

  // main server loop
  while (!g_exit_flag) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    client_socket = accept(server_socket, (struct sockaddr *)&client_addr,
                           &client_addr_len);
    if (client_socket < 0) {
      if (g_exit_flag)
        break;
      if (errno == EINTR)
        continue;
      syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
      continue;
    }

    // log accepted connection with client IP
    // --------------------------------------------------------------------------
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    // open data file once per client connection
    int data_fd = open(DATA_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (data_fd < 0) {
      syslog(LOG_ERR, "Failed to open data file: %s", strerror(errno));
      close(client_socket);
      client_socket = -1;
      continue;
    }

    size_t buf_cap = 1024;
    size_t buf_len = 0;
    char *packet_buf = malloc(buf_cap);
    if (!packet_buf) {
      syslog(LOG_ERR, "Failed to allocate memory for packet buffer");
      close(data_fd);
      close(client_socket);
      client_socket = -1;
      continue;
    }

    ssize_t bytes_received;
    while (!g_exit_flag) {
      char read_buf[512];
      bytes_received = recv(client_socket, read_buf, sizeof(read_buf), 0);
      if (bytes_received < 0) {
        if (errno == EINTR)
          continue;
        break;
      } else if (bytes_received == 0) {
        break; // client closed connection
      }
      for (ssize_t i = 0; i < bytes_received; i++) {
        if (buf_len + 1 >= buf_cap) {
          buf_cap *= 2;
          char *new_buf = realloc(packet_buf, buf_cap);
          if (!new_buf) {
            syslog(LOG_ERR, "Failed to reallocate memory for packet buffer");
            free(packet_buf);
            packet_buf = NULL;
            break;
          }
          packet_buf = new_buf;
        }
        packet_buf[buf_len++] = read_buf[i];
        packet_buf[buf_len] = '\0';

        // packet complete on newline character
        if (read_buf[i] == '\n') {
          // write multiple times using the already-opened file descriptor
          ssize_t written = write(data_fd, packet_buf, buf_len);
          if (written < 0) {
            syslog(LOG_ERR, "Failed to write to data file: %s",
                   strerror(errno));
          }

          // read full content of data file and send back to client
          int read_fd = open(DATA_FILE, O_RDONLY);
          if (read_fd >= 0) {
            char send_buf[1024];
            ssize_t r_bytes;
            while ((r_bytes = read(read_fd, send_buf, sizeof(send_buf))) > 0) {
              ssize_t total_sent = 0;
              while (total_sent < r_bytes) {
                ssize_t s_bytes = send(client_socket, send_buf + total_sent,
                                       r_bytes - total_sent, 0);
                if (s_bytes < 0) {
                  if (errno == EINTR)
                    continue;
                  break;
                }
                total_sent += s_bytes;
              }
            }
            close(read_fd);
          }

          buf_len = 0;
          packet_buf[0] = '\0';
        }
      }
      if (!packet_buf)
        break;
    }

    if (packet_buf)
      free(packet_buf);
    // close the file descriptor when client disconnects
    close(data_fd);
    close(client_socket);
    client_socket = -1;
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
  }

  if (server_socket != -1)
    close(server_socket);

  closelog();
  return EXIT_SUCCESS;
}
