#ifndef FM_FILE_OPS_H
#define FM_FILE_OPS_H

/*
 * Chỉ thao tác với tệp thường, không chấp nhận thư mục hoặc liên kết tượng trưng.
 * Các hàm trả 0 khi thành công, -1 khi lỗi và đặt errno; không nhập/xuất giao diện.
 * Nguồn và đích cùng tệp (kể cả hard link) bị từ chối với EINVAL.
 * Đích đã tồn tại mà allow_overwrite bằng 0 bị từ chối với EEXIST.
 */
int file_ops_copy(const char *source, const char *destination, int allow_overwrite);
int file_ops_move(const char *source, const char *destination, int allow_overwrite);
int file_ops_delete(const char *path);

/*
 * Sao chép kiểm tra cả lỗi đọc, ghi và đóng tệp. Khi lỗi, đích vừa tạo được dọn;
 * đích có sẵn đã cho phép ghi đè có thể chứa dữ liệu chưa đầy đủ nếu lỗi I/O.
 * Di chuyển không thực hiện sao chép dự phòng khi khác hệ thống tệp (EXDEV).
 */

#endif
