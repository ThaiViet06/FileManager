#include "directory.h"
#include "path_utils.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

/* Đóng thư mục mà không làm mất lỗi xảy ra trước đó. */
static int finish_directory(DIR *dir, int error) {
    int result = closedir(dir);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return result;
}

static int is_dot_entry(const char *name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static int matches_filter(const struct stat *info, DirectoryFilter filter) {
    return filter == DIRECTORY_FILTER_ALL ||
           (filter == DIRECTORY_FILTER_FILES && S_ISREG(info->st_mode)) ||
           (filter == DIRECTORY_FILTER_DIRECTORIES && S_ISDIR(info->st_mode));
}

static int contains_case_insensitive(const char *text, const char *needle) {
    size_t needle_length = strlen(needle);
    if (needle_length == 0) return 1;

    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        if (strncasecmp(cursor, needle, needle_length) == 0) return 1;
    }
    return 0;
}

int directory_list(const char *base_dir, DirectoryFilter filter,
                   DirectoryEntry *entries, size_t capacity,
                   size_t *count, int *truncated) {
    if (count != NULL) *count = 0;
    if (truncated != NULL) *truncated = 0;
    if (base_dir == NULL || count == NULL || truncated == NULL ||
        (entries == NULL && capacity > 0) ||
        (filter != DIRECTORY_FILTER_ALL && filter != DIRECTORY_FILTER_FILES &&
         filter != DIRECTORY_FILTER_DIRECTORIES)) {
        errno = EINVAL;
        return -1;
    }
    if (strlen(base_dir) >= FM_PATH_CAP) {
        errno = ENAMETOOLONG;
        return -1;
    }

    DIR *dir = opendir(base_dir);
    if (dir == NULL) return -1;

    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) return finish_directory(dir, errno);
        if (is_dot_entry(entry->d_name)) continue;

        char path[FM_PATH_CAP];
        if (path_join(path, sizeof(path), base_dir, entry->d_name) != 0) {
            return finish_directory(dir, errno);
        }
        struct stat info;
        if (lstat(path, &info) != 0) return finish_directory(dir, errno);
        if (!matches_filter(&info, filter)) continue;

        if (*count == capacity) {
            *truncated = 1;
            return finish_directory(dir, 0);
        }
        DirectoryEntry *item = &entries[*count];
        strcpy(item->name, entry->d_name);
        strcpy(item->path, path);
        item->info = info;
        (*count)++;
    }
}

static int print_tree_recursive(FILE *stream, const char *base_dir,
                                int depth, int max_depth) {
    DIR *dir = opendir(base_dir);
    if (dir == NULL) return -1;

    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) return finish_directory(dir, errno);
        if (is_dot_entry(entry->d_name)) continue;

        for (int i = 0; i < depth; i++) {
            if (fputs("│   ", stream) == EOF) return finish_directory(dir, EIO);
        }
        if (fprintf(stream, "├── %s\n", entry->d_name) < 0) {
            return finish_directory(dir, EIO);
        }

        char path[FM_PATH_CAP * 2];
        if (path_join(path, sizeof(path), base_dir, entry->d_name) != 0) {
            return finish_directory(dir, errno);
        }
        struct stat info;
        if (stat(path, &info) != 0) return finish_directory(dir, errno);
        if (S_ISDIR(info.st_mode) && depth < max_depth &&
            print_tree_recursive(stream, path, depth + 1, max_depth) != 0) {
            return finish_directory(dir, errno);
        }
    }
}

int directory_print_tree(FILE *stream, const char *base_dir, int max_depth) {
    if (stream == NULL || base_dir == NULL || max_depth < 0) {
        errno = EINVAL;
        return -1;
    }
    return print_tree_recursive(stream, base_dir, 0, max_depth);
}

static int search_recursive(FILE *stream, const char *base_dir,
                            const char *keyword, size_t *match_count) {
    DIR *dir = opendir(base_dir);
    if (dir == NULL) return -1;

    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) return finish_directory(dir, errno);
        if (is_dot_entry(entry->d_name)) continue;

        char path[FM_PATH_CAP * 2];
        if (path_join(path, sizeof(path), base_dir, entry->d_name) != 0) {
            return finish_directory(dir, errno);
        }
        struct stat info;
        if (stat(path, &info) != 0) return finish_directory(dir, errno);

        if (contains_case_insensitive(entry->d_name, keyword)) {
            (*match_count)++;
            int result;
            if (S_ISDIR(info.st_mode)) {
                result = fprintf(stream, " [%zu] [DIR ] %s\n", *match_count, path);
            } else {
                result = fprintf(stream, " [%zu] [FILE] %s (%.2f KB)\n",
                                 *match_count, path, (double)info.st_size / 1024);
            }
            if (result < 0) return finish_directory(dir, EIO);
        }

        if (S_ISDIR(info.st_mode) &&
            search_recursive(stream, path, keyword, match_count) != 0) {
            return finish_directory(dir, errno);
        }
    }
}

int directory_search(FILE *stream, const char *base_dir, const char *keyword,
                     size_t *match_count) {
    if (match_count != NULL) *match_count = 0;
    if (stream == NULL || base_dir == NULL || keyword == NULL ||
        keyword[0] == '\0' || match_count == NULL) {
        errno = EINVAL;
        return -1;
    }
    return search_recursive(stream, base_dir, keyword, match_count);
}
