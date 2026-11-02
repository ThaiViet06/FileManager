#include "file_ops.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int windows_error(DWORD error) {
    switch (error) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND: errno = ENOENT; break;
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS: errno = EEXIST; break;
        case ERROR_ACCESS_DENIED:
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION: errno = EACCES; break;
        case ERROR_NOT_SAME_DEVICE: errno = EXDEV; break;
        case ERROR_DISK_FULL: errno = ENOSPC; break;
        case ERROR_INVALID_NAME: errno = EINVAL; break;
        default: errno = EIO; break;
    }
    return -1;
}
#endif

static int require_regular(const char *path, struct stat *info) {
    if (path == NULL || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) return windows_error(GetLastError());
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (stat(path, info) != 0) return -1;
#else
    if (lstat(path, info) != 0) return -1;
#endif
    if (!S_ISREG(info->st_mode)) {
        errno = S_ISDIR(info->st_mode) ? EISDIR : EINVAL;
        return -1;
    }
    return 0;
}

static int same_file(const char *source, const struct stat *source_info,
                     const char *destination, const struct stat *destination_info) {
#ifdef _WIN32
    /* Windows CRT không cung cấp inode dùng để nhận diện tệp như POSIX.
     * https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-getfileinformationbyhandle
     */
    (void)source_info;
    (void)destination_info;
    DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE source_handle = CreateFileA(source, 0, sharing, NULL, OPEN_EXISTING, 0, NULL);
    if (source_handle == INVALID_HANDLE_VALUE) return windows_error(GetLastError());
    HANDLE destination_handle = CreateFileA(destination, 0, sharing, NULL, OPEN_EXISTING, 0, NULL);
    if (destination_handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        CloseHandle(source_handle);
        return windows_error(error);
    }
    BY_HANDLE_FILE_INFORMATION source_id, destination_id;
    if (!GetFileInformationByHandle(source_handle, &source_id) ||
        !GetFileInformationByHandle(destination_handle, &destination_id)) {
        DWORD error = GetLastError();
        CloseHandle(source_handle);
        CloseHandle(destination_handle);
        return windows_error(error);
    }
    CloseHandle(source_handle);
    CloseHandle(destination_handle);
    return source_id.dwVolumeSerialNumber == destination_id.dwVolumeSerialNumber &&
           source_id.nFileIndexHigh == destination_id.nFileIndexHigh &&
           source_id.nFileIndexLow == destination_id.nFileIndexLow;
#else
    (void)source;
    (void)destination;
    return source_info->st_dev == destination_info->st_dev &&
           source_info->st_ino == destination_info->st_ino;
#endif
}

static int prepare_operation(const char *source, const char *destination,
                             int allow_overwrite, int *destination_exists) {
    struct stat source_info, destination_info;
    if (require_regular(source, &source_info) != 0) return -1;
    if (destination == NULL || destination[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    if (require_regular(destination, &destination_info) != 0) {
        if (errno != ENOENT) return -1;
        *destination_exists = 0;
        return 0;
    }
    *destination_exists = 1;
    int same = same_file(source, &source_info, destination, &destination_info);
    if (same < 0) return -1;
    if (same) {
        errno = EINVAL;
        return -1;
    }
    if (!allow_overwrite) {
        errno = EEXIST;
        return -1;
    }
    return 0;
}

static int io_error(void) {
    return errno != 0 ? errno : EIO;
}

int file_ops_copy(const char *source, const char *destination, int allow_overwrite) {
    int destination_exists;
    if (prepare_operation(source, destination, allow_overwrite, &destination_exists) != 0) return -1;

    FILE *input = fopen(source, "rb");
    if (input == NULL) return -1;

    FILE *output;
    if (destination_exists) {
        output = fopen(destination, "wb");
    } else {
        int flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef _WIN32
        flags |= O_BINARY;
#endif
        int descriptor = open(destination, flags, 0666);
        if (descriptor < 0) {
            int error = errno;
            fclose(input);
            errno = error;
            return -1;
        }
        output = fdopen(descriptor, "wb");
        if (output == NULL) {
            int error = errno;
            close(descriptor);
            unlink(destination);
            fclose(input);
            errno = error;
            return -1;
        }
    }
    if (output == NULL) {
        int error = errno;
        fclose(input);
        errno = error;
        return -1;
    }

    int error = 0;
    char buffer[4096];
    for (;;) {
        errno = 0;
        size_t count = fread(buffer, 1, sizeof(buffer), input);
        if (ferror(input)) {
            error = io_error();
            break;
        }
        if (count == 0) break;
        errno = 0;
        if (fwrite(buffer, 1, count, output) != count) {
            error = io_error();
            break;
        }
    }
    errno = 0;
    if (fclose(output) != 0 && error == 0) error = io_error();
    errno = 0;
    if (fclose(input) != 0 && error == 0) error = io_error();
    if (error != 0) {
        if (!destination_exists) unlink(destination);
        errno = error;
        return -1;
    }
    return 0;
}

int file_ops_move(const char *source, const char *destination, int allow_overwrite) {
    int destination_exists;
    if (prepare_operation(source, destination, allow_overwrite, &destination_exists) != 0) return -1;
    (void)destination_exists;
#ifdef _WIN32
    DWORD flags = allow_overwrite ? MOVEFILE_REPLACE_EXISTING : 0;
    if (!MoveFileExA(source, destination, flags)) return windows_error(GetLastError());
    return 0;
#else
    return rename(source, destination);
#endif
}

int file_ops_delete(const char *path) {
    struct stat info;
    if (require_regular(path, &info) != 0) return -1;
    return unlink(path);
}
