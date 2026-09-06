/*****************************************************************************
 * Shared-memory IPC between encfs.exe and encfsw.exe for umount busy state.
 * No temp files — parent reads the child's named file mapping by PID.
 *****************************************************************************/

#ifndef _EncFSBusyIpc_incl_
#define _EncFSBusyIpc_incl_

#if defined(WIN32) || defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>

namespace encfs {

enum {
  ENCFS_BUSY_IPC_MAGIC = 0x45424653,  // 'EBFS'
  ENCFS_BUSY_IPC_VERSION = 1,
  ENCFS_BUSY_PATHS_BYTES = 4096
};

#pragma pack(push, 4)
struct EncFSBusyIpcStatus {
  DWORD magic;
  DWORD version;
  volatile LONG unsyncedCount;
  char paths[ENCFS_BUSY_PATHS_BYTES];  // UTF-8, '\n'-separated, NUL-terminated
};
#pragma pack(pop)

inline void EncFSBusyIpcMappingNameA(DWORD pid, char *out, size_t outChars) {
  _snprintf_s(out, outChars, _TRUNCATE, "Local\\EncFSBusyStatus-%lu",
              (unsigned long)pid);
}

inline void EncFSBusyIpcMappingNameW(DWORD pid, wchar_t *out, size_t outChars) {
  _snwprintf_s(out, outChars, _TRUNCATE, L"Local\\EncFSBusyStatus-%lu",
               (unsigned long)pid);
}

}  // namespace encfs

#endif  // WIN32

#endif
