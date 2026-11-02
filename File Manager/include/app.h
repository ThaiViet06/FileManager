#ifndef FM_APP_H
#define FM_APP_H

#include "config.h"

typedef struct {
    /* Chuỗi rỗng biểu thị chưa chọn; chỉ app cập nhật trạng thái này. */
    char selected_path[FM_PATH_CAP];
} AppContext;

/* Context được khởi tạo bằng 0 trước lần chạy đầu. Trả mã thoát của ứng dụng. */
int app_run(AppContext *context);

#endif
