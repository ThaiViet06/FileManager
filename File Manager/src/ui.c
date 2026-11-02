#include "ui.h"
#include "config.h"
#include "directory.h"
#include "terminal.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ui_read_line(const char *prompt, char *buffer, size_t capacity) {
    if (buffer == NULL || capacity < 2 || capacity > INT_MAX) {
        errno = EINVAL;
        return -1;
    }
    buffer[0] = '\0';
    if (prompt != NULL) fputs(prompt, stdout);
    fflush(stdout);
    if (fgets(buffer, (int)capacity, stdin) == NULL) {
        if (ferror(stdin)) {
            perror("Lỗi đọc dữ liệu nhập");
            return -1;
        }
        return 0;
    }

    char *newline = strchr(buffer, '\n');
    if (newline != NULL) {
        *newline = '\0';
        return 1;
    }

    int ch = getchar();
    if (ch == EOF && ferror(stdin)) {
        buffer[0] = '\0';
        perror("Lỗi đọc dữ liệu nhập");
        return -1;
    }
    if (ch == '\n' || ch == EOF) return 1;

    while ((ch = getchar()) != '\n' && ch != EOF) {}
    buffer[0] = '\0';
    printf(">> Dữ liệu nhập quá dài (tối đa %zu ký tự).\n", capacity - 1);
    errno = ENAMETOOLONG;
    return -1;
}

static int parse_choice(const char *text, long maximum, int *choice) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value < 0 || value > maximum) {
        return 0;
    }
    *choice = (int)value;
    return 1;
}

static void print_menu(const char *selected_path) {
    printf("\n================ MIDTERM FILE MANAGER ================\n");
    if (selected_path[0] != '\0') {
        printf(" [Đường dẫn đang chọn: %s]\n", selected_path);
    } else {
        printf(" [Chưa chọn đường dẫn mặc định]\n");
    }
    printf("------------------------------------------------------\n");
    printf(" 1. Chọn / Đổi đường dẫn mặc định (tệp/thư mục/liên kết)\n");
    printf(" 2. Xem thuộc tính đường dẫn & nội dung (TXT/PDF/DOCX)\n");
    printf(" 3. Hiển thị cây thư mục\n");
    printf(" 4. Di chuyển / Đổi tên file (Cho chọn thư mục đích)\n");
    printf(" 5. Sao chép file\n");
    printf(" 6. Xóa file\n");
    printf(" 7. Tìm kiếm file theo tên hoặc đuôi (.txt, .pdf,...)\n");
    printf(" 0. Thoát\n");
    printf("======================================================\n");
}

int ui_choose_menu(const char *selected_path, int *choice) {
    if (terminal_available()) {
        const TerminalOption options[] = {
            {"1. Chọn / Đổi đường dẫn (tệp, thư mục, liên kết)", 1},
            {"2. Xem thuộc tính và nội dung (TXT/PDF/DOCX)", 2},
            {"3. Hiển thị cây thư mục", 3},
            {"4. Di chuyển / Đổi tên tệp", 4},
            {"5. Sao chép tệp", 5},
            {"6. Xóa tệp", 6},
            {"7. Tìm kiếm theo tên hoặc phần mở rộng", 7},
            {"0. Thoát", 0}
        };
        char title[FM_PATH_CAP + 80];
        snprintf(title, sizeof(title), "FILE MANAGER | %s",
                 selected_path[0] ? selected_path : "Chưa chọn đường dẫn");
        int result = terminal_select(title, options, sizeof(options) / sizeof(options[0]), 0, choice);
        if (result < 0) perror("Lỗi điều khiển terminal");
        if (result == 0) *choice = 0;
        return result >= 0;
    }
    print_menu(selected_path);
    char buffer[64];
    for (;;) {
        int result = ui_read_line("Chọn chức năng [0-7]: ", buffer, sizeof(buffer));
        if (result == 0 || ferror(stdin)) return 0;
        if (result < 0 || buffer[0] == '\0') continue;
        if (parse_choice(buffer, 7, choice)) return 1;
        printf(">> Lỗi: Lựa chọn '%s' không hợp lệ! Vui lòng chỉ nhập số từ 0 đến 7.\n", buffer);
    }
}

int ui_wait_for_continue(void) {
    if (!terminal_available()) return 1;
    char buffer[64];
    for (;;) {
        int result = ui_read_line("\nNhấn Enter để quay lại menu... ", buffer, sizeof(buffer));
        if (result == 0 || ferror(stdin)) return 0;
        if (result == 1 && buffer[0] == '\0') return 1;
    }
}

int ui_selected_path_action(const char *selected_path) {
    if (terminal_available()) {
        const TerminalOption options[] = {
            {"Chọn đường dẫn khác", 1}, {"Hủy chọn đường dẫn hiện tại", 2}, {"Quay lại menu", 0}
        };
        char title[FM_PATH_CAP + 80];
        snprintf(title, sizeof(title), "Đường dẫn đang chọn: %s", selected_path);
        int choice;
        int result = terminal_select(title, options, 3, 0, &choice);
        if (result < 0) perror("Lỗi điều khiển terminal");
        return result == 1 ? choice : 0;
    }
    printf("\nĐường dẫn đang chọn hiện tại là: '%s'\n", selected_path);
    printf("  [1] Chọn đường dẫn khác\n");
    printf("  [2] Hủy chọn (Không dùng đường dẫn mặc định)\n");
    printf("  [0] Quay lại menu\n");
    char buffer[16];
    if (ui_read_line("Chọn thao tác: ", buffer, sizeof(buffer)) != 1) return 0;
    if (strcmp(buffer, "1") == 0) return 1;
    if (strcmp(buffer, "2") == 0) return 2;
    return 0;
}

static int confirm_yes(void) {
    char answer[16];
    return ui_read_line(NULL, answer, sizeof(answer)) == 1 &&
           (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0);
}

int ui_confirm_overwrite(const char *path) {
    if (terminal_available()) {
        const TerminalOption options[] = {{"Hủy, giữ nguyên tệp đích", 0}, {"Ghi đè tệp đích", 1}};
        char title[FM_PATH_CAP + 80];
        snprintf(title, sizeof(title), "Đích đã tồn tại: %s", path);
        int choice;
        return terminal_select(title, options, 2, 0, &choice) == 1 && choice == 1;
    }
    printf("Đích '%s' đã tồn tại. Ghi đè? (y/n): ", path);
    return confirm_yes();
}

int ui_confirm_delete(const char *path) {
    if (terminal_available()) {
        const TerminalOption options[] = {{"Hủy, giữ nguyên tệp", 0}, {"Xóa tệp", 1}};
        char title[FM_PATH_CAP + 80];
        snprintf(title, sizeof(title), "Xác nhận xóa: %s", path);
        int choice;
        return terminal_select(title, options, 2, 0, &choice) == 1 && choice == 1;
    }
    printf("Bạn có chắc chắn muốn xóa '%s'? (y/n): ", path);
    return confirm_yes();
}

static const char *entry_type(const struct stat *info) {
    if (S_ISREG(info->st_mode)) return "FILE";
    if (S_ISDIR(info->st_mode)) return "DIR";
    if (S_ISLNK(info->st_mode)) return "LINK";
    return "OTHER";
}

static int validate_path(const char *path, DirectoryFilter filter) {
    struct stat info;
    if (lstat(path, &info) != 0) {
        int error = errno;
        perror("Không thể chọn đường dẫn");
        errno = error;
        return -1;
    }
    if ((filter == DIRECTORY_FILTER_FILES && !S_ISREG(info.st_mode)) ||
        (filter == DIRECTORY_FILTER_DIRECTORIES && !S_ISDIR(info.st_mode))) {
        fputs(filter == DIRECTORY_FILTER_FILES ? ">> Chỉ chấp nhận tệp thường.\n" :
                                                ">> Chỉ chấp nhận thư mục.\n", stdout);
        errno = EINVAL;
        return -1;
    }
    return 1;
}

static int accept_path_choice(const char *base_dir, const DirectoryEntry *entries,
                               DirectoryFilter filter, int choice,
                               char *selected_path, size_t capacity) {
    int directories = filter == DIRECTORY_FILTER_DIRECTORIES;
    if (choice < 0) return 0;
    if (choice == 0) {
        int result = ui_read_line(directories ? "Nhập đường dẫn đích: " : "Nhập đường dẫn: ",
                                  selected_path, capacity);
        if (result != 1) return result;
        if (selected_path[0] == '\0') return 0;
        return validate_path(selected_path, filter);
    }
    const char *path = directories && choice == 1 ? base_dir :
                       entries[(size_t)choice - 1 - (size_t)directories].path;
    if (strlen(path) >= capacity) {
        errno = ENAMETOOLONG;
        perror("Đường dẫn được chọn quá dài");
        return -1;
    }
    strcpy(selected_path, path);
    return validate_path(selected_path, filter);
}

static int pick_with_arrows(const char *base_dir, const DirectoryEntry *entries,
                            size_t count, int truncated, DirectoryFilter filter,
                            char *selected_path, size_t capacity) {
    TerminalOption options[FM_MAX_ITEMS + 2];
    char labels[FM_MAX_ITEMS][FM_PATH_CAP + 80];
    size_t length = 0;
    int directories = filter == DIRECTORY_FILTER_DIRECTORIES;
    if (directories) options[length++] = (TerminalOption){"[1] Thư mục hiện tại", 1};
    for (size_t i = 0; i < count; i++) {
        snprintf(labels[i], sizeof(labels[i]), "[%zu] [%-4s] %s", i + 1 + (size_t)directories,
                 entry_type(&entries[i].info), entries[i].name);
        options[length++] = (TerminalOption){labels[i], (int)(i + 1 + (size_t)directories)};
    }
    options[length++] = (TerminalOption){"[0] Nhập đường dẫn khác...", 0};
    options[length++] = (TerminalOption){"Quay lại", -1};
    char title[FM_PATH_CAP + 120];
    snprintf(title, sizeof(title), "Chọn %s trong '%s'%s",
             directories ? "thư mục đích" : filter == DIRECTORY_FILTER_FILES ? "tệp thường" : "đường dẫn",
             base_dir, truncated ? " (danh sách đã giới hạn)" : "");
    int choice;
    int result = terminal_select(title, options, length, 0, &choice);
    if (result != 1) return result;
    return accept_path_choice(base_dir, entries, filter, choice, selected_path, capacity);
}

static int pick_path(const char *base_dir, DirectoryFilter filter,
                     char *selected_path, size_t capacity) {
    if (base_dir == NULL || selected_path == NULL || capacity < 2 || capacity > INT_MAX) {
        errno = EINVAL;
        return -1;
    }
    selected_path[0] = '\0';
    int directories = filter == DIRECTORY_FILTER_DIRECTORIES;
    DirectoryEntry entries[FM_MAX_ITEMS];
    size_t count;
    int truncated;
    if (directory_list(base_dir, filter, entries, FM_MAX_ITEMS - (size_t)directories,
                       &count, &truncated) != 0) {
        int error = errno;
        perror(directories ? "Không thể mở thư mục để liệt kê" :
                            "Không thể mở thư mục để liệt kê đường dẫn");
        errno = error;
        return -1;
    }

    if (terminal_available()) {
        return pick_with_arrows(base_dir, entries, count, truncated, filter, selected_path, capacity);
    }

    if (directories) printf("\nDanh sách thư mục đích khả dụng:\n");
    else printf("\nDanh sách %s trong '%s':\n",
                filter == DIRECTORY_FILTER_ALL ? "đường dẫn" : "tệp thường", base_dir);
    printf(" --------------------------------------------------\n");
    if (directories) printf("  [1] %s (Thư mục hiện tại)\n", base_dir);
    for (size_t i = 0; i < count; i++) {
        if (directories) printf("  [%zu] %s/\n", i + 2, entries[i].name);
        else printf("  [%zu] [%-4s] %-30s (%.2f KiB)\n", i + 1,
                    entry_type(&entries[i].info), entries[i].name,
                    (double)entries[i].info.st_size / 1024);
    }
    if (!directories && count == 0) printf("  (Không tìm thấy mục phù hợp trong thư mục này)\n");
    if (truncated) {
        if (directories) {
            printf("  (Danh sách giới hạn %d thư mục con; dùng [0] để nhập đường dẫn khác)\n",
                   FM_MAX_ITEMS - 1);
        } else {
            printf("  (Danh sách giới hạn %d mục; dùng [0] để nhập đường dẫn khác)\n", FM_MAX_ITEMS);
        }
    }
    fputs(directories ? "  [0] Tự nhập đường dẫn thư mục đích bằng tay\n" :
                       "  [0] Tự nhập đường dẫn khác bằng tay\n", stdout);
    printf(" --------------------------------------------------\n");

    size_t maximum = count + (size_t)directories;
    char prompt[80];
    snprintf(prompt, sizeof(prompt), directories ? "Chọn thư mục đích theo số [0-%zu]: " :
                                                  "Chọn đường dẫn theo số [0-%zu]: ", maximum);
    char buffer[64];
    for (;;) {
        int result = ui_read_line(prompt, buffer, sizeof(buffer));
        if (result == 0) return 0;
        if (ferror(stdin)) return -1;
        if (result < 0 || buffer[0] == '\0') continue;

        int choice;
        if (!parse_choice(buffer, (long)maximum, &choice)) {
            printf(">> Số chọn không hợp lệ! Vui lòng nhập lại.\n");
            continue;
        }
        return accept_path_choice(base_dir, entries, filter, choice, selected_path, capacity);
    }
}

int ui_pick_path(const char *base_dir, char *selected_path, size_t capacity) {
    return pick_path(base_dir, DIRECTORY_FILTER_ALL, selected_path, capacity);
}

int ui_pick_file(const char *target_dir, char *selected_path, size_t capacity) {
    return pick_path(target_dir, DIRECTORY_FILTER_FILES, selected_path, capacity);
}

int ui_pick_directory(const char *base_dir, char *selected_dir, size_t capacity) {
    return pick_path(base_dir, DIRECTORY_FILTER_DIRECTORIES, selected_dir, capacity);
}

