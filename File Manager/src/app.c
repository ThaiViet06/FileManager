#include "app.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "attributes.h"
#include "config.h"
#include "directory.h"
#include "file_ops.h"
#include "path_utils.h"
#include "preview.h"
#include "ui.h"

static int pick_source_file(char *path, size_t capacity)
{
    return ui_pick_file(".", path, capacity) == 1;
}

/* Kiểm tra trước khi hỏi đích hoặc xác nhận xóa. file_ops kiểm tra lại khi làm. */
static int require_regular_source(const char *path)
{
    struct stat info;
    if (lstat(path, &info) != 0)
    {
        perror("Không thể đọc đường dẫn nguồn");
        return 0;
    }
    if (!S_ISREG(info.st_mode))
    {
        printf(">> Thao tác này chỉ hỗ trợ tệp thường; đường dẫn đang chọn được giữ nguyên.\n");
        return 0;
    }
    return 1;
}

// 1. Quản lý chọn / đổi đường dẫn mặc định
static void select_or_change_path(AppContext *context)
{
    if (strlen(context->selected_path) > 0)
    {
        int action = ui_selected_path_action(context->selected_path);
        if (action == 2)
        {
            context->selected_path[0] = '\0';
            printf(">> Đã hủy chọn đường dẫn mặc định.\n");
            return;
        }
        else if (action != 1)
        {
            return;
        }
    }

    char temp_path[FM_PATH_CAP];
    if (ui_pick_path(".", temp_path, sizeof(temp_path)) == 1)
    {
        strncpy(context->selected_path, temp_path, sizeof(context->selected_path) - 1);
        context->selected_path[sizeof(context->selected_path) - 1] = '\0';
        printf(">> Đã đặt '%s' làm đường dẫn mặc định thành công!\n", context->selected_path);
    }
}

// 2. Xem thông tin đường dẫn
static void show_path_info(AppContext *context)
{
    char filepath[FM_PATH_CAP];
    if (strlen(context->selected_path) > 0)
    {
        strncpy(filepath, context->selected_path, sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
        printf("\n>> Đang dùng đường dẫn mặc định: %s\n", filepath);
    }
    else
    {
        if (ui_pick_path(".", filepath, sizeof(filepath)) != 1)
            return;
    }

    PathAttributes attributes = {0};
    if (attributes_read(filepath, &attributes) != 0)
    {
        attributes_free(&attributes);
        perror("Lỗi đọc thông tin đường dẫn");
        return;
    }

    if (attributes_print(stdout, filepath, &attributes) != 0)
    {
        attributes_free(&attributes);
        perror("Lỗi hiển thị thuộc tính");
        return;
    }

    int is_regular = S_ISREG(attributes.info.st_mode);
    attributes_free(&attributes);
    if (!is_regular)
        return;

    if (preview_show(stdout, filepath) != 0)
    {
        perror("Lỗi xem trước nội dung");
    }
}

// 3. Hiển thị cây thư mục
static void show_directory_tree(void)
{
    char path[FM_PATH_CAP];
    printf("\nNhập thư mục (Enter để xem thư mục hiện tại): ");
    if (ui_read_line(NULL, path, sizeof(path)) != 1)
        return;
    if (strlen(path) == 0)
        strcpy(path, ".");
    printf("\n%s\n", path);
    if (directory_print_tree(stdout, path, FM_TREE_MAX_DEPTH) != 0)
    {
        perror("Lỗi hiển thị cây thư mục");
    }
}

// 4. Di chuyển / Đổi tên file
static void move_file(AppContext *context)
{
    char src[FM_PATH_CAP], dest_dir[FM_PATH_CAP];
    char dest_path[FM_PATH_CAP];

    if (strlen(context->selected_path) > 0)
    {
        strncpy(src, context->selected_path, sizeof(src) - 1);
        src[sizeof(src) - 1] = '\0';
        printf("\n>> Đang thao tác trên file nguồn mặc định: '%s'\n", src);
    }
    else
    {
        printf("\n--- Chọn file nguồn cần di chuyển ---");
        if (!pick_source_file(src, sizeof(src)))
            return;
    }

    if (!require_regular_source(src))
        return;

    if (ui_pick_directory(".", dest_dir, sizeof(dest_dir)) != 1)
        return;

    const char *filename = path_basename(src);

    char new_name[256];
    printf("Tên file ở vị trí đích (Enter để giữ nguyên '%s'): ", filename);
    if (ui_read_line(NULL, new_name, sizeof(new_name)) != 1)
        return;

    const char *dest_name = strlen(new_name) == 0 ? filename : new_name;
    if (path_join(dest_path, sizeof(dest_path), dest_dir, dest_name) != 0)
    {
        perror("Lỗi ghép đường dẫn di chuyển");
        return;
    }

    int result = file_ops_move(src, dest_path, 0);
    if (result != 0 && errno == EEXIST)
    {
        if (!ui_confirm_overwrite(dest_path))
        {
            printf(">> Đã hủy di chuyển.\n");
            return;
        }
        result = file_ops_move(src, dest_path, 1);
    }
    if (result == 0)
    {
        printf(">> Đã di chuyển '%s' -> '%s' thành công!\n", src, dest_path);
        if (strcmp(src, context->selected_path) == 0)
        {
            strncpy(context->selected_path, dest_path, sizeof(context->selected_path) - 1);
            context->selected_path[sizeof(context->selected_path) - 1] = '\0';
        }
    }
    else
    {
        perror("Lỗi di chuyển");
    }
}

// 5. Sao chép file
static void copy_file(AppContext *context)
{
    char src[FM_PATH_CAP], dest_dir[FM_PATH_CAP];
    char dest_path[FM_PATH_CAP];

    if (strlen(context->selected_path) > 0)
    {
        strncpy(src, context->selected_path, sizeof(src) - 1);
        src[sizeof(src) - 1] = '\0';
        printf("\n>> Đang thao tác trên file nguồn mặc định: '%s'\n", src);
    }
    else
    {
        printf("\n--- Chọn file nguồn cần sao chép ---");
        if (!pick_source_file(src, sizeof(src)))
            return;
    }

    if (!require_regular_source(src))
        return;

    if (ui_pick_directory(".", dest_dir, sizeof(dest_dir)) != 1)
        return;

    const char *filename = path_basename(src);

    char new_name[256];
    printf("Đặt tên cho file sao chép (Enter để thêm tiền tố 'copy_'): ");
    if (ui_read_line(NULL, new_name, sizeof(new_name)) != 1)
        return;

    char copy_name[FM_PATH_CAP];
    const char *dest_name = new_name;
    if (strlen(new_name) == 0)
    {
        int length = snprintf(copy_name, sizeof(copy_name), "copy_%s", filename);
        if (length < 0 || (size_t)length >= sizeof(copy_name))
        {
            if (length >= 0)
                errno = ENAMETOOLONG;
            perror("Lỗi tạo tên file sao chép");
            return;
        }
        dest_name = copy_name;
    }
    if (path_join(dest_path, sizeof(dest_path), dest_dir, dest_name) != 0)
    {
        perror("Lỗi ghép đường dẫn sao chép");
        return;
    }

    int result = file_ops_copy(src, dest_path, 0);
    if (result != 0 && errno == EEXIST)
    {
        if (!ui_confirm_overwrite(dest_path))
        {
            printf(">> Đã hủy sao chép.\n");
            return;
        }
        result = file_ops_copy(src, dest_path, 1);
    }
    if (result != 0)
    {
        perror("Lỗi sao chép");
        return;
    }
    printf(">> Đã sao chép thành công tới '%s'!\n", dest_path);
}

// 6. Xóa file
static void delete_file(AppContext *context)
{
    char target[FM_PATH_CAP];

    if (strlen(context->selected_path) > 0)
    {
        strncpy(target, context->selected_path, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
        printf("\n>> Đang thao tác trên file mặc định: '%s'\n", target);
    }
    else
    {
        printf("\n--- Chọn file cần xóa ---");
        if (!pick_source_file(target, sizeof(target)))
            return;
    }

    if (!require_regular_source(target))
        return;

    if (ui_confirm_delete(target))
    {
        if (file_ops_delete(target) == 0)
        {
            printf(">> Đã xóa file '%s' thành công!\n", target);
            if (strcmp(target, context->selected_path) == 0)
                context->selected_path[0] = '\0';
        }
        else
        {
            perror("Lỗi xóa file");
        }
    }
    else
    {
        printf(">> Đã hủy thao tác xóa.\n");
    }
}

// 7. Tìm kiếm file
static void search_file(void)
{
    char keyword[128];
    char start_dir[FM_PATH_CAP];

    printf("\nNhập từ khóa hoặc đuôi mở rộng (VD: demo, .txt, .pdf): ");
    if (ui_read_line(NULL, keyword, sizeof(keyword)) != 1)
        return;

    if (strlen(keyword) == 0)
    {
        printf(">> Từ khóa không được để trống!\n");
        return;
    }

    printf("Nhập thư mục tìm kiếm (Enter để tìm từ thư mục hiện tại '.'): ");
    if (ui_read_line(NULL, start_dir, sizeof(start_dir)) != 1)
        return;
    if (strlen(start_dir) == 0)
        strcpy(start_dir, ".");

    printf("\n--- Kết quả tìm kiếm cho '%s' trong '%s' ---\n", keyword, start_dir);
    size_t match_count = 0;
    if (directory_search(stdout, start_dir, keyword, &match_count) != 0)
    {
        perror("Lỗi tìm kiếm trong thư mục");
        return;
    }

    if (match_count == 0)
    {
        printf(">> Không tìm thấy kết quả nào.\n");
    }
    else
    {
        printf("--------------------------------------------\n");
        printf(">> Tìm thấy: %zu mục.\n", match_count);
    }
}

int app_run(AppContext *context)
{
    if (context == NULL)
    {
        errno = EINVAL;
        return 1;
    }
    for (;;)
    {
        int choice;
        if (!ui_choose_menu(context->selected_path, &choice))
            return 0;

        switch (choice)
        {
        case 1:
            select_or_change_path(context);
            break;
        case 2:
            show_path_info(context);
            break;
        case 3:
            show_directory_tree();
            break;
        case 4:
            move_file(context);
            break;
        case 5:
            copy_file(context);
            break;
        case 6:
            delete_file(context);
            break;
        case 7:
            search_file();
            break;
        case 0:
            printf("\nĐã thoát chương trình.\n");
            return 0;
        }
        if (!ui_wait_for_continue())
            return 0;
    }
}
