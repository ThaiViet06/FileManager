#include "attributes.h"

#include <errno.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* Tăng bộ đệm đến khi đọc đủ đích, không dựa vào st_size hoặc cắt ngắn kết quả. */
static int read_link_target(const char *path, char **target) {
    size_t capacity = 128;
    for (;;) {
        char *buffer = malloc(capacity + 1);
        if (buffer == NULL) return -1;

        ssize_t length = readlink(path, buffer, capacity);
        if (length < 0) {
            int error = errno;
            free(buffer);
            errno = error;
            return -1;
        }
        if ((size_t)length < capacity) {
            buffer[length] = '\0';
            *target = buffer;
            return 0;
        }
        free(buffer);
        if (capacity > (size_t)SSIZE_MAX / 2) {
            errno = EOVERFLOW;
            return -1;
        }
        capacity *= 2;
    }
}

int attributes_read(const char *path, PathAttributes *out) {
    if (path == NULL || path[0] == '\0' || out == NULL) {
        errno = EINVAL;
        return -1;
    }
    *out = (PathAttributes){0};
    if (lstat(path, &out->info) != 0) return -1;
    if (S_ISLNK(out->info.st_mode)) return read_link_target(path, &out->link_target);
    return 0;
}

void attributes_free(PathAttributes *attributes) {
    if (attributes == NULL) return;
    int error = errno;
    free(attributes->link_target);
    *attributes = (PathAttributes){0};
    errno = error;
}

static const char *file_type(mode_t mode) {
    if (S_ISREG(mode)) return "Tệp thường";
    if (S_ISDIR(mode)) return "Thư mục";
    if (S_ISLNK(mode)) return "Liên kết tượng trưng";
    if (S_ISFIFO(mode)) return "FIFO (ống có tên)";
    if (S_ISSOCK(mode)) return "Socket";
    if (S_ISCHR(mode)) return "Thiết bị ký tự";
    if (S_ISBLK(mode)) return "Thiết bị khối";
    return "Không xác định";
}

static void format_permissions(mode_t mode, char output[10]) {
    static const mode_t bits[] = {
        S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH
    };
    const char *symbols = "rwxrwxrwx";
    for (size_t i = 0; i < 9; i++) output[i] = (mode & bits[i]) ? symbols[i] : '-';
    if (mode & S_ISUID) output[2] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) output[5] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) output[8] = (mode & S_IXOTH) ? 't' : 'T';
    output[9] = '\0';
}

static int format_time(time_t value, char *output, size_t capacity) {
    struct tm local;
    if (localtime_r(&value, &local) == NULL) return -1;
    if (strftime(output, capacity, "%Y-%m-%d %H:%M:%S %z", &local) == 0) {
        errno = EOVERFLOW;
        return -1;
    }
    return 0;
}

int attributes_print(FILE *stream, const char *path, const PathAttributes *attributes) {
    if (stream == NULL || path == NULL || attributes == NULL ||
        (S_ISLNK(attributes->info.st_mode) && attributes->link_target == NULL)) {
        errno = EINVAL;
        return -1;
    }
    const struct stat *info = &attributes->info;
    char accessed[64], modified[64], changed[64], permissions[10];
    if (format_time(info->st_atime, accessed, sizeof(accessed)) != 0 ||
        format_time(info->st_mtime, modified, sizeof(modified)) != 0 ||
        format_time(info->st_ctime, changed, sizeof(changed)) != 0) return -1;
    format_permissions(info->st_mode, permissions);

    if (fprintf(stream,
                "\n=== THÔNG TIN HỆ THỐNG ===\n"
                "Đường dẫn  : %s\n"
                "Loại       : %s\n"
                "Dung lượng : %" PRIdMAX " byte (%.2f KiB)\n"
                "Quyền      : %s (%04" PRIoMAX ")\n",
                path, file_type(info->st_mode), (intmax_t)info->st_size,
                (double)info->st_size / 1024, permissions,
                (uintmax_t)(info->st_mode & 07777)) < 0) return -1;

    struct passwd *owner = getpwuid(info->st_uid);
    if (owner != NULL) {
        if (fprintf(stream, "Chủ sở hữu : %" PRIuMAX " (%s)\n",
                    (uintmax_t)info->st_uid, owner->pw_name) < 0) return -1;
    } else if (fprintf(stream, "Chủ sở hữu : %" PRIuMAX "\n", (uintmax_t)info->st_uid) < 0) {
        return -1;
    }
    struct group *group = getgrgid(info->st_gid);
    if (group != NULL) {
        if (fprintf(stream, "Nhóm       : %" PRIuMAX " (%s)\n",
                    (uintmax_t)info->st_gid, group->gr_name) < 0) return -1;
    } else if (fprintf(stream, "Nhóm       : %" PRIuMAX "\n", (uintmax_t)info->st_gid) < 0) {
        return -1;
    }

    if (fprintf(stream,
                "Inode      : %" PRIuMAX "\n"
                "Liên kết cứng : %" PRIuMAX "\n"
                "Truy cập   : %s (atime)\n"
                "Sửa đổi    : %s (mtime)\n"
                "Trạng thái : %s (ctime; không phải ngày tạo)\n",
                (uintmax_t)info->st_ino, (uintmax_t)info->st_nlink,
                accessed, modified, changed) < 0) return -1;
    if (S_ISLNK(info->st_mode) &&
        fprintf(stream, "Đích liên kết : %s\n", attributes->link_target) < 0) return -1;
    if (S_ISDIR(info->st_mode) &&
        fputs("Ghi chú    : Kích thước thư mục do hệ thống trả về, không cộng dung lượng bên trong.\n",
              stream) == EOF) return -1;

    return fflush(stream) == 0 ? 0 : -1;
}
