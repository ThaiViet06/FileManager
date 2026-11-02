#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "attributes.h"
#include "config.h"
#include "directory.h"
#include "file_ops.h"
#include "path_utils.h"
#include "preview.h"

// Biến toàn cục lưu file được chọn mặc định
char g_selected_file[FM_PATH_CAP] = "";

void clean_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') str[len - 1] = '\0';
}

static int confirm_yes(void) {
    char answer[16];
    if (!fgets(answer, sizeof(answer), stdin)) return 0;
    if (strchr(answer, '\n') == NULL && !feof(stdin)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {}
        return 0;
    }
    clean_newline(answer);
    return strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0;
}

static int confirm_overwrite(const char *path) {
    printf("Đích '%s' đã tồn tại. Ghi đè? (y/n): ", path);
    return confirm_yes();
}

// Bảng chọn file
int pick_file(const char *target_dir, char *selected_path) {
    DirectoryEntry files[FM_MAX_ITEMS];
    size_t count;
    int truncated;
    if (directory_list(target_dir, DIRECTORY_FILTER_FILES, files,
                       FM_MAX_ITEMS, &count, &truncated) != 0) {
        perror("Không thể mở thư mục để liệt kê file");
        return 0;
    }

    printf("\nDanh sách file trong '%s':\n", target_dir);
    printf(" --------------------------------------------------\n");

    for (size_t i = 0; i < count; i++) {
        printf("  [%zu] %-30s (%.2f KB)\n", i + 1, files[i].name,
               (double)files[i].info.st_size / 1024);
    }

    if (count == 0) {
        printf("  (Không tìm thấy file nào trong thư mục này)\n");
    }
    if (truncated) {
        printf("  (Danh sách giới hạn %d file; dùng [0] để nhập đường dẫn khác)\n", FM_MAX_ITEMS);
    }

    printf("  [0] Tự nhập đường dẫn khác bằng tay\n");
    printf(" --------------------------------------------------\n");

    char buf[64];
    while (1) {
        printf("Chọn file theo số [0-%zu]: ", count);
        if (!fgets(buf, sizeof(buf), stdin)) return 0;
        clean_newline(buf);
        if (strlen(buf) == 0) continue;

        char *endptr;
        long choice = strtol(buf, &endptr, 10);
        if (*endptr != '\0' || choice < 0 || (size_t)choice > count) {
            printf(">> Số chọn không hợp lệ! Vui lòng nhập lại.\n");
            continue;
        }

        if (choice == 0) {
            printf("Nhập đường dẫn file: ");
            if (!fgets(selected_path, FM_PATH_CAP, stdin)) return 0;
            clean_newline(selected_path);
            return (strlen(selected_path) > 0);
        } else {
            strcpy(selected_path, files[(size_t)choice - 1].path);
            return 1;
        }
    }
}

// Bảng chọn thư mục đích
int pick_directory(const char *base_dir, char *selected_dir) {
    DirectoryEntry directories[FM_MAX_ITEMS - 1];
    size_t child_count;
    int truncated;
    if (directory_list(base_dir, DIRECTORY_FILTER_DIRECTORIES, directories,
                       FM_MAX_ITEMS - 1, &child_count, &truncated) != 0) {
        perror("Không thể mở thư mục để liệt kê");
        return 0;
    }

    size_t count = child_count + 1;
    printf("\nDanh sách thư mục đích khả dụng:\n");
    printf(" --------------------------------------------------\n");
    printf("  [1] %s (Thư mục gốc Midterm hiện tại)\n", base_dir);

    for (size_t i = 0; i < child_count; i++) {
        printf("  [%zu] %s/\n", i + 2, directories[i].name);
    }
    if (truncated) {
        printf("  (Danh sách giới hạn %d thư mục con; dùng [0] để nhập đường dẫn khác)\n",
               FM_MAX_ITEMS - 1);
    }

    printf("  [0] Tự nhập đường dẫn thư mục đích bằng tay\n");
    printf(" --------------------------------------------------\n");

    char buf[64];
    while (1) {
        printf("Chọn thư mục đích theo số [0-%zu]: ", count);
        if (!fgets(buf, sizeof(buf), stdin)) return 0;
        clean_newline(buf);
        if (strlen(buf) == 0) continue;

        char *endptr;
        long choice = strtol(buf, &endptr, 10);
        if (*endptr != '\0' || choice < 0 || (size_t)choice > count) {
            printf(">> Số chọn không hợp lệ! Vui lòng nhập lại.\n");
            continue;
        }

        if (choice == 0) {
            printf("Nhập đường dẫn đích: ");
            if (!fgets(selected_dir, FM_PATH_CAP, stdin)) return 0;
            clean_newline(selected_dir);
            return (strlen(selected_dir) > 0);
        } else {
            const char *path = choice == 1 ? base_dir : directories[(size_t)choice - 2].path;
            strcpy(selected_dir, path);
            return 1;
        }
    }
}

// 1. Quản lý chọn / đổi file mặc định
void select_or_change_file() {
    if (strlen(g_selected_file) > 0) {
        printf("\nFile đang chọn hiện tại là: '%s'\n", g_selected_file);
        printf("  [1] Chọn file khác\n");
        printf("  [2] Hủy chọn (Không dùng file mặc định)\n");
        printf("  [0] Quay lại menu\n");
        printf("Chọn thao tác: ");
        char buf[16];
        if (!fgets(buf, sizeof(buf), stdin)) return;
        clean_newline(buf);
        if (strcmp(buf, "2") == 0) {
            g_selected_file[0] = '\0';
            printf(">> Đã hủy chọn file mặc định.\n");
            return;
        } else if (strcmp(buf, "1") != 0) {
            return;
        }
    }

    char temp_path[FM_PATH_CAP];
    if (pick_file("test_files", temp_path) || pick_file(".", temp_path)) {
        strncpy(g_selected_file, temp_path, sizeof(g_selected_file) - 1);
        g_selected_file[sizeof(g_selected_file) - 1] = '\0';
        printf(">> Đã đặt '%s' làm file mặc định thành công!\n", g_selected_file);
    }
}

// 2. Xem thông tin file
void show_file_info() {
    char filepath[FM_PATH_CAP];
    if (strlen(g_selected_file) > 0) {
        strncpy(filepath, g_selected_file, sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
        printf("\n>> Đang dùng file mặc định: %s\n", filepath);
    } else {
        if (!pick_file("test_files", filepath) && !pick_file(".", filepath)) return;
    }

    struct stat st;
    if (attributes_read(filepath, &st) != 0) {
        perror("Lỗi đọc thông tin file");
        return;
    }

    if (attributes_print(stdout, filepath, &st) != 0) {
        perror("Lỗi hiển thị thuộc tính");
        return;
    }

    if (!S_ISREG(st.st_mode)) return;

    if (preview_show(stdout, filepath) != 0) {
        perror("Lỗi xem trước nội dung");
    }
}

// 3. Hiển thị cây thư mục
void show_directory_tree() {
    char path[FM_PATH_CAP];
    printf("\nNhập thư mục (Enter để xem thư mục Midterm hiện tại): ");
    if (!fgets(path, sizeof(path), stdin)) return;
    clean_newline(path);
    if (strlen(path) == 0) strcpy(path, ".");
    printf("\n%s\n", path);
    if (directory_print_tree(stdout, path, FM_TREE_MAX_DEPTH) != 0) {
        perror("Lỗi hiển thị cây thư mục");
    }
}

// 4. Di chuyển / Đổi tên file
void move_file() {
    char src[FM_PATH_CAP], dest_dir[FM_PATH_CAP];
    char dest_path[FM_PATH_CAP];

    if (strlen(g_selected_file) > 0) {
        strncpy(src, g_selected_file, sizeof(src) - 1);
        src[sizeof(src) - 1] = '\0';
        printf("\n>> Đang thao tác trên file nguồn mặc định: '%s'\n", src);
    } else {
        printf("\n--- Chọn file nguồn cần di chuyển ---");
        if (!pick_file("test_files", src) && !pick_file(".", src)) return;
    }

    if (!pick_directory(".", dest_dir)) return;

    const char *filename = path_basename(src);

    char new_name[256];
    printf("Tên file ở vị trí đích (Enter để giữ nguyên '%s'): ", filename);
    if (!fgets(new_name, sizeof(new_name), stdin)) return;
    clean_newline(new_name);

    const char *dest_name = strlen(new_name) == 0 ? filename : new_name;
    if (path_join(dest_path, sizeof(dest_path), dest_dir, dest_name) != 0) {
        perror("Lỗi ghép đường dẫn di chuyển");
        return;
    }

    int result = file_ops_move(src, dest_path, 0);
    if (result != 0 && errno == EEXIST) {
        if (!confirm_overwrite(dest_path)) {
            printf(">> Đã hủy di chuyển.\n");
            return;
        }
        result = file_ops_move(src, dest_path, 1);
    }
    if (result == 0) {
        printf(">> Đã di chuyển '%s' -> '%s' thành công!\n", src, dest_path);
        if (strcmp(src, g_selected_file) == 0) {
            strncpy(g_selected_file, dest_path, sizeof(g_selected_file) - 1);
            g_selected_file[sizeof(g_selected_file) - 1] = '\0';
        }
    } else {
        perror("Lỗi di chuyển");
    }
}

// 5. Sao chép file
void copy_file() {
    char src[FM_PATH_CAP], dest_dir[FM_PATH_CAP];
    char dest_path[FM_PATH_CAP];

    if (strlen(g_selected_file) > 0) {
        strncpy(src, g_selected_file, sizeof(src) - 1);
        src[sizeof(src) - 1] = '\0';
        printf("\n>> Đang thao tác trên file nguồn mặc định: '%s'\n", src);
    } else {
        printf("\n--- Chọn file nguồn cần sao chép ---");
        if (!pick_file("test_files", src) && !pick_file(".", src)) return;
    }

    if (!pick_directory(".", dest_dir)) return;

    const char *filename = path_basename(src);

    char new_name[256];
    printf("Đặt tên cho file sao chép (Enter để thêm tiền tố 'copy_'): ");
    if (!fgets(new_name, sizeof(new_name), stdin)) return;
    clean_newline(new_name);

    char copy_name[FM_PATH_CAP];
    const char *dest_name = new_name;
    if (strlen(new_name) == 0) {
        int length = snprintf(copy_name, sizeof(copy_name), "copy_%s", filename);
        if (length < 0 || (size_t)length >= sizeof(copy_name)) {
            if (length >= 0) errno = ENAMETOOLONG;
            perror("Lỗi tạo tên file sao chép");
            return;
        }
        dest_name = copy_name;
    }
    if (path_join(dest_path, sizeof(dest_path), dest_dir, dest_name) != 0) {
        perror("Lỗi ghép đường dẫn sao chép");
        return;
    }

    int result = file_ops_copy(src, dest_path, 0);
    if (result != 0 && errno == EEXIST) {
        if (!confirm_overwrite(dest_path)) {
            printf(">> Đã hủy sao chép.\n");
            return;
        }
        result = file_ops_copy(src, dest_path, 1);
    }
    if (result != 0) {
        perror("Lỗi sao chép");
        return;
    }
    printf(">> Đã sao chép thành công tới '%s'!\n", dest_path);
}

// 6. Xóa file
void delete_file() {
    char target[FM_PATH_CAP];

    if (strlen(g_selected_file) > 0) {
        strncpy(target, g_selected_file, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
        printf("\n>> Đang thao tác trên file mặc định: '%s'\n", target);
    } else {
        printf("\n--- Chọn file cần xóa ---");
        if (!pick_file("test_files", target) && !pick_file(".", target)) return;
    }

    printf("Bạn có chắc chắn muốn xóa '%s'? (y/n): ", target);
    if (confirm_yes()) {
        if (file_ops_delete(target) == 0) {
            printf(">> Đã xóa file '%s' thành công!\n", target);
            if (strcmp(target, g_selected_file) == 0) g_selected_file[0] = '\0';
        } else {
            perror("Lỗi xóa file");
        }
    } else {
        printf(">> Đã hủy thao tác xóa.\n");
    }
}

// 7. Tìm kiếm file
void search_file() {
    char keyword[128];
    char start_dir[FM_PATH_CAP];

    printf("\nNhập từ khóa hoặc đuôi mở rộng (VD: demo, .txt, .pdf): ");
    if (!fgets(keyword, sizeof(keyword), stdin)) return;
    clean_newline(keyword);

    if (strlen(keyword) == 0) {
        printf(">> Từ khóa không được để trống!\n");
        return;
    }

    printf("Nhập thư mục tìm kiếm (Enter để tìm từ thư mục hiện tại '.'): ");
    if (!fgets(start_dir, sizeof(start_dir), stdin)) return;
    clean_newline(start_dir);
    if (strlen(start_dir) == 0) strcpy(start_dir, ".");

    printf("\n--- Kết quả tìm kiếm cho '%s' trong '%s' ---\n", keyword, start_dir);
    size_t match_count = 0;
    if (directory_search(stdout, start_dir, keyword, &match_count) != 0) {
        perror("Lỗi tìm kiếm trong thư mục");
        return;
    }

    if (match_count == 0) {
        printf(">> Không tìm thấy kết quả nào.\n");
    } else {
        printf("--------------------------------------------\n");
        printf(">> Tìm thấy: %zu mục.\n", match_count);
    }
}

int main() {
    char buf[64];
    char *endptr;
    long choice;

    while (1) {
        printf("\n================ MIDTERM FILE MANAGER ================\n");
        if (strlen(g_selected_file) > 0) {
            printf(" [File đang chọn: %s]\n", g_selected_file);
        } else {
            printf(" [Chưa chọn file mặc định]\n");
        }
        printf("------------------------------------------------------\n");
        printf(" 1. Chọn / Đổi file mặc định\n");
        printf(" 2. Xem thông tin & nội dung file (TXT/PDF/DOCX)\n");
        printf(" 3. Hiển thị cây thư mục\n");
        printf(" 4. Di chuyển / Đổi tên file (Cho chọn thư mục đích)\n");
        printf(" 5. Sao chép file\n");
        printf(" 6. Xóa file\n");
        printf(" 7. Tìm kiếm file theo tên hoặc đuôi (.txt, .pdf,...)\n");
        printf(" 0. Thoát\n");
        printf("======================================================\n");

        while (1) {
            printf("Chọn chức năng [0-7]: ");
            if (!fgets(buf, sizeof(buf), stdin)) return 0;
            clean_newline(buf);

            if (strlen(buf) == 0) continue;

            choice = strtol(buf, &endptr, 10);

            if (*endptr != '\0' || choice < 0 || choice > 7) {
                printf(">> Lỗi: Lựa chọn '%s' không hợp lệ! Vui lòng chỉ nhập số từ 0 đến 7.\n", buf);
            } else {
                break;
            }
        }

        switch (choice) {
            case 1: select_or_change_file(); break;
            case 2: show_file_info(); break;
            case 3: show_directory_tree(); break;
            case 4: move_file(); break;
            case 5: copy_file(); break;
            case 6: delete_file(); break;
            case 7: search_file(); break;
            case 0:
                printf("\nĐã thoát chương trình.\n");
                return 0;
        }
    }
    return 0;
}
