#include <arpa/inet.h>  /* for inet_ntoa */
#include <assert.h>     /* for assert */
#include <fcntl.h>      /* for open O_RDONLY */
#include <netinet/in.h> /* for IPv4 addr */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>     /* for strerror strstr,strch*/
#include <sys/errno.h>  /* for errno */
#include <sys/mman.h>   /* for mmap */
#include <sys/socket.h> /* for socket,bind,listen,accept etc*/
#include <sys/stat.h>   /* for stat */
#include <sys/time.h>   /* for gettimeofday,struct timespec */
#include <time.h>       /* for strftime */
#include <unistd.h>     /* for write  read*/

static char *log_time() {
  const ssize_t TIME_LEN = 64;
  static char buf[TIME_LEN] = {0};
  bzero(buf, TIME_LEN);
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm *tm = localtime(&tv.tv_sec);
  int millis = tv.tv_usec / 1000;
  size_t pos = strftime(buf, TIME_LEN, "%F %T", tm);
  snprintf(&buf[pos], TIME_LEN - pos, ".%03d", millis);
  return buf;
};

#define CLEAN_ERRNO() (errno == 0 ? "None" : strerror(errno))
#define LOG_ERR(MSG, ...)                                                      \
  fprintf(stderr, "([%s]%s:%s:%d: errno: %s) " MSG "\n", log_time(), __FILE__, \
          __func__, __LINE__, CLEAN_ERRNO(), ##__VA_ARGS__)

#define LOG_INFO(MSG, ...) \
  fprintf(stdout, "[%s] " MSG "\n", log_time(), ##__VA_ARGS__)

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

const char *http_status_code_to_text(int status_code) {
  switch (status_code) {
    case 200:
      return "OK";
    case 400:
      return "Not Found";
    case 500:
      return "Internal Server Error";
    default:
      return " ";
  }
}

int write_http_error(ct_socket_t clientfd, int status_code) {
  const char *msg = http_status_code_to_text(status_code);
  const char *header_tpl =
      "HTTP/1.1 %d %s" CRLF "Content-Type:text/html;charset=utf-8" CRLF
      "Content-Length:0" CRLF "Connection: close" CRLF CRLF;
  char header[CT_BUF_SIZE] = {0};
  size_t header_len =
      snprintf(header, CT_BUF_SIZE, header_tpl, status_code, msg);
  return write(clientfd, header, header_len);
}

ssize_t write_http_header(ct_socket_t clientfd, const char *content_type,
                          size_t content_length) {
  char header[CT_BUF_SIZE] = {0};
  const char *header_tpl =
      "HTTP/1.1 200 OK" CRLF "Content-Type:%s" CRLF "Content-Length:%d" CRLF
      "Connection: close" CRLF CRLF;
  size_t header_len =
      snprintf(header, CT_BUF_SIZE, header_tpl, content_type, content_length);
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
  munmap(map_mem, size);
  close(fd);
  return ret;
}

long diff_timespec_to_nsec(struct timespec start, struct timespec end) {
  if (start.tv_sec > end.tv_sec) {
    return diff_timespec_to_nsec(end, start);
  }
  long s = end.tv_sec - start.tv_sec;
  long ns = s * 1000000000L + end.tv_nsec - start.tv_nsec;
  return ns;
}

const char *guess_content_type(const char *filename) {
  // TODO 优化改为使用后缀判断
  if (strstr(filename, ".png")) {
    return "image/png";
  } else if (strstr(filename, ".jpg")) {
    return "image/jpg";
  } else if (strstr(filename, ".gif")) {
    return "image/gif";
  } else {
    return "text/html;charset=utf-8";
  }
}

ssize_t send_static_file(ct_socket_t clientfd, const char *filename) {
  struct timespec ts_start;
  struct timespec ts_end;
  int ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts_start);
  CT_GUARD(ret);
  size_t file_size = get_file_size(filename);
  CT_GUARD(file_size);
  const char *content_type = guess_content_type(filename);
  int send_cnt = write_http_header(clientfd, content_type, file_size);
  CT_GUARD(send_cnt);
  send_cnt = write_http_body(clientfd, filename, file_size);
  ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts_end);
  CT_GUARD(ret);
  long ns = diff_timespec_to_nsec(ts_start, ts_end);
  double ms = ns / 1000000.0;
  LOG_INFO("send file %s, size:%zu bytes, cost: %0.3fms", filename, file_size,
           ms);
  return send_cnt;
}

#define IS_FILE(mode) (S_ISREG(mode))
ssize_t send_static_file_or_404(ct_socket_t clientfd, const char *filename) {
  struct stat st;
  int ret = stat(filename, &st);
  if (CHECK_FAIL(ret)) {
    return write_http_error(clientfd, 404);
  }
  if (!IS_FILE(st.st_mode)) {
    return write_http_error(clientfd, 404);
  }
  return send_static_file(clientfd, filename);
}

typedef struct RequestLine {
  char method[16];
  char uri[256];
  char version[16];
} RequestLine;

int parse_request_line(ct_socket_t clientfd, RequestLine *line) {
  assert(line);
  FILE *fin = fdopen(clientfd, "r+");
  if (!fin) {
    return -1;
  }
  char buf[CT_BUF_SIZE] = {0};
  char *pos = fgets(buf, CT_BUF_SIZE, fin);
  if (pos == NULL) {
    return -1;
  }
  LOG_INFO("%s", buf);
  int cnt = sscanf(buf, "%s %s %s\n", line->method, line->uri, line->version);
  if (cnt == EOF) {
    LOG_ERR("http request line parse error");
    return -1;
  }
  return 0;
};
bool is_index_path(const char *uri) {
  return strcasecmp(uri, "/") == 0 || strcasecmp(uri, "/index.html") == 0;
}
const char *const STATIC_ROOT = "html";
void get_file_path_from_uri(char *file_path, const char *uri) {
  sprintf(file_path, "%s%s", STATIC_ROOT, uri);
  char *ptr = strchr(file_path, '?');
  if (ptr) {
    *ptr = '\0';
  }
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
    LOG_INFO("%s:%d connected!", client_ip, ntohs(client_addr.sin_port));
    RequestLine req;
    int ret = parse_request_line(clientfd, &req);
    if (CHECK_FAIL(ret)) {
      write_http_error(clientfd, 500);
    } else {
      const char *filename = index_filename;
      if (!is_index_path(req.uri)) {
        char filename_buf[256] = {0};
        get_file_path_from_uri(filename_buf, req.uri);
        filename = filename_buf;
      }
      int send_cnt = send_static_file_or_404(clientfd, filename);
      CHECK_FAIL(send_cnt);
    }
    close(clientfd);
  }

  return 0;
}
