#include "path_utils.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int path_join(char *out, size_t out_size, const char *directory, const char *name) {
    if (out == NULL || out_size == 0 || directory == NULL || name == NULL) {
        if (out != NULL && out_size > 0) out[0] = '\0';
        errno = EINVAL;
        return -1;
    }

    size_t directory_length = strlen(directory);
    const char *separator = "";
    if (directory_length > 0 && directory[directory_length - 1] != '/' && name[0] != '\0') {
        separator = "/";
    }

    int length = snprintf(out, out_size, "%s%s%s", directory, separator, name);
    if (length < 0) {
        out[0] = '\0';
        return -1;
    }
    if ((size_t)length >= out_size) {
        out[0] = '\0';
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

const char *path_basename(const char *path) {
    if (path == NULL) return NULL;

    const char *separator = strrchr(path, '/');
    return separator != NULL ? separator + 1 : path;
}
