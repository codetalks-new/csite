#pragma once

/// 将 HTTP 状态码转换成对应描述短语
const char *http_status_code_to_text(int status_code) {
  switch (status_code) {
    case 200:
      return "OK";
    case 301:
      return "Moved permanently";
    case 302:
      return "Found/Moved Temporarily";
    case 303:
      return "See other";
    case 304:
      return "Not modified";
    case 400:
      return "Bad request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method not allowed";
    case 406:
      return "Not acceptable";
    case 500:
      return "Internal Server Error";
    case 501:
      return "Not implemented";
    case 502:
      return "Bad gateway";
    case 503:
      return "Service unavailable";
    case 504:
      return "Gateway timeout";
    case 505:
      return "HTTP version not supported";
    default:
      return " ";
  }
}
