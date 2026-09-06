#include "encfs.h"
#include "pthread.h"

#include <errno.h>
#include <stdio.h>
#include <io.h>
#include "unistd.h"
#include <fcntl.h>
#include <fuse.h>
#include <utils.h>
#include <winioctl.h>
#include <direct.h>
#include <vector>
#include <Shobjidl.h>

void pthread_mutex_init(pthread_mutex_t *mtx, int)
{
  InitializeCriticalSection(mtx);
}

void pthread_mutex_destroy(pthread_mutex_t *mtx)
{
  DeleteCriticalSection(mtx);
}

void pthread_mutex_lock(pthread_mutex_t *mtx)
{
  EnterCriticalSection(mtx);
}

void pthread_mutex_unlock(pthread_mutex_t *mtx)
{
  LeaveCriticalSection(mtx);
}

int pthread_create(pthread_t *thread, int, void *(*start_routine)(void*), void *arg)
{
  errno = 0;
  DWORD dwId;
  HANDLE res = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, &dwId);
  if (!res)
    return ENOMEM;
  *thread = res;
  return 0;
}

void pthread_join(pthread_t thread, int)
{
  WaitForSingleObject(thread, INFINITE);
}


#if !defined(USE_LEGACY_DOKAN)
struct errentry
{
  unsigned long oscode;           /* OS return value */
  int errnocode;  /* System V error code */
};

static const struct errentry errtable[] = {
  { ERROR_INVALID_FUNCTION,       EINVAL },  /* 1 */
  { ERROR_FILE_NOT_FOUND,         ENOENT },  /* 2 */
  { ERROR_PATH_NOT_FOUND,         ENOENT },  /* 3 */
  { ERROR_TOO_MANY_OPEN_FILES,    EMFILE },  /* 4 */
  { ERROR_ACCESS_DENIED,          EACCES },  /* 5 */
  { ERROR_INVALID_HANDLE,         EBADF },  /* 6 */
  { ERROR_ARENA_TRASHED,          ENOMEM },  /* 7 */
  { ERROR_NOT_ENOUGH_MEMORY,      ENOMEM },  /* 8 */
  { ERROR_INVALID_BLOCK,          ENOMEM },  /* 9 */
  { ERROR_BAD_ENVIRONMENT,        E2BIG },  /* 10 */
  { ERROR_BAD_FORMAT,             ENOEXEC },  /* 11 */
  { ERROR_INVALID_ACCESS,         EINVAL },  /* 12 */
  { ERROR_INVALID_DATA,           EINVAL },  /* 13 */
  { ERROR_INVALID_DRIVE,          ENOENT },  /* 15 */
  { ERROR_CURRENT_DIRECTORY,      EACCES },  /* 16 */
  { ERROR_NOT_SAME_DEVICE,        EXDEV },  /* 17 */
  { ERROR_NO_MORE_FILES,          ENOENT },  /* 18 */
  { ERROR_LOCK_VIOLATION,         EACCES },  /* 33 */
  { ERROR_BAD_NETPATH,            ENOENT },  /* 53 */
  { ERROR_NETWORK_ACCESS_DENIED,  EACCES },  /* 65 */
  { ERROR_BAD_NET_NAME,           ENOENT },  /* 67 */
  { ERROR_ALREADY_EXISTS,         EEXIST },  /* 183 */
  { ERROR_FILE_EXISTS,            EEXIST },  /* 80 */
  { ERROR_CANNOT_MAKE,            EACCES },  /* 82 */
  { ERROR_FAIL_I24,               EACCES },  /* 83 */
  { ERROR_INVALID_PARAMETER,      EINVAL },  /* 87 */
  { ERROR_NO_PROC_SLOTS,          EAGAIN },  /* 89 */
  { ERROR_DRIVE_LOCKED,           EACCES },  /* 108 */
  { ERROR_BROKEN_PIPE,            EPIPE },  /* 109 */
  { ERROR_DISK_FULL,              ENOSPC },  /* 112 */
  { ERROR_INVALID_TARGET_HANDLE,  EBADF },  /* 114 */
  { ERROR_INVALID_HANDLE,         EINVAL },  /* 124 */
  { ERROR_WAIT_NO_CHILDREN,       ECHILD },  /* 128 */
  { ERROR_CHILD_NOT_COMPLETE,     ECHILD },  /* 129 */
  { ERROR_DIRECT_ACCESS_HANDLE,   EBADF },  /* 130 */
  { ERROR_NEGATIVE_SEEK,          EINVAL },  /* 131 */
  { ERROR_SEEK_ON_DEVICE,         EACCES },  /* 132 */
  { ERROR_DIR_NOT_EMPTY,          ENOTEMPTY },  /* 145 */
  { ERROR_NOT_LOCKED,             EACCES },  /* 158 */
  { ERROR_BAD_PATHNAME,           ENOENT },  /* 161 */
  { ERROR_MAX_THRDS_REACHED,      EAGAIN },  /* 164 */
  { ERROR_LOCK_FAILED,            EACCES },  /* 167 */
  { ERROR_FILENAME_EXCED_RANGE,   ENOENT },  /* 206 */
  { ERROR_NESTING_NOT_ALLOWED,    EAGAIN },  /* 215 */
  { ERROR_NOT_ENOUGH_QUOTA,       ENOMEM }    /* 1816 */
};
const int errtable_size = sizeof(errtable) / sizeof(errtable[0]);

extern "C" int win32_error_to_errno(int win_res)
{
  if (win_res == 0) return 0; //No error

  if (win_res < 0) win_res = -win_res;
  for (int f = 0; f < errtable_size; ++f)
    if (errtable[f].oscode == win_res) return errtable[f].errnocode;
  return EINVAL;
}

extern "C" int errno_to_win32_error(int err)
{
  if (err == 0) return 0; //No error

  if (err < 0) err = -err;
  for (int f = 0; f < errtable_size; ++f)
    if (errtable[f].errnocode == err) return errtable[f].oscode;
  return ERROR_INVALID_FUNCTION;
}
#endif
//!USE_LEGACY_DOKAN


int unix::fsync(int fd)
{
  //VLOG(1) << "NOTIFY -- unix::fsync";
  FlushFileBuffers((HANDLE)_get_osfhandle(fd));
  return 0;
}

int unix::fdatasync(int fd)
{
  //VLOG(1) << "NOTIFY -- unix::fdatasync";
  FlushFileBuffers((HANDLE)_get_osfhandle(fd));
  return 0;
}

ssize_t unix::pread(int fd, void *buf, size_t count, __int64 offset)
{
  //VLOG(1) << "NOTIFY -- unix::pread";
  HANDLE h = (HANDLE)_get_osfhandle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    errno = EINVAL;
    return -1;
  }
  OVERLAPPED ov;
  memset(&ov, 0, sizeof(ov));
  DWORD len;
  ov.Offset = (DWORD)offset;
  ov.OffsetHigh = (DWORD)(offset >> 32);
  if (!ReadFile(h, buf, count, &len, &ov)) {
    if (GetLastError() == ERROR_HANDLE_EOF)
      return 0;
    errno = EIO;
    return -1;
  }
  return len;
}

ssize_t unix::pwrite(int fd, const void *buf, size_t count, __int64 offset)
{
  //VLOG(1) << "NOTIFY -- unix::pwrite";
  HANDLE h = (HANDLE)_get_osfhandle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    errno = EINVAL;
    return -1;
  }
  OVERLAPPED ov;
  memset(&ov, 0, sizeof(ov));
  DWORD len;
  ov.Offset = (DWORD)offset;
  ov.OffsetHigh = (DWORD)(offset >> 32);
  if (!WriteFile(h, buf, count, &len, &ov)) {
    errno = EIO;
    return -1;
  }
  return len;
}

static int truncate_handle(HANDLE fd, __int64 length)
{
  //VLOG(1) << "NOTIFY -- truncate_handle";
  LONG high = length >> 32;
  if (!SetFilePointer(fd, (LONG)length, &high, FILE_BEGIN)
    || !SetEndOfFile(fd)) {
    int save_errno = ERRNO_FROM_WIN32(GetLastError());
    errno = save_errno;
    return -1;
  }
  return 0;
}

int unix::ftruncate(int fd, __int64 length)
{
  //VLOG(1) << "NOTIFY -- unix::ftruncate";
  HANDLE h = (HANDLE)_get_osfhandle(fd);
  return truncate_handle(h, length);
}

int unix::truncate(const char *path, __int64 length)
{
  //VLOG(1) << "NOTIFY -- unix::truncate";
  std::wstring fn(utf8_to_wfn(path));
  HANDLE fd = CreateFileW(fn.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

  if (fd == INVALID_HANDLE_VALUE)
    fd = CreateFileW(fn.c_str(), GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

  if (fd == INVALID_HANDLE_VALUE) {
    errno = ERRNO_FROM_WIN32(GetLastError());
    return -1;
  }

  int res = truncate_handle(fd, length);
  CloseHandle(fd);
  return res;
}

int
pthread_cond_init(pthread_cond_t *cv, int)
{
  cv->waiters_count_ = 0;
  cv->wait_generation_count_ = 0;
  cv->release_count_ = 0;

  InitializeCriticalSection(&cv->waiters_count_lock_);
  // Create a manual-reset event.
  cv->event_ = CreateEvent(NULL,  // no security
    TRUE,  // manual-reset
    FALSE, // non-signaled initially
    NULL); // unnamed
  return 0;
}

int
pthread_cond_destroy(pthread_cond_t *cv)
{
  if (cv) {
    CloseHandle(cv->event_);
    cv->event_ = NULL;
    DeleteCriticalSection(&cv->waiters_count_lock_);
  }
  return 0;
}

int
pthread_cond_wait(pthread_cond_t *cv,
  pthread_mutex_t *external_mutex)
{
  // Avoid race conditions.
  EnterCriticalSection(&cv->waiters_count_lock_);

  // Increment count of waiters.
  cv->waiters_count_++;

  // Store current generation in our activation record.
  int my_generation = cv->wait_generation_count_;

  LeaveCriticalSection(&cv->waiters_count_lock_);
  LeaveCriticalSection(external_mutex);

  for (;;) {
    // Wait until the event is signaled.
    WaitForSingleObject(cv->event_, INFINITE);

    EnterCriticalSection(&cv->waiters_count_lock_);
    // Exit the loop when the <cv->event_> is signaled and
    // there are still waiting threads from this <wait_generation>
    // that haven't been released from this wait yet.
    int wait_done = cv->release_count_ > 0
      && cv->wait_generation_count_ != my_generation;
    LeaveCriticalSection(&cv->waiters_count_lock_);

    if (wait_done)
      break;
  }

  EnterCriticalSection(external_mutex);
  EnterCriticalSection(&cv->waiters_count_lock_);
  cv->waiters_count_--;
  cv->release_count_--;
  int last_waiter = cv->release_count_ == 0;
  LeaveCriticalSection(&cv->waiters_count_lock_);

  if (last_waiter)
    // We're the last waiter to be notified, so reset the manual event.
    ResetEvent(cv->event_);
  return 0;
}

void
pthread_cond_timedwait(pthread_cond_t *cv,
  pthread_mutex_t *external_mutex, const struct timespec *abstime)
{
  // Avoid race conditions.
  EnterCriticalSection(&cv->waiters_count_lock_);

  // Increment count of waiters.
  cv->waiters_count_++;

  // Store current generation in our activation record.
  int my_generation = cv->wait_generation_count_;

  LeaveCriticalSection(&cv->waiters_count_lock_);
  LeaveCriticalSection(external_mutex);

  DWORD start = GetTickCount();
  DWORD timeout = 0;
  if (abstime)
    timeout = abstime->tv_sec * 1000 + abstime->tv_nsec / 1000000;

  for (;;) {
    // Wait until the event is signaled.
    WaitForSingleObject(cv->event_, INFINITE);

    EnterCriticalSection(&cv->waiters_count_lock_);
    // Exit the loop when the <cv->event_> is signaled and
    // there are still waiting threads from this <wait_generation>
    // that haven't been released from this wait yet.
    int wait_done = cv->release_count_ > 0
      && cv->wait_generation_count_ != my_generation;
    LeaveCriticalSection(&cv->waiters_count_lock_);

    if (wait_done)
      break;
    if (abstime && ((DWORD)(GetTickCount() - start)) > timeout)
      break;
  }

  EnterCriticalSection(external_mutex);
  EnterCriticalSection(&cv->waiters_count_lock_);
  cv->waiters_count_--;
  cv->release_count_--;
  int last_waiter = cv->release_count_ == 0;
  LeaveCriticalSection(&cv->waiters_count_lock_);

  if (last_waiter)
    // We're the last waiter to be notified, so reset the manual event.
    ResetEvent(cv->event_);
}

int
pthread_cond_signal(pthread_cond_t *cv)
{
  EnterCriticalSection(&cv->waiters_count_lock_);
  if (cv->waiters_count_ > cv->release_count_) {
    SetEvent(cv->event_); // Signal the manual-reset event.
    cv->release_count_++;
    cv->wait_generation_count_++;
  }
  LeaveCriticalSection(&cv->waiters_count_lock_);
  return 0;
}

int
pthread_cond_broadcast(pthread_cond_t *cv)
{
  EnterCriticalSection(&cv->waiters_count_lock_);
  if (cv->waiters_count_ > 0) {
    SetEvent(cv->event_);
    // Release all the threads in this generation.
    cv->release_count_ = cv->waiters_count_;

    // Start a new generation.
    cv->wait_generation_count_++;
  }
  LeaveCriticalSection(&cv->waiters_count_lock_);
  return 0;
}

#include <sys/utime.h>

static FILETIME
timevalToFiletime(struct timeval t)
{
  // Note that LONGLONG is a 64-bit value
  LONGLONG ll;

  ll = Int32x32To64(t.tv_sec, 10000000) + 116444736000000000LL + 10u * t.tv_usec;
  FILETIME res;
  res.dwLowDateTime = (DWORD)ll;
  res.dwHighDateTime = (DWORD)(ll >> 32);
  return res;
}

int
unix::utimes(const char *filename, const struct timeval times[2])
{
  //VLOG(1) << "NOTIFY -- unix::utimes";
  std::wstring fn(utf8_to_wfn(filename));
  HANDLE h = CreateFileW(fn.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  if (h == INVALID_HANDLE_VALUE)
    h = CreateFileW(fn.c_str(), FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    errno = ERRNO_FROM_WIN32(GetLastError());
    return -1;
  }
  FILETIME fta = timevalToFiletime(times[0]);
  FILETIME ftm = timevalToFiletime(times[1]);
  BOOL res = SetFileTime(h, NULL, &fta, &ftm);
  DWORD win_err = GetLastError();
  CloseHandle(h);
  if (!res) {
    errno = ERRNO_FROM_WIN32(win_err);
    return -1;
  }
  return 0;
}

int
unix::statvfs(const char *path, struct statvfs *fs)
{
  //VLOG(1) << "NOTIFY -- unix::statvfs";
  fs->f_bsize = 4096;
  fs->f_frsize = 4096;
  fs->f_fsid = 0;
  fs->f_flag = 0;
  fs->f_namemax = 255;
  fs->f_files = -1;
  fs->f_ffree = -1;
  fs->f_favail = -1;

  ULARGE_INTEGER avail, free_bytes, bytes;
  if (!GetDiskFreeSpaceExA(path, &avail, &bytes, &free_bytes)) {
    errno = ERRNO_FROM_WIN32(GetLastError());
    return -1;
  }

  fs->f_bavail = avail.QuadPart / fs->f_bsize;
  fs->f_bfree = free_bytes.QuadPart / fs->f_bsize;
  fs->f_blocks = bytes.QuadPart / fs->f_bsize;

  errno = 0;
  return 0;
}

static int
set_sparse(HANDLE fd)
{
  DWORD returned;
  return (int)DeviceIoControl(fd, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &returned, NULL);
}

int
my_open(const char *fn_utf8, int flags)
{
  //VLOG(1) << "NOTIFY -- my_open";
  std::wstring fn = utf8_to_wfn(fn_utf8);
  HANDLE f = CreateFileW(fn.c_str(), flags == O_RDONLY ? GENERIC_READ : GENERIC_WRITE | GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  if (f == INVALID_HANDLE_VALUE) {
    int save_errno = ERRNO_FROM_WIN32(GetLastError());
    f = CreateFileW(fn.c_str(), flags == O_RDONLY ? GENERIC_READ : GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) {
      errno = save_errno;
      return -1;
    }
  }
  set_sparse(f);

  int fd = _open_osfhandle((intptr_t)f, flags);
  if (fd < 0) {
    errno = ENOENT;
    CloseHandle(f);
    return -1;
  }
  //VLOG(1) << "END NOTIFY -- my_open";
  return fd;
}

int
unix::open(const char *fn, int flags, ...)
{
  //VLOG(1) << "NOTIFY -- unix::open";
  int mode = 0;
  va_list ap;
  va_start(ap, flags);
  if (flags & O_CREAT)
    mode = va_arg(ap, int);
  va_end(ap);
  return _wopen(utf8_to_wfn(fn).c_str(), flags, mode);
}

int
unix::utime(const char *filename, struct utimbuf *times)
{
  //VLOG(1) << "NOTIFY -- unix::utime";
  if (!times)
    return unix::utimes(filename, NULL);

  struct timeval tm[2];
  tm[0].tv_sec = times->actime;
  tm[0].tv_usec = 0;
  tm[1].tv_sec = times->modtime;
  tm[1].tv_usec = 0;
  return unix::utimes(filename, tm);
}

int
unix::mkdir(const char *fn, int mode)
{
  //VLOG(1) << "NOTIFY -- unix::mkdir";
  if (CreateDirectoryW(utf8_to_wfn(fn).c_str(), NULL))
    return 0;
  errno = ERRNO_FROM_WIN32(GetLastError());
  return -1;
}

int
unix::rename(const char *oldpath, const char *newpath)
{
  VLOG(1) << "NOTIFY -- unix::rename";

  // We need to be able to move system files (e.g., DESKTOP.INI) 
  bool isSysFile = false;

  // back up old attributes 
  DWORD backupAttrs = GetFileAttributesW(utf8_to_wfn(oldpath).c_str());
  if (backupAttrs == INVALID_FILE_ATTRIBUTES) {
    VLOG(1) << "Error renaming " << oldpath << ": Change attributes failure";
    errno = ERRNO_FROM_WIN32(GetLastError());
    return -1;
  }

  if (backupAttrs & FILE_ATTRIBUTE_SYSTEM) {
    isSysFile = true;

    // Remove readonly and system attributes (for move) -- fix DESKTOP.INI issues 
    SetFileAttributesW(utf8_to_wfn(oldpath).c_str(), backupAttrs &
      (~FILE_ATTRIBUTE_READONLY & ~FILE_ATTRIBUTE_SYSTEM & ~FILE_ATTRIBUTE_HIDDEN));
  }

  if (MoveFileExW(utf8_to_wfn(oldpath).c_str(), utf8_to_wfn(newpath).c_str(), 
        (MOVEFILE_COPY_ALLOWED & MOVEFILE_WRITE_THROUGH))) {
    
    // Put back original attributes (if necessary) 
    if (isSysFile) {
      SetFileAttributesW(utf8_to_wfn(newpath).c_str(), backupAttrs);
    }

    return 0;
  }

  errno = ERRNO_FROM_WIN32(GetLastError());

  // Put back original attributes (failed move) 
  if (isSysFile) {
    SetFileAttributesW(utf8_to_wfn(oldpath).c_str(), backupAttrs);
  }

  return -1;
}

int
unix::unlink(const char *path)
{
  VLOG(1) << "NOTIFY -- unix::unlink";

  // Ensure it's a vanilla file (not hidden or system) 
  SetFileAttributesW(utf8_to_wfn(path).c_str(), FILE_ATTRIBUTE_NORMAL);

  if (DeleteFileW(utf8_to_wfn(path).c_str()))
    return 0;
  errno = ERRNO_FROM_WIN32(GetLastError());
  return -1;
}

int
unix::rmdir(const char *path)
{
  VLOG(1) << "NOTIFY -- unix::rmdir";

  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (!SUCCEEDED(hr))
  {
    VLOG(1) << "rmdir err: failed to CoInitializeEx";
    return -1;
  }

  IFileOperation *pfo;
  hr = CoCreateInstance(CLSID_FileOperation, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pfo));
  if (!SUCCEEDED(hr))
  {
    VLOG(1) << "rmdir err: failed to CoCreateInstance";
    return -1;
  }

  hr = pfo->SetOperationFlags(FOF_NO_UI);
  if (!SUCCEEDED(hr)) 
  {
    VLOG(1) << "rmdir err: failed to SetOperationFlags";
    return -1;
  }

  IShellItem* item = NULL;
  hr = SHCreateItemFromParsingName(nix_to_winw(path).c_str(), NULL, IID_PPV_ARGS(&item));
  if (!SUCCEEDED(hr)) 
  {
    VLOG(1) << "rmdir err: failed to SHCreateItemFromParsingName";
    return -1;
  }

  hr = pfo->DeleteItems(item);
  if (!SUCCEEDED(hr))
  {
    VLOG(1) << "rmdir err: failed to DeleteItem";
    return -1;
  }

  hr = pfo->PerformOperations();
  if (!SUCCEEDED(hr))
  {
    VLOG(1) << "rmdir err: failed to PerformOperations";
    return -1;
  }

  pfo->Release();
  CoUninitialize();

  return 0;
}

int
unix::stat(const char *path, struct stat_st *buffer)
{
  //VLOG(1) << "NOTIFY -- unix::stat";
  memset(buffer, 0, sizeof(*buffer));

  std::wstring fn = utf8_to_wfn(path);
  if (fn.length() && fn[fn.length() - 1] == L'\\')
    fn.resize(fn.length() - 1);
  if (strpbrk(path, "?*") != NULL) {
    errno = ENOENT;
    return -1;
  }

  DWORD attrs = 0;
  uint64_t size = 0;
  uint64_t ino = 0;
  FILETIME ftLastAccessTime = {};
  FILETIME ftLastWriteTime = {};
  FILETIME ftCreationTime = {};

  // Primary path: metadata without opening the file (reliable size + timestamps).
  WIN32_FILE_ATTRIBUTE_DATA fad;
  ZeroMemory(&fad, sizeof(fad));
  if (GetFileAttributesExW(fn.c_str(), GetFileExInfoStandard, &fad)) {
    attrs = fad.dwFileAttributes;
    size = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) |
           static_cast<uint64_t>(fad.nFileSizeLow);
    ftLastAccessTime = fad.ftLastAccessTime;
    ftLastWriteTime = fad.ftLastWriteTime;
    ftCreationTime = fad.ftCreationTime;
  } else {
    // Fallback: FindFirstFile (handles some sharing / special name cases).
    WIN32_FIND_DATAW wfd;
    ZeroMemory(&wfd, sizeof(wfd));
    HANDLE hFind = FindFirstFileW(fn.c_str(), &wfd);
    if (hFind == INVALID_HANDLE_VALUE) {
      errno = ERRNO_FROM_WIN32(GetLastError());
      return -1;
    }
    FindClose(hFind);

    attrs = wfd.dwFileAttributes;
    size = (static_cast<uint64_t>(wfd.nFileSizeHigh) << 32) |
           static_cast<uint64_t>(wfd.nFileSizeLow);
    ftLastAccessTime = wfd.ftLastAccessTime;
    ftLastWriteTime = wfd.ftLastWriteTime;
    ftCreationTime = wfd.ftCreationTime;
  }

  // Optional: inode via handle (not required for size display).
  HANDLE hFile = CreateFileW(fn.c_str(), FILE_READ_ATTRIBUTES,
    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (hFile != INVALID_HANDLE_VALUE) {
    BY_HANDLE_FILE_INFORMATION hfi;
    ZeroMemory(&hfi, sizeof(hfi));
    if (GetFileInformationByHandle(hFile, &hfi)) {
      ino = (static_cast<uint64_t>(hfi.nFileIndexHigh) << 32) |
            static_cast<uint64_t>(hfi.nFileIndexLow);
      // Prefer handle size when available (should match attributes data).
      if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LARGE_INTEGER li;
        if (GetFileSizeEx(hFile, &li) && li.QuadPart >= 0) {
          size = static_cast<uint64_t>(li.QuadPart);
        }
      }
    }
    CloseHandle(hFile);
  }

  int drive;
  if (path[1] == ':')
    drive = tolower(path[0]) - 'a';
  else
    drive = _getdrive() - 1;

  unsigned mode;
  if (attrs & FILE_ATTRIBUTE_DIRECTORY)
    mode = _S_IFDIR | 0777;
  else
    mode = _S_IFREG | 0666;
  if (attrs & FILE_ATTRIBUTE_READONLY)
    mode &= ~0222;

  buffer->st_dev = buffer->st_rdev = drive;
  buffer->st_ino = ino;
  buffer->st_mode = mode;
  buffer->st_nlink = 1;
  buffer->st_uid = 0;
  buffer->st_gid = 0;
  buffer->st_size = static_cast<FUSE_OFF_T>(size);

#ifdef USE_LEGACY_DOKAN
  buffer->st_atime = filetimeToUnixTime(&ftLastAccessTime);
  buffer->st_mtime = filetimeToUnixTime(&ftLastWriteTime);
  buffer->st_ctime = filetimeToUnixTime(&ftCreationTime);
#else
  buffer->st_atim.tv_sec = filetimeToUnixTime(&ftLastAccessTime);
  buffer->st_mtim.tv_sec = filetimeToUnixTime(&ftLastWriteTime);
  buffer->st_ctim.tv_sec = filetimeToUnixTime(&ftCreationTime);
#endif

  return 0;
}

int
unix::chmod(const char* path, int mode)
{
  //VLOG(1) << "NOTIFY -- unix::chmod";
  return _wchmod(utf8_to_wfn(path).c_str(), mode);
}

struct unix::DIR
{
  HANDLE hff;
  struct dirent ent;
  WIN32_FIND_DATAW wfd;
  int pos;
};

unix::DIR*
unix::opendir(const char *name)
{
  //VLOG(1) << "NOTIFY -- unix::opendir";
  unix::DIR *dir = (unix::DIR*) malloc(sizeof(unix::DIR));
  if (!dir) {
    errno = ENOMEM;
    return NULL;
  }
  memset(dir, 0, sizeof(*dir));
  std::wstring path = utf8_to_wfn(name);
  if (path.length() > 0 && path[path.length() - 1] == L'\\')
    path += L"*";
  else
    path += L"\\*";
  dir->hff = FindFirstFileW(path.c_str(), &dir->wfd);
  if (dir->hff == INVALID_HANDLE_VALUE) {
    errno = ERRNO_FROM_WIN32(GetLastError());
    free(dir);
    return NULL;
  }
  return dir;
}

int
unix::closedir(unix::DIR* dir)
{
  //VLOG(1) << "NOTIFY -- unix::closedir";
  errno = 0;
  if (dir && dir->hff != INVALID_HANDLE_VALUE)
    FindClose(dir->hff);
  free(dir);
  return 0;
}

struct unix::dirent*
  unix::readdir(unix::DIR* dir)
{
  //VLOG(1) << "NOTIFY -- unix::readdir";
  errno = EBADF;
  if (!dir) return NULL;
  errno = 0;
  if (dir->pos < 0) return NULL;
skip:
  if (dir->pos == 0) {
    ++dir->pos;
  }
  else if (!FindNextFileW(dir->hff, &dir->wfd)) {
    errno = GetLastError() == ERROR_NO_MORE_FILES ? 0 : ERRNO_FROM_WIN32(GetLastError());
    return NULL;
  }
  if (dir->wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    // let's ignore reparse points / links until we found a solution above to correctly handle them
    goto skip;
  }
  
  std::string path = wchar_to_utf8_cstr(dir->wfd.cFileName);
  strncpy(dir->ent.d_name, path.c_str(), sizeof(dir->ent.d_name));
  dir->ent.d_name[sizeof(dir->ent.d_name) - 1] = 0;
  dir->ent.d_namlen = strlen(dir->ent.d_name);

  // Figure out the inode number for this file
  /* Unfortunately this call is useless as path.c_str() is not a full path,
   * so stat() will fail. In addition, inode number is used by fuse_fill_dir_t filler()
   * in encfs_readdir() for caching purpose, and unfortunately Dokany does not
   * provide caching (yet ?) : https://github.com/dokan-dev/dokany/issues/670
   * WinFSP, a better alternative ? https://github.com/billziss-gh/winfsp/issues/44
  */
  /*
  struct stat_st stbuf;
  memset(&stbuf, 0, sizeof(struct stat_st));
  unix::stat(path.c_str(), &stbuf);
  dir->ent.d_ino = stbuf.st_ino;
  */

  return &dir->ent;
}

// Similar to utf8_to_wfn, but do not add fn prefixes 
std::wstring
nix_to_winw(const std::string& src)
{
  //VLOG(1) << "NOTIFY -- nix_to_winw";
  int len = src.length() + 1;
  std::vector<wchar_t> buf(len);
  utf8_to_wchar_buf(src.c_str(), buf.data(), len);
  for (wchar_t *p = buf.data(); *p; ++p)
    if (*p == L'/')
      *p = L'\\';
  return buf.data();
}

std::wstring
utf8_to_wfn(const std::string& src)
{
  //VLOG(1) << "NOTIFY -- utf8_to_wfn";
  int len = src.length() + 1;
  const size_t addSpace = 6;
  std::vector<wchar_t> buf(len + addSpace);
  utf8_to_wchar_buf(src.c_str(), buf.data() + addSpace, len);
  for (wchar_t *p = buf.data() + addSpace; *p; ++p)
    if (*p == L'/')
      *p = L'\\';
  char drive = tolower(buf[addSpace]);
  if (drive >= 'a' && drive <= 'z' && buf[addSpace + 1] == ':') {
    memcpy(buf.data() + (addSpace - 4), L"\\\\?\\", 4 * sizeof(wchar_t));
    return buf.data() + (addSpace - 4);
  }
  else if (buf[addSpace] == L'\\' && buf[addSpace + 1] == L'\\') {
    memcpy(buf.data() + (addSpace - 6), L"\\\\?\\UNC", 7 * sizeof(wchar_t));
    return buf.data() + (addSpace - 6);
  }
  return buf.data() + addSpace;
}

