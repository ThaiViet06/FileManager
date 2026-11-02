#ifndef FM_PATH_UTILS_H
#define FM_PATH_UTILS_H

#include <stddef.h>

/*
 * Nối directory với tên hoặc đường dẫn tương đối name theo cú pháp POSIX.
 * Chấp nhận chuỗi rỗng; không chuẩn hóa '.' hay '..'. out không được trùng đầu vào.
 * Trả 0 khi thành công, -1 khi lỗi và đặt errno; đầu vào không hợp lệ là EINVAL,
 * thiếu chỗ chứa kết quả là ENAMETOOLONG. Khi lỗi, xóa out nếu bộ đệm hợp lệ
 * để bên gọi không sử dụng đường dẫn bị cắt ngắn.
 */
int path_join(char *out, size_t out_size, const char *directory, const char *name);

/*
 * Trả con trỏ đến phần cuối sau '/' trong path, không cấp phát bộ nhớ.
 * Đường dẫn rỗng hoặc kết thúc bằng '/' cho kết quả rỗng; NULL trả NULL.
 */
const char *path_basename(const char *path);

#endif
