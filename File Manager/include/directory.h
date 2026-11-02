#ifndef FM_DIRECTORY_H
#define FM_DIRECTORY_H

#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>

#include "config.h"

typedef enum {
    DIRECTORY_FILTER_ALL,
    DIRECTORY_FILTER_FILES,
    DIRECTORY_FILTER_DIRECTORIES
} DirectoryFilter;

typedef struct {
    char name[FM_PATH_CAP];
    char path[FM_PATH_CAP];
    struct stat info;
} DirectoryEntry;

/*
 * Liệt kê trực tiếp, bỏ qua '.' và '..', giữ thứ tự của hệ thống tệp.
 * Ghi tối đa capacity mục, đặt count và truncated khi còn mục phù hợp.
 * entries có thể là NULL nếu capacity bằng 0. Không in hoặc đọc bàn phím.
 * Trả 0 khi thành công, -1 khi lỗi và giữ thông tin lỗi trong errno.
 */
int directory_list(const char *base_dir, DirectoryFilter filter,
                   DirectoryEntry *entries, size_t capacity,
                   size_t *count, int *truncated);

/*
 * In các mục con, không in dòng tên thư mục gốc. Mức thụt lề đầu tiên là 0;
 * chỉ đi vào thư mục con khi mức hiện tại nhỏ hơn max_depth.
 * Trả 0 khi thành công, -1 khi đọc thư mục/thuộc tính hoặc ghi kết quả lỗi.
 */
int directory_print_tree(FILE *stream, const char *base_dir, int max_depth);

/*
 * Tìm tên không phân biệt hoa/thường trong cả cây, không giới hạn số kết quả.
 * Đặt lại match_count về 0; khi lỗi, số đếm phản ánh phần đã xử lý.
 * Trả 0 khi thành công, -1 khi lỗi và giữ thông tin lỗi trong errno.
 */
int directory_search(FILE *stream, const char *base_dir, const char *keyword,
                     size_t *match_count);

#endif
