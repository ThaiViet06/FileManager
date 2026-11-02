#ifndef FM_PREVIEW_H
#define FM_PREVIEW_H

#include <stdio.h>

/*
 * Xem trước tệp thường: PDF/DOCX dùng công cụ ngoài, các tệp khác đọc như văn bản.
 * Giới hạn theo số dòng thực, không chia một dòng dài thành nhiều dòng đếm.
 * Không đọc bàn phím; truyền đường dẫn trực tiếp cho tiến trình, không ghép shell.
 * Trả 0 khi thành công, -1 khi lỗi và đặt errno. Từng công cụ lỗi được ghi rõ
 * trong kết quả; vẫn thử xem nội dung nếu công cụ metadata không chạy được.
 */
int preview_show(FILE *stream, const char *path);

#endif
