#include <arpa/inet.h>  /* for inet_ntoa */
#include <fcntl.h>      /* for open O_RDONLY */
#include <netinet/in.h> /* for IPv4 addr */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>     /* for strerror */
#include <sys/errno.h>  /* for errno */
#include <sys/mman.h>   /* for mmap */
#include <sys/socket.h> /* for socket,bind,listen,accept etc*/
#include <sys/stat.h>   /* for stat */
#include <unistd.h>     /* for write  read*/
#define CHECK_FAIL(ret)                                       \
  ({                                                          \
    bool fail = ret == -1;                                    \
    errno_t __save_errno = errno;                             \
    if (fail) {                                               \
      fprintf(stderr, "%s:error:%d,%s\n", __func__, __LINE__, \
              strerror(__save_errno));                        \
    }                                                         \
    errno = __save_errno;                                     \
    fail;                                                     \
  })

#define CT_GUARD(ret) \
  ({                  \
    if (ret == -1) {  \
      return -1;      \
    }                 \
  })
/// socket 套接字描述符
typedef int ct_socket_t;
/// 创建用于 Web 站点的 Socket  (IPv4 Only)
ct_socket_t create_web_server_socket(in_port_t port) {
  int serverfd = socket(AF_INET, SOCK_STREAM, 0);
  CT_GUARD(serverfd);
  size_t addr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in addr4 = {
      .sin_len = addr_len,
      .sin_family = AF_INET,
      .sin_port = htons(port),
  };
  addr4.sin_addr.s_addr = htonl(INADDR_ANY);
  int ret = -1;
  int option = 1;
  ret = setsockopt(serverfd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option));
  CT_GUARD(ret);
  ret = bind(serverfd, (struct sockaddr *)&addr4, sizeof(addr4));
  CT_GUARD(ret);
  int backlog = 128;
  ret = listen(serverfd, backlog);
  CT_GUARD(ret);
  return serverfd;
}

#define CRLF "\r\n"
#define CT_BUF_SIZE 1024
ssize_t write_http_header(ct_socket_t clientfd, size_t content_length) {
  char header[CT_BUF_SIZE] = {0};
  const char *header_tpl =
      "HTTP/1.1 200 OK" CRLF "Content-Type:text/html;charset=utf-8" CRLF
      "Content-Length:%d" CRLF "Connection: close" CRLF CRLF;
  size_t header_len = snprintf(header, CT_BUF_SIZE, header_tpl, content_length);
  return write(clientfd, header, header_len);
}

ssize_t get_file_size(const char *filename) {
  struct stat st;
  int ret = stat(filename, &st);
  CT_GUARD(ret);
  return st.st_size;
}

ssize_t write_http_body(ct_socket_t clientfd, const char *filename,
                        size_t size) {
  int fd = open(filename, O_RDONLY);
  CT_GUARD(fd);
  void *map_mem = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (map_mem == MAP_FAILED) {
    return -1;
  }
  int ret = write(clientfd, map_mem, size);
  printf("sendfile %s size: %d bytes\n", filename, ret);
  munmap(map_mem, size);
  close(fd);
  return ret;
}

int main(int argc, char const *argv[]) {
  in_port_t port = 4321;
  int serverfd = create_web_server_socket(4321);
  printf("Serving at: http://0.0.0.0:%d\n", port);
  // struct sockaddr_storage addr_storage; // the right choice
  struct sockaddr_in client_addr;  // simple
  socklen_t client_addr_len = 0;
  const char *index_filename = "html/index.html";
  while (true) {
    int clientfd =
        accept(serverfd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (CHECK_FAIL(clientfd)) {
      continue;
    }
    char *client_ip = inet_ntoa(client_addr.sin_addr);
    printf("client connected: %s:%d\n", client_ip, ntohs(client_addr.sin_port));
    size_t file_size = get_file_size(index_filename);
    if (CHECK_FAIL(file_size)) {
      goto out_close;
    }
    int send_cnt = write_http_header(clientfd, file_size);
    if (CHECK_FAIL(send_cnt)) {
      goto out_close;
    }
    send_cnt = write_http_body(clientfd, index_filename, file_size);
    if (CHECK_FAIL(send_cnt)) {
      goto out_close;
    }
  out_close:
    close(clientfd);
  }

  return 0;
}
