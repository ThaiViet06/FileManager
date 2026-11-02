#ifndef FM_UI_H
#define FM_UI_H

#include <stddef.h>

/*
 * Nhập một dòng, bỏ ký tự xuống dòng; prompt có thể là NULL.
 * Trả 1 khi đọc được dòng, 0 khi EOF, -1 khi lỗi hoặc dòng quá dài.
 * Khi dòng quá dài, bỏ hết phần còn lại, xóa buffer và báo lỗi.
 */
int ui_read_line(const char *prompt, char *buffer, size_t capacity);

/* TTY dùng mũi tên/Enter; đầu vào chuyển hướng dùng menu số. Esc chọn thoát. */
int ui_choose_menu(const char *selected_path, int *choice);
/* Dừng để xem kết quả ở TTY; đầu vào chuyển hướng không cần dừng. */
int ui_wait_for_continue(void);
/* Trả 1 để chọn khác, 2 để hủy chọn, 0 để quay lại hoặc khi EOF. */
int ui_selected_path_action(const char *selected_path);

/* TTY mặc định Hủy; đầu vào chuyển hướng chỉ xác nhận đúng dòng y hoặc Y. */
int ui_confirm_overwrite(const char *path);
int ui_confirm_delete(const char *path);

/*
 * Lấy danh sách qua directory_list(), hiển thị và nhận đường dẫn.
 * Trả 1 khi chọn được, 0 khi hủy/EOF, -1 khi lỗi (đặt errno).
 * Không đổi trạng thái app.
 * ui_pick_path nhận mọi loại; hai bộ lọc còn lại chỉ nhận đúng loại qua lstat().
 */
int ui_pick_path(const char *base_dir, char *selected_path, size_t capacity);
int ui_pick_file(const char *target_dir, char *selected_path, size_t capacity);
int ui_pick_directory(const char *base_dir, char *selected_dir, size_t capacity);

#endif
