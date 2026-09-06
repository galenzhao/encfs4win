#ifndef __PTHREAD_H
#define __PTHREAD_H

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

#include "encfs.h"
#include <dokan/dokan.h>
#include <stdio.h>
#include <io.h>
#include "dirent.h"
#include <sys/stat.h>
#include <sys/utime.h>
#include <string>


// Dokan 0.7.x used DOKAN_VERSION values like 600/740.
// Dokan 1.x/2.x use smaller version numbers (e.g. 131, 231).
#if DOKAN_VERSION > 700
#define USE_LEGACY_DOKAN
#define stat_st _stati64
#define ERRNO_FROM_WIN32(x) win32_error_to_errno(x)
#else
#define stat_st stat64_cygwin
extern "C" int errno_to_win32_error(int err);
extern "C" int win32_error_to_errno(int win_res);
#define ERRNO_FROM_WIN32(x) win32_error_to_errno(x)
#endif


typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;

void pthread_mutex_init(pthread_mutex_t *mtx, int );
void pthread_mutex_destroy(pthread_mutex_t *mtx);
void pthread_mutex_lock(pthread_mutex_t *mtx);
void pthread_mutex_unlock(pthread_mutex_t *mtx);

int pthread_create(pthread_t *thread, int, void *(*start_routine)(void*), void *arg);
void pthread_join(pthread_t thread, int);

int my_open(const char *fn, int flags);

#if defined(_WIN32)
typedef int ssize_t;
#endif

namespace unix {
int fsync(int fd);
int fdatasync(int fd);

ssize_t pread(int fd, void *buf, size_t count, __int64 offset);
ssize_t pwrite(int fd, const void *buf, size_t count, __int64 offset);

int truncate(const char *path, __int64 length);
int ftruncate(int fd, __int64 length);
int statvfs(const char *path, struct statvfs *buf);
int utimes(const char *filename, const struct timeval times[2]);
int utime(const char *filename, struct utimbuf *times);
int open(const char *fn, int flags, ...);
static inline int close(int fd) {
    return ::_close(fd);
}
int mkdir(const char *fn, int mode);
int rename(const char *oldpath, const char *newpath);
int unlink(const char *path);
int rmdir(const char *path);
int stat(const char *path, struct stat_st *buffer);
static inline int lstat(const char *path, struct stat_st *buffer) {
	return unix::stat(path, buffer);
}
int chmod (const char*, int);

#if !defined(_WIN32)
using ::dirent;
#else
struct dirent
{
        long            d_ino;          /* Always zero. */
        unsigned short  d_namlen;       /* Length of name in d_name. */
        char            d_name[FILENAME_MAX]; /* File name. */
};
#endif

struct DIR;
DIR *opendir(const char *name);
int closedir(DIR* dir);
struct dirent* readdir(DIR* dir);
};

std::wstring nix_to_winw(const std::string& src);
std::wstring utf8_to_wfn(const std::string& src);

#define mlock(a,b) do { } while(0)
#define munlock(a,b) do { } while(0)


typedef struct
{
  int waiters_count_;
  // Count of the number of waiters.

  CRITICAL_SECTION waiters_count_lock_;
  // Serialize access to <waiters_count_>.

  int release_count_;
  // Number of threads to release via a <pthread_cond_broadcast> or a
  // <pthread_cond_signal>. 
  
  int wait_generation_count_;
  // Keeps track of the current "generation" so that we don't allow
  // one thread to steal all the "releases" from the broadcast.

  HANDLE event_;
  // A manual-reset event that's used to block and release waiting
  // threads. 
} pthread_cond_t;

int pthread_cond_init (pthread_cond_t *cv, int);
int pthread_cond_destroy(pthread_cond_t *cv);
int pthread_cond_wait (pthread_cond_t *cv,
	pthread_mutex_t *external_mutex);
void pthread_cond_timedwait (pthread_cond_t *cv,
	pthread_mutex_t *external_mutex, const struct timespec *abstime);
int pthread_cond_signal (pthread_cond_t *cv);
int pthread_cond_broadcast (pthread_cond_t *cv);

#endif /* __PTHREAD_H */

