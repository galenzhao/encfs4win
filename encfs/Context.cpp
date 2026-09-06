/*****************************************************************************
 * Author:   Valient Gough <vgough@pobox.com>
 *
 *****************************************************************************
 * Copyright (c) 2007, Valient Gough
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "easylogging++.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <utility>
#include <vector>

#include "Context.h"
#include "DirNode.h"
#include "Error.h"
#include "FileNode.h"
#include "Mutex.h"
#include "unistd.h"

#if defined(WIN32) || defined(_WIN32)
#include "EncFSBusyIpc.h"
#endif

namespace encfs {

EncFS_Context::EncFS_Context() {
  pthread_cond_init(&wakeupCond, 0);
  pthread_mutex_init(&wakeupMutex, 0);
  pthread_mutex_init(&contextMutex, 0);

  usageCount = 0;
  idleCount = -1;
  isUnmounting = false;
  currentFuseFh = 1;
#if defined(WIN32) || defined(_WIN32)
  busyMapping = NULL;
  busyView = NULL;
#endif
}

EncFS_Context::~EncFS_Context() {
#if defined(WIN32) || defined(_WIN32)
  if (busyView) {
    UnmapViewOfFile(busyView);
    busyView = NULL;
  }
  if (busyMapping) {
    CloseHandle((HANDLE)busyMapping);
    busyMapping = NULL;
  }
#endif
  pthread_mutex_destroy(&contextMutex);
  pthread_mutex_destroy(&wakeupMutex);
  pthread_cond_destroy(&wakeupCond);

  // release all entries from map
  openFiles.clear();
  dirtyPlainPaths.clear();
}

std::shared_ptr<DirNode> EncFS_Context::getRoot(int *errCode) {
  return getRoot(errCode, false);
}

std::shared_ptr<DirNode> EncFS_Context::getRoot(int *errCode,
                                                bool skipUsageCount) {
  std::shared_ptr<DirNode> ret;
  do {
    {
      Lock lock(contextMutex);
      if (isUnmounting) {
        *errCode = -EBUSY;
        break;
      }
      ret = root;
      // On some systems, stat of "/" is allowed even if the calling user is
      // not allowed to list / to go deeper. Do not then count this call.
      if (!skipUsageCount) {
        ++usageCount;
      }
    }

    if (!ret) {
      int res = remountFS(this);
      if (res != 0) {
        *errCode = res;
        break;
      }
    }
  } while (!ret);

  return ret;
}

void EncFS_Context::setRoot(const std::shared_ptr<DirNode> &r) {
  Lock lock(contextMutex);

  root = r;
  if (r) rootCipherDir = r->rootDirectory();
}

bool EncFS_Context::isMounted() { return root.get() != nullptr; }

// Called periodically by the idle monitoring thread.
// Returns true if FS has really been unmounted, false otherwise.
bool EncFS_Context::usageAndUnmount(int timeoutCycles) {
  {
    Lock lock(contextMutex);

    if (root == nullptr) {
      return false;
    }

    if (usageCount == 0) {
      ++idleCount;
    } else {
      idleCount = 0;
    }
    VLOG(1) << "idle cycle count: " << idleCount << ", timeout at "
            << timeoutCycles;

    usageCount = 0;

    if (idleCount < timeoutCycles) {
      return false;
    }

    if (!openFiles.empty()) {
      if (idleCount % timeoutCycles == 0) {
        RLOG(WARNING) << "Filesystem inactive, but " << openFiles.size()
                      << " files opened: " << this->opts->mountPoint;
      }
      return false;
    }
    if (!this->opts->mountOnDemand) {
      isUnmounting = true;
    }
  }  // release contextMutex before unmount

  return unmountFS(this);
}

std::shared_ptr<FileNode> EncFS_Context::lookupNode(const char *path) {
  Lock lock(contextMutex);

  FileMap::iterator it = openFiles.find(std::string(path));
  if (it != openFiles.end()) {
    // all the items in the list point to the same node.. so just use the
    // first
    return it->second.front().node;
  }
  return std::shared_ptr<FileNode>();
}

void EncFS_Context::renameNode(const char *from, const char *to) {
  Lock lock(contextMutex);

  FileMap::iterator it = openFiles.find(std::string(from));
  if (it != openFiles.end()) {
    auto val = it->second;
    openFiles.erase(it);
    openFiles[std::string(to)] = val;
  }

  auto dirtyIt = dirtyPlainPaths.find(std::string(from));
  if (dirtyIt != dirtyPlainPaths.end()) {
    dirtyPlainPaths.erase(dirtyIt);
    dirtyPlainPaths.insert(std::string(to));
  }
}

void EncFS_Context::putNode(const char *path,
                            const std::shared_ptr<FileNode> &node, int flags) {
  Lock lock(contextMutex);
  OpenPlaceholder ph;
  ph.node = node;
  ph.flags = flags;
  auto &list = openFiles[std::string(path)];
  // The length of "list" serves as the reference count.
  list.push_front(ph);
  fuseFhMap[node->fuseFh] = node;
}

void EncFS_Context::eraseNode(const char *path,
                              const std::shared_ptr<FileNode> &fnode) {
  Lock lock(contextMutex);

  FileMap::iterator it = openFiles.find(std::string(path));
#if defined(WIN32) || defined(__CYGWIN__)
  // When renaming a file, Windows first opens it, renames it and then closes
  // it. Filenode may have then been renamed too.
  if (it == openFiles.end()) {
    RLOG(WARNING)
        << "Filenode to erase not found, file has certainly been renamed: "
        << path;
    return;
  }
#endif
  rAssert(it != openFiles.end());
  auto &list = it->second;

  auto findIter =
      std::find_if(list.begin(), list.end(),
                   [&fnode](const OpenPlaceholder &ph) { return ph.node == fnode; });
  rAssert(findIter != list.end());
  list.erase(findIter);

  // If no reference to "fnode" remains, drop the entry from fuseFhMap
  // and overwrite the canary.
  findIter =
      std::find_if(list.begin(), list.end(),
                   [&fnode](const OpenPlaceholder &ph) { return ph.node == fnode; });
  if (findIter == list.end()) {
    fuseFhMap.erase(fnode->fuseFh);
    fnode->canary = CANARY_RELEASED;
  }

  if (list.empty()) {
    openFiles.erase(it);
  }
}

size_t EncFS_Context::writableOpenCount() const {
  Lock lock(contextMutex);
  size_t count = 0;
  for (const auto &entry : openFiles) {
    for (const auto &ph : entry.second) {
      if (isWritableOpenFlags(ph.flags)) {
        ++count;
      }
    }
  }
  return count;
}

size_t EncFS_Context::unsyncedOpenCount() const {
  Lock lock(contextMutex);
  size_t count = 0;
  for (const auto &entry : openFiles) {
    if (dirtyPlainPaths.find(entry.first) != dirtyPlainPaths.end()) {
      ++count;
    }
  }
  return count;
}

std::vector<std::string> EncFS_Context::unsyncedOpenPaths() const {
  Lock lock(contextMutex);
  std::vector<std::string> paths;
  for (const auto &entry : openFiles) {
    if (dirtyPlainPaths.find(entry.first) != dirtyPlainPaths.end()) {
      paths.push_back(entry.first);
    }
  }
  return paths;
}

void EncFS_Context::ensureBusyIpc() const {
#if defined(WIN32) || defined(_WIN32)
  if (busyView) {
    return;
  }
  char name[128];
  EncFSBusyIpcMappingNameA(GetCurrentProcessId(), name, sizeof(name));
  HANDLE mapping =
      CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                         (DWORD)sizeof(EncFSBusyIpcStatus), name);
  if (!mapping) {
    RLOG(WARNING) << "CreateFileMapping for busy IPC failed: "
                  << GetLastError();
    return;
  }
  void *view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0,
                             sizeof(EncFSBusyIpcStatus));
  if (!view) {
    RLOG(WARNING) << "MapViewOfFile for busy IPC failed: " << GetLastError();
    CloseHandle(mapping);
    return;
  }
  // Only the first initializer owns the mapping handles.
  if (InterlockedCompareExchangePointer(&busyView, view, NULL) != NULL) {
    UnmapViewOfFile(view);
    CloseHandle(mapping);
    return;
  }
  memset(view, 0, sizeof(EncFSBusyIpcStatus));
  auto *status = static_cast<EncFSBusyIpcStatus *>(view);
  status->magic = ENCFS_BUSY_IPC_MAGIC;
  status->version = ENCFS_BUSY_IPC_VERSION;
  InterlockedExchange(&status->unsyncedCount, 0);
  InterlockedExchangePointer(&busyMapping, mapping);
#else
  (void)0;
#endif
}

void EncFS_Context::publishBusyStatus() const {
#if defined(WIN32) || defined(_WIN32)
  ensureBusyIpc();
  if (!busyView) {
    return;
  }
  auto paths = unsyncedOpenPaths();
  auto *status = static_cast<EncFSBusyIpcStatus *>(busyView);
  InterlockedExchange(&status->unsyncedCount, (LONG)paths.size());
  status->paths[0] = 0;
  size_t used = 0;
  for (const auto &p : paths) {
    if (used + p.size() + 2 >= ENCFS_BUSY_PATHS_BYTES) {
      break;
    }
    memcpy(status->paths + used, p.c_str(), p.size());
    used += p.size();
    status->paths[used++] = '\n';
  }
  status->paths[used] = 0;
#else
  (void)0;
#endif
}

void EncFS_Context::upgradeOpenFlags(const char *path, int flags) {
  if (!isWritableOpenFlags(flags)) {
    return;
  }
  Lock lock(contextMutex);
  FileMap::iterator it = openFiles.find(std::string(path));
  if (it == openFiles.end()) {
    return;
  }
  for (auto &ph : it->second) {
    if (!isWritableOpenFlags(ph.flags)) {
      ph.flags |= O_RDWR;
    }
  }
}

void EncFS_Context::markDirty(const char *plainPath) {
  if (!plainPath) {
    return;
  }
  {
    Lock lock(contextMutex);
    dirtyPlainPaths.insert(std::string(plainPath));
  }
  publishBusyStatus();
}

void EncFS_Context::clearDirty(const char *plainPath) {
  if (!plainPath) {
    return;
  }
  {
    Lock lock(contextMutex);
    dirtyPlainPaths.erase(std::string(plainPath));
  }
  publishBusyStatus();
}

bool EncFS_Context::isDirty(const char *plainPath) const {
  if (!plainPath) {
    return false;
  }
  Lock lock(contextMutex);
  return dirtyPlainPaths.find(std::string(plainPath)) != dirtyPlainPaths.end();
}

void EncFS_Context::syncCipherPath(const std::string &cipherPath) {
  int fd = unix::open(cipherPath.c_str(), O_RDONLY);
  if (fd < 0) {
    VLOG(1) << "syncCipherPath open failed for " << cipherPath << ": "
            << strerror(errno);
    return;
  }
  if (unix::fsync(fd) == -1) {
    VLOG(1) << "syncCipherPath fsync failed for " << cipherPath << ": "
            << strerror(errno);
  }
  unix::close(fd);
}

bool EncFS_Context::syncForUnmount() {
  std::vector<std::shared_ptr<FileNode>> nodesToSync;
  std::vector<std::string> dirtyCopy;
  std::shared_ptr<DirNode> FSRoot;

  size_t unsyncedCount = 0;
  {
    Lock lock(contextMutex);
    std::set<FileNode *> seen;
    for (const auto &entry : openFiles) {
      if (dirtyPlainPaths.find(entry.first) != dirtyPlainPaths.end()) {
        ++unsyncedCount;
      }
      for (const auto &ph : entry.second) {
        if (ph.node && seen.insert(ph.node.get()).second) {
          nodesToSync.push_back(ph.node);
        }
      }
    }
    if (unsyncedCount > 0) {
      RLOG(WARNING) << "Cannot unmount, " << unsyncedCount
                    << " file(s) still open with unsynced writes: "
                    << (opts ? opts->mountPoint : std::string());
      for (const auto &entry : openFiles) {
        if (dirtyPlainPaths.find(entry.first) != dirtyPlainPaths.end()) {
          RLOG(WARNING) << "  busy: " << entry.first;
        }
      }
      // Release lock before writing the report (it re-locks via unsyncedOpenPaths).
    }
  }
  if (unsyncedCount > 0) {
    publishBusyStatus();
    return false;
  }
  {
    Lock lock(contextMutex);
    dirtyCopy.assign(dirtyPlainPaths.begin(), dirtyPlainPaths.end());
    FSRoot = root;
  }

  for (const auto &node : nodesToSync) {
    int res = node->sync(true);
    if (res < 0) {
      VLOG(1) << "syncForUnmount open-node sync failed: " << node->cipherName();
    }
  }

  for (const auto &plain : dirtyCopy) {
    std::string cyName;
    if (FSRoot) {
      cyName = FSRoot->cipherPath(plain.c_str());
    }
    if (!cyName.empty()) {
      syncCipherPath(cyName);
    }
  }

  {
    Lock lock(contextMutex);
    dirtyPlainPaths.clear();
  }
  return true;
}

uint64_t EncFS_Context::nextFuseFh() {
  // Thread-safe because currentFuseFh is std::atomic
  return currentFuseFh++;
}

std::shared_ptr<FileNode> EncFS_Context::lookupFuseFh(uint64_t n) {
  Lock lock(contextMutex);
  auto it = fuseFhMap.find(n);
  if (it == fuseFhMap.end()) {
    return std::shared_ptr<FileNode>();
  }
  return it->second;
}

}  // namespace encfs
