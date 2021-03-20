#include <lauxlib.h>
#include <lua.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <assert.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/time.h>


#define ONE_MB (1024 * 1024)
#define DEFAULT_ROLL_SIZE (1024 * ONE_MB)
#define DEFAULT_BASENAME "default"
#define DEFAULT_DIRNAME "."
#define DEFAULT_INTERVAL 5

#define LOG_MAX 4 * 1024
#define LOG_BUFFER_SIZE 4 * 1024 * 1024


#if defined(__APPLE__)
#define fwrite_unlocked fwrite
#define fflush_unlocked fflush

#include <mach/mach_time.h>
#define ORWL_NANO (+1.0E-9)
#define ORWL_GIGA UINT64_C(1000000000)

// for macOS
#ifndef CLOCK_REALTIME
static int CLOCK_REALTIME = 0;
static double orwl_timebase = 0.0;
static uint64_t orwl_timestart = 0;
void clock_gettime(int useless, struct timespec *t) {
  // be more careful in a multithreaded environement
  if (!orwl_timestart) {
    mach_timebase_info_data_t tb = {0};
    mach_timebase_info(&tb);
    orwl_timebase = tb.numer;
    orwl_timebase /= tb.denom;
    orwl_timestart = mach_absolute_time();
  }
  double diff = (mach_absolute_time() - orwl_timestart) * orwl_timebase;
  t->tv_sec = diff * ORWL_NANO;
  t->tv_nsec = diff - (t->tv_sec * ORWL_GIGA);
}
#endif // #ifndef CLOCK_REALTIME

#endif // defined(__APPLE__)


enum logger_level {
  DEBUG = 0,
  INFO = 1,
  WARNING = 2,
  ERROR = 3,
  FATAL = 4,
  SKY = 5 // infomation from skynet
};


struct buffer {
  struct buffer *next;
  char data[LOG_BUFFER_SIZE];
  int size; // already used bytes
};


struct buffer_list {
  struct buffer *head;
  struct buffer *tail;
  int size; // number of buffers
};


struct logger {
  FILE *handle;
  int loglevel;
  int rollsize;
  char basename[64];
  char dirname[64];
  size_t written_bytes;
  int roll_mday;        // as tm_mday of struct tm
  int flush_interval;
  int roll_index;
  int print_log; // if print to console

  struct buffer *curr_buffer;
  struct buffer *next_buffer; // standby buffer
  struct buffer_list buffers;

  int running;
  pthread_t thread;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} inst;


char *get_log_filename(const char *basename) {
  static char filename[128];
  memset(filename, 0, sizeof(filename));
  char timebuf[64];
  struct tm tm;
  time_t now = time(NULL);
  localtime_r(&now, &tm);
  strftime(timebuf, sizeof(timebuf), "_%Y%m%d_%H%M%S", &tm);
  snprintf(filename, sizeof(filename), "%s%s_%d.log", basename, timebuf,
           inst.roll_index++);
  return filename;
}


void rollfile() {
  if (inst.handle == stdin || inst.handle == stdout || inst.handle == stderr)
    return;

  if (inst.handle != NULL && inst.written_bytes > 0) {
    fflush(inst.handle);
    fclose(inst.handle);
  }

  char filename[128];
  DIR *dir;
  dir = opendir(inst.dirname);
  if (dir == NULL) {
    int saved_errno = errno;
    if (saved_errno == ENOENT) {
      if (mkdir(inst.dirname, 0755) == -1) {
        saved_errno = errno;
        fprintf(stderr, "mkdir error: %s\n", strerror(saved_errno));
        exit(EXIT_FAILURE);
      }
    } else {
      fprintf(stderr, "opendir error: %s\n", strerror(saved_errno));
      exit(EXIT_FAILURE);
    }
  } else
    closedir(dir);

  while (1) {
    snprintf(filename, sizeof(filename), "%s/%s", inst.dirname,
             get_log_filename(inst.basename));
    inst.handle = fopen(filename, "a+");
    if (inst.handle == NULL) {
      int saved_errno = errno;
      fprintf(stderr, "open file error: %s\n", strerror(saved_errno));
      inst.handle = stdout;
      break;
    } else {
      if (inst.written_bytes >= inst.rollsize) {
        fclose(inst.handle);
        inst.written_bytes = 0;
        continue;
      }

      struct tm tm;
      time_t now = time(NULL);
      localtime_r(&now, &tm);
      inst.roll_mday = tm.tm_mday;
      break;
    }
  }
}


void append(const char *logline, size_t len) {
  pthread_mutex_lock(&inst.mutex);
  if (inst.curr_buffer->size + len < LOG_BUFFER_SIZE) {
    // append to tail
    memcpy(inst.curr_buffer->data + inst.curr_buffer->size, logline, len);
    inst.curr_buffer->size += len;
  } else {
    // add curr_buffer to buffers
    if (!inst.buffers.head) {
      assert(inst.buffers.tail == NULL);
      inst.buffers.head = inst.curr_buffer;
      inst.buffers.tail = inst.curr_buffer;
    } else {
      inst.buffers.tail->next = inst.curr_buffer;
      inst.buffers.tail = inst.curr_buffer;
    }
    inst.buffers.size++;
    assert(inst.buffers.tail->next == NULL);

    if (inst.next_buffer) {
      inst.curr_buffer->next = inst.next_buffer;
      inst.curr_buffer = inst.next_buffer;
      inst.next_buffer = NULL;
    } else {
      //HACK: almost impossible.
      // huge logs, curr_buffer and next_buffer all ran out... 
      struct buffer *new_buffer =
          (struct buffer *)calloc(1, sizeof(struct buffer));
      inst.curr_buffer->next = new_buffer;
      inst.curr_buffer = new_buffer;
    }
    memcpy(inst.curr_buffer->data + inst.curr_buffer->size, logline, len);
    inst.curr_buffer->size += len;

    pthread_cond_signal(&inst.cond);
  }

  pthread_mutex_unlock(&inst.mutex);
}


static inline void *worker_func(void *p) {
  rollfile();
  struct timespec ts;
  struct buffer_list buffers_to_write;
  memset(&buffers_to_write, 0, sizeof(buffers_to_write));
  
  struct buffer *new_buffer1 =
      (struct buffer *)calloc(1, sizeof(struct buffer));
  struct buffer *new_buffer2 =
      (struct buffer *)calloc(1, sizeof(struct buffer));
  
  while (inst.running) {
    assert(buffers_to_write.head == NULL);
    assert(new_buffer1->size == 0);
    assert(new_buffer2->size == 0);

    pthread_mutex_lock(&inst.mutex);
    if (inst.buffers.head == NULL) {
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += inst.flush_interval;
      pthread_cond_timedwait(&inst.cond, &inst.mutex, &ts);
    }

    struct tm tm;
    time_t now = time(NULL);
    localtime_r(&now, &tm);
    // roll by new day
    if (inst.roll_mday != tm.tm_mday)
      rollfile();

    // move curr_buffer into buffers
    if (!inst.buffers.head) {
      inst.buffers.head = inst.curr_buffer;
      inst.buffers.tail = inst.curr_buffer;
    } else
      inst.buffers.tail = inst.curr_buffer;
    inst.buffers.size += 1;

    inst.curr_buffer = new_buffer1;
    new_buffer1 = NULL;

    // swap buffers and buffers_to_write, for access buffers_to_write safely
    buffers_to_write.head = inst.buffers.head;
    buffers_to_write.tail = inst.buffers.tail;
    buffers_to_write.size = inst.buffers.size;
    inst.buffers.head = 0;
    inst.buffers.tail = 0;
    inst.buffers.size = 0;

    if (!inst.next_buffer) {
      // ensure alway have a standby buffer
      //    lessen allocate memory frequency, 
      //    shorter length of critical memory.
      inst.next_buffer = new_buffer2;
      new_buffer2 = NULL;
    }

    pthread_mutex_unlock(&inst.mutex);

    assert(buffers_to_write.size > 0);

    if (buffers_to_write.size > 25) {
      char timebuf[64];
      struct tm tm;
      time_t now = time(NULL);
      localtime_r(&now, &tm);
      strftime(timebuf, sizeof(timebuf), "%Y%m%d-%H%M%S", &tm);

      char buf[256];
      snprintf(buf, sizeof(buf),
               "Dropped log messages at %s, %d larger buffers\n", timebuf,
               buffers_to_write.size - 2);
      fprintf(stderr, "%s", buf);
      append(buf, strlen(buf));

      // drop logs, reserve two buffers
      struct buffer *new_tail = buffers_to_write.head->next;
      struct buffer *node = new_tail->next;
      while (node != NULL) {
        struct buffer *p = node;
        node = node->next;
        free(p);
      }
      buffers_to_write.tail = new_tail;
      buffers_to_write.tail->next = NULL;
      buffers_to_write.size = 2;
    }
    
    struct buffer *node;
    for (node = buffers_to_write.head; node != NULL; node = node->next) {
      if (inst.handle) {
        fwrite_unlocked(node->data, node->size, 1, inst.handle);
        inst.written_bytes += node->size;
      }
    }

    fflush_unlocked(inst.handle);
    if (inst.written_bytes > inst.rollsize) {
      rollfile();
    }

    if (!new_buffer1) {
      assert(buffers_to_write.size > 0);
      new_buffer1 = buffers_to_write.head;
      buffers_to_write.head = buffers_to_write.head->next;
      memset(new_buffer1, 0, sizeof(struct buffer));
      buffers_to_write.size -= 1;
    }

    if (!new_buffer2) {
      assert(buffers_to_write.size > 0);
      new_buffer2 = buffers_to_write.head;
      buffers_to_write.head = buffers_to_write.head->next;
      memset(new_buffer2, 0, sizeof(struct buffer));
      buffers_to_write.size -= 1;
    }

    // clear buffers_to_write
    node = buffers_to_write.head;
    while (node != NULL) {
      struct buffer *p = node;
      node = node->next;
      free(p);
    }
    buffers_to_write.head = 0;
    buffers_to_write.tail = 0;
    buffers_to_write.size = 0;

#if defined(__APPLE__)
    sleep(1); // unnecessary on linux 
#endif
  }

  fflush_unlocked(inst.handle);
  return NULL;
}


static void log_exit() {
  inst.running = 0;
  pthread_join(inst.thread, NULL);
  pthread_mutex_destroy(&inst.mutex);
  pthread_cond_destroy(&inst.cond);
  if (inst.handle)
    fclose(inst.handle);
}


const char *log_level_str(int level) {
  switch (level) {
  case DEBUG:
    return "[DEBUG]";
  case INFO:
    return "[INFO]";
  case WARNING:
    return "[WARNING]";
  case ERROR:
    return "[ERROR]";
  case FATAL:
    return "[FATAL]";
  case SKY:
    return "[SKY]";
  default:
    return "[UNKNOWN]";
  }
}


void append_log(const char *log, int level) {
  char msg[LOG_MAX];
  char timebuf[64];
  struct timeval tv;
  struct tm tm;
  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tm);
  strftime(timebuf, 64, "%Y-%m-%d %H:%M:%S", &tm);
  snprintf(msg, sizeof(msg), "[%s.%3.3d] %s %s\n", timebuf, tv.tv_usec / 1000,
           log_level_str(level), log);
  append(msg, strlen(msg));

  if (inst.print_log)
    printf("%s", msg);
}


int linit(lua_State *L) {
  memset(&inst, 0, sizeof(inst));
  inst.loglevel = lua_tointeger(L, 1);
  inst.rollsize = lua_tointeger(L, 2);
  inst.flush_interval = lua_tointeger(L, 3);
  inst.rollsize =
      inst.rollsize > 0 ? inst.rollsize * ONE_MB : DEFAULT_ROLL_SIZE;
  inst.flush_interval =
      inst.flush_interval > 0 ? inst.flush_interval : DEFAULT_INTERVAL;
  inst.curr_buffer = (struct buffer *)calloc(1, sizeof(struct buffer));
  inst.next_buffer = (struct buffer *)calloc(1, sizeof(struct buffer));
  inst.roll_index = 0;
  inst.print_log = lua_toboolean(L, 4);

  const char *dirname = lua_tolstring(L, 5, NULL);
  if (dirname == NULL)
    strncpy(inst.dirname, DEFAULT_DIRNAME, sizeof(inst.dirname));
  else
    strncpy(inst.dirname, dirname, sizeof(inst.dirname));

  const char *basename = lua_tolstring(L, 6, NULL);
  if (basename == NULL)
    strncpy(inst.basename, DEFAULT_BASENAME, sizeof(inst.basename));
  else
    strncpy(inst.basename, basename, sizeof(inst.basename));

  if (pthread_mutex_init(&inst.mutex, NULL) != 0) {
    int saved_errno = errno;
    fprintf(stderr, "pthread_mutex_init error: %s\n", strerror(saved_errno));
    exit(EXIT_FAILURE);
  }
  if (pthread_cond_init(&inst.cond, NULL) != 0) {
    int saved_errno = errno;
    fprintf(stderr, "pthread_cond_init error: %s\n", strerror(saved_errno));
    exit(EXIT_FAILURE);
  }

  inst.running = 1;
  if (pthread_create(&inst.thread, NULL, worker_func, NULL) != 0) {
    int saved_errno = errno;
    fprintf(stderr, "pthread_create error: %s\n", strerror(saved_errno));
    exit(EXIT_FAILURE);
  }

  return 0;
}


int lexit(lua_State *L) {
  log_exit();
  return 0;
}


int ldebug(lua_State *L) {
  if (inst.loglevel <= DEBUG) {
    size_t len = 0;
    char *data = (char *)lua_tolstring(L, 1, &len);
    if (data == NULL) {
      return 0;
    }
    if (len >= LOG_MAX)
      return 0;

    append_log(data, DEBUG);
  }
  return 0;
}


int linfo(lua_State *L) {
  if (inst.loglevel <= INFO) {
    size_t len = 0;
    char *data = (char *)lua_tolstring(L, 1, &len);
    if (data == NULL) {
      return 0;
    }
    if (len >= LOG_MAX)
      return 0;

    append_log(data, INFO);
  }
  return 0;
}


int lwarning(lua_State *L) {
  if (inst.loglevel <= WARNING) {
    size_t len = 0;
    char *data = (char *)lua_tolstring(L, 1, &len);
    if (data == NULL) {
      return 0;
    }
    if (len >= LOG_MAX)
      return 0;

    append_log(data, WARNING);
  }
  return 0;
}


int lerror(lua_State *L) {
  if (inst.loglevel <= ERROR) {
    size_t len = 0;
    char *data = (char *)lua_tolstring(L, 1, &len);
    if (data == NULL) {
      return 0;
    }
    if (len >= LOG_MAX)
      return 0;

    append_log(data, ERROR);
  }
  return 0;
}


int lfatal(lua_State *L) {
  size_t len = 0;
  char *data = (char *)lua_tolstring(L, 1, &len);
  if (data == NULL) {
    return 0;
  }
  if (len >= LOG_MAX)
    return 0;
  append_log(data, FATAL);

  log_exit();
  abort();
  return 0;
}


int lsky(lua_State *L) {
  size_t len = 0;
  char *data = (char *)lua_tolstring(L, 1, &len);
  if (data == NULL) {
    return 0;
  }
  if (len >= LOG_MAX)
    return 0;

  append_log(data, SKY);
  return 0;
}


int luaopen_log_core(lua_State *L) {
  luaL_checkversion(L);
  luaL_Reg l[] = {{"init", linit},   {"exit", lexit},       {"debug", ldebug},
                  {"info", linfo},   {"warning", lwarning}, {"error", lerror},
                  {"fatal", lfatal}, {"sky", lsky},         {NULL, NULL}};
  luaL_newlib(L, l);

  return 1;
}
