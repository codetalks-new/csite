#pragma once
#include <string.h>    /* for strerror strstr,strch*/
#include <sys/errno.h> /* for errno **/
#define CLEAN_ERRNO() (errno == 0 ? "None" : strerror(errno))
#define LOG_ERR(MSG, ...)                                                      \
  fprintf(stderr, "([%s]%s:%s:%d: errno: %s) " MSG "\n", log_time(), __FILE__, \
          __func__, __LINE__, CLEAN_ERRNO(), ##__VA_ARGS__)

#define LOG_INFO(MSG, ...) \
  fprintf(stdout, "[%s] " MSG "\n", log_time(), ##__VA_ARGS__)

#define CHECK_FAIL(ret)                                       \
  ({                                                          \
    bool fail = ret == -1;                                    \
    int __save_errno = errno;                                 \
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

#define CRLF "\r\n"
#define CR '\r'
#define LF '\n'
#define CT_BUF_SIZE 1024