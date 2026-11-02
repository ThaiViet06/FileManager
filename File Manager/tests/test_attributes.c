#include "attributes.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

/* Giả lập hệ thống không tra được tên, không cần sửa UID/GID của tệp thật. */
struct passwd *__wrap_getpwuid(uid_t uid) { (void)uid; return NULL; }
struct group *__wrap_getgrgid(gid_t gid) { (void)gid; return NULL; }

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void) {
    PathAttributes attributes = {0};
    CHECK(attributes_read(NULL, &attributes) == -1 && errno == EINVAL);
    CHECK(attributes_read("", &attributes) == -1 && errno == EINVAL);
    CHECK(attributes_read("/dev/null", NULL) == -1 && errno == EINVAL);
    CHECK(attributes_read("/dev/null/child", &attributes) == -1 && errno == ENOTDIR);
    CHECK(attributes_read("/dev/null", &attributes) == 0);
    CHECK(S_ISCHR(attributes.info.st_mode) && attributes.link_target == NULL);
    attributes_free(&attributes);

    attributes.info.st_mode = S_IFREG | 0644;
    attributes.info.st_uid = 12345;
    attributes.info.st_gid = 23456;
    FILE *output = tmpfile();
    CHECK(output != NULL);
    CHECK(attributes_print(output, "synthetic", &attributes) == 0);
    rewind(output);
    char buffer[4096];
    size_t length = fread(buffer, 1, sizeof(buffer) - 1, output);
    buffer[length] = '\0';
    CHECK(strstr(buffer, "Chủ sở hữu : 12345\n") != NULL);
    CHECK(strstr(buffer, "Nhóm       : 23456\n") != NULL);
    CHECK(fclose(output) == 0);

    output = fopen("/dev/full", "w");
    CHECK(output != NULL);
    CHECK(attributes_print(output, "synthetic", &attributes) == -1 && errno == ENOSPC);
    fclose(output);
    CHECK(attributes_print(NULL, "synthetic", &attributes) == -1 && errno == EINVAL);
    attributes.info.st_mode = S_IFLNK | 0777;
    CHECK(attributes_print(stdout, "link", &attributes) == -1 && errno == EINVAL);

    attributes.link_target = malloc(128);
    CHECK(attributes.link_target != NULL);
    errno = EACCES;
    attributes_free(&attributes);
    CHECK(attributes.link_target == NULL && errno == EACCES);
    attributes_free(&attributes);
    CHECK(errno == EACCES);
    attributes_free(NULL);
    CHECK(errno == EACCES);
    puts("PASS attributes API: invalid input, UID/GID fallback, write error, cleanup");
    return 0;
}
