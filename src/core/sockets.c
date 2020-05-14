#pragma once
#include <arpa/inet.h>  /* for inet_ntoa */
#include <netinet/in.h> /* for IPv4 addr */
#include <sys/socket.h> /* for socket,bind,listen,accept etc*/

#include "macros.h"
/// socket 套接字描述符
typedef int ct_socket_t;
/// 创建用于 Web 站点的 Socket  (IPv4 Only)
ct_socket_t create_web_server_socket(in_port_t port) {
  int serverfd = socket(AF_INET, SOCK_STREAM, 0);
  CT_GUARD(serverfd);
  // size_t addr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in addr4 = {
      // .sin_len = addr_len, // Linux 下没有此字段
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
