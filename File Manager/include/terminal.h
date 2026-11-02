#ifndef FM_TERMINAL_H
#define FM_TERMINAL_H

#include <stddef.h>

typedef struct {
    const char *label;
    int value;
} TerminalOption;

/* Chỉ bật giao diện ANSI khi cả stdin/stdout là terminal phù hợp. */
int terminal_available(void);

/*
 * Chọn bằng mũi tên và Enter; Esc/Ctrl-D hủy, số là phím tắt tùy chọn.
 * Menu chỉ có số 0-9 dùng phím số mới nhất; bảng có mục >=10 cho nhập nhiều chữ số.
 * Trả 1 khi chọn, 0 khi hủy/EOF, -1 khi lỗi và đặt errno.
 * Luôn khôi phục chế độ terminal, con trỏ và màn hình trước khi trả về.
 */
int terminal_select(const char *title, const TerminalOption *options, size_t count,
                    size_t initial, int *value);

#endif
