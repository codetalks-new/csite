#pragma once

#include <sys/time.h> /* for gettimeofday,struct timespec */
#include <time.h>     /* for strftime */
/// 用于生成适合日志输出显示的时间
char *log_time() {
  const u_int8_t TIME_LEN = 64;
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

/// 计算两个用于性能统计的前后两个 timespec 的差值.以纳秒返回
long diff_timespec_to_nsec(struct timespec start, struct timespec end) {
  if (start.tv_sec > end.tv_sec) {
    return diff_timespec_to_nsec(end, start);
  }
  long s = end.tv_sec - start.tv_sec;
  long ns = s * 1000000000L + end.tv_nsec - start.tv_nsec;
  return ns;
}

typedef struct TimeSpecRange {
  struct timespec start;
  struct timespec end;
} TimeSpecRange;

int perf_start(TimeSpecRange *range) {
  return clock_gettime(CLOCK_THREAD_CPUTIME_ID, &range->start);
}
int64_t perf_end_count_ns(TimeSpecRange *range) {
  int ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &range->end);
  if (ret == -1) {
    return ret;
  }
  return diff_timespec_to_nsec(range->start, range->end);
}
