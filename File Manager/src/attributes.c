#include "attributes.h"

#include <errno.h>
#include <inttypes.h>
#include <time.h>

int attributes_read(const char *path, struct stat *out) {
    if (path == NULL || out == NULL) {
        errno = EINVAL;
        return -1;
    }
    return stat(path, out);
}

int attributes_print(FILE *stream, const char *path, const struct stat *info) {
    if (stream == NULL || path == NULL || info == NULL) {
        errno = EINVAL;
        return -1;
    }

    char time_buf[64];
    struct tm *modified_time = localtime(&info->st_mtime);
    if (modified_time == NULL ||
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", modified_time) == 0) {
        errno = EOVERFLOW;
        return -1;
    }

    if (fprintf(stream,
                "\n=== THÔNG TIN HỆ THỐNG ===\n"
                "Đường dẫn  : %s\n"
                "Dung lượng : %" PRIdMAX " bytes (%.2f KB)\n"
                "Sửa đổi    : %s\n",
                path, (intmax_t)info->st_size, (double)info->st_size / 1024, time_buf) < 0) {
        return -1;
    }
    return 0;
}
