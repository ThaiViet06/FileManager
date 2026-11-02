#ifndef FM_ATTRIBUTES_H
#define FM_ATTRIBUTES_H

#include <stdio.h>
#include <sys/stat.h>

/*
 * Đọc thuộc tính bằng stat(), giữ cách xử lý đường dẫn hiện tại.
 * Trả 0 khi thành công, -1 khi lỗi và đặt errno; không tự in thông báo lỗi.
 */
int attributes_read(const char *path, struct stat *out);

/*
 * In đường dẫn, kích thước và thời gian sửa đổi từ dữ liệu đã đọc.
 * Trả 0 khi thành công, -1 khi đối số, thời gian hoặc thao tác ghi bị lỗi.
 * Không đọc bàn phím và không mở tệp để xem nội dung.
 */
int attributes_print(FILE *stream, const char *path, const struct stat *info);

#endif
