#ifndef FM_ATTRIBUTES_H
#define FM_ATTRIBUTES_H

#include <stdio.h>
#include <sys/stat.h>

typedef struct {
    struct stat info;
    /* Bộ nhớ do attributes_read cấp phát, NULL với đường dẫn không phải liên kết. */
    char *link_target;
} PathAttributes;

/*
 * Đọc thuộc tính bằng lstat() và đích liên kết bằng readlink(), không đi theo đích.
 * Khởi tạo out bằng 0; gọi attributes_free() trước khi dùng lại hoặc hết sử dụng.
 * Trả 0 khi thành công, -1 khi lỗi và đặt errno; không tự in thông báo lỗi.
 */
int attributes_read(const char *path, PathAttributes *out);

/* Giải phóng dữ liệu đã đọc, đưa về trạng thái rỗng và giữ nguyên errno. */
void attributes_free(PathAttributes *attributes);

/*
 * In loại, kích thước, quyền, chủ sở hữu, inode, số liên kết, thời gian và đích.
 * Tra tên người dùng/nhóm để trình bày; nếu không tìm được vẫn hiển thị UID/GID.
 * Trả 0 khi thành công, -1 khi đối số, thời gian hoặc thao tác ghi bị lỗi.
 * Không đọc bàn phím và không mở tệp để xem nội dung.
 */
int attributes_print(FILE *stream, const char *path, const PathAttributes *attributes);

#endif
