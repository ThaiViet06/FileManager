#include "preview.h"
#include "config.h"
#include "path_utils.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

static int io_error(void) {
    return errno != 0 ? errno : EIO;
}

/* Tiến trình phải được đọc hết đầu ra để không kẹt pipe hoặc nhận SIGPIPE. */
static int copy_preview(FILE *input, FILE *output, size_t max_lines, int drain) {
    size_t lines = 0;
    int error = 0;
    int truncated = 0;
    int last_printed = '\n';
    for (;;) {
        errno = 0;
        int ch = fgetc(input);
        if (ch == EOF) {
            if (ferror(input) && error == 0) error = io_error();
            break;
        }
        if (lines >= max_lines) {
            truncated = 1;
            if (!drain) break;
            continue;
        }
        if (error == 0) {
            errno = 0;
            if (fputc(ch, output) == EOF) error = io_error();
        }
        last_printed = ch;
        if (ch == '\n') lines++;
    }
    if (truncated && error == 0) {
        if (last_printed != '\n' && fputc('\n', output) == EOF) error = io_error();
        if (fputs("... [Đã thu gọn nội dung] ...\n", output) == EOF && error == 0) {
            error = io_error();
        }
    }
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

static int absolute_path(char *output, size_t size, const char *path) {
#ifdef _WIN32
    return _fullpath(output, path, size) != NULL ? 0 : -1;
#else
    if (path[0] == '/') {
        if (strlen(path) >= size) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strcpy(output, path);
        return 0;
    }
    char current_dir[FM_PATH_CAP * 2];
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) return -1;
    return path_join(output, size, current_dir, path);
#endif
}

#ifdef _WIN32
static int process_error(DWORD error) {
    switch (error) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND: errno = ENOENT; break;
        case ERROR_ACCESS_DENIED: errno = EACCES; break;
        case ERROR_BAD_EXE_FORMAT: errno = ENOEXEC; break;
        case ERROR_NOT_ENOUGH_MEMORY: errno = ENOMEM; break;
        default: errno = EIO; break;
    }
    return -1;
}

static int append_chars(char *line, size_t capacity, size_t *length, char ch, size_t count) {
    if (count >= capacity - *length) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memset(line + *length, ch, count);
    *length += count;
    line[*length] = '\0';
    return 0;
}

/* Quote theo quy tắc argv của Windows CRT, không qua cmd.exe/PowerShell. */
static int command_line(char *line, size_t capacity, char *const arguments[]) {
    size_t length = 0;
    line[0] = '\0';
    for (size_t i = 0; arguments[i] != NULL; i++) {
        if (i > 0 && append_chars(line, capacity, &length, ' ', 1) != 0) return -1;
        if (append_chars(line, capacity, &length, '"', 1) != 0) return -1;
        const char *cursor = arguments[i];
        for (;;) {
            size_t backslashes = 0;
            while (*cursor == '\\') {
                backslashes++;
                cursor++;
            }
            if (*cursor == '\0' || *cursor == '"') {
                if (append_chars(line, capacity, &length, '\\',
                                 backslashes * 2 + (*cursor == '"')) != 0) return -1;
            } else if (append_chars(line, capacity, &length, '\\', backslashes) != 0) {
                return -1;
            }
            if (*cursor == '\0') break;
            if (append_chars(line, capacity, &length, *cursor++, 1) != 0) return -1;
        }
        if (append_chars(line, capacity, &length, '"', 1) != 0) return -1;
    }
    return 0;
}

static int run_tool(FILE *output, char *const arguments[], const char *input_path,
                    size_t max_lines, int *exit_code) {
    char line[FM_PATH_CAP * 8];
    if (command_line(line, sizeof(line), arguments) != 0) return -1;

    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE pipe_read = NULL, pipe_write = NULL, input = INVALID_HANDLE_VALUE;
    FILE *captured = NULL;
    if (!CreatePipe(&pipe_read, &pipe_write, &security, 0)) {
        return process_error(GetLastError());
    }
    if (!SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0)) {
        process_error(GetLastError());
        goto failed;
    }
    input = CreateFileA(input_path != NULL ? input_path : "NUL", GENERIC_READ,
                        FILE_SHARE_READ, &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (input == INVALID_HANDLE_VALUE) {
        process_error(GetLastError());
        goto failed;
    }
    int descriptor = _open_osfhandle((intptr_t)pipe_read, _O_RDONLY | _O_TEXT | _O_NOINHERIT);
    if (descriptor < 0) goto failed;
    pipe_read = NULL; /* descriptor sở hữu handle đọc từ đây. */
    captured = fdopen(descriptor, "r");
    if (captured == NULL) {
        int error = errno;
        _close(descriptor);
        errno = error;
        goto failed;
    }

    STARTUPINFOA startup = {0};
    PROCESS_INFORMATION process = {0};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = pipe_write;
    startup.hStdError = pipe_write;
    if (!CreateProcessA(NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &startup, &process)) {
        process_error(GetLastError());
        goto failed;
    }
    CloseHandle(process.hThread);
    CloseHandle(input);
    CloseHandle(pipe_write);

    int result = copy_preview(captured, output, max_lines, 1);
    int error = result != 0 ? io_error() : 0;
    if (fclose(captured) != 0 && error == 0) error = io_error();
    DWORD status;
    if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0 ||
        !GetExitCodeProcess(process.hProcess, &status)) {
        if (error == 0) error = EIO;
    } else {
        *exit_code = status <= INT_MAX ? (int)status : 128;
        if (status != 0 && error == 0) error = EIO;
    }
    CloseHandle(process.hProcess);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;

failed: {
        int error = io_error();
        if (captured != NULL) fclose(captured);
        if (pipe_read != NULL) CloseHandle(pipe_read);
        if (pipe_write != NULL) CloseHandle(pipe_write);
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        errno = error;
        return -1;
    }
}
#else
static int wait_tool(pid_t child, int *exit_code) {
    int status;
    pid_t result;
    do {
        result = waitpid(child, &status, 0);
    } while (result < 0 && errno == EINTR);
    if (result < 0) return -1;
    if (WIFEXITED(status)) *exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) *exit_code = 128 + WTERMSIG(status);
    else *exit_code = 128;
    if (*exit_code != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int run_tool(FILE *output, char *const arguments[], const char *input_path,
                    size_t max_lines, int *exit_code) {
    int descriptors[2];
    if (pipe(descriptors) != 0) return -1;
    int input = open(input_path != NULL ? input_path : "/dev/null", O_RDONLY);
    if (input < 0) {
        int error = errno;
        close(descriptors[0]);
        close(descriptors[1]);
        errno = error;
        return -1;
    }

    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    if (error == 0) {
        error = posix_spawn_file_actions_adddup2(&actions, input, STDIN_FILENO);
        if (error == 0) error = posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO);
        if (error == 0) error = posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDERR_FILENO);
        if (error == 0) error = posix_spawn_file_actions_addclose(&actions, input);
        if (error == 0) error = posix_spawn_file_actions_addclose(&actions, descriptors[0]);
        if (error == 0) error = posix_spawn_file_actions_addclose(&actions, descriptors[1]);
        pid_t child = -1;
        if (error == 0) error = posix_spawnp(&child, arguments[0], &actions, NULL, arguments, environ);
        posix_spawn_file_actions_destroy(&actions);
        close(input);
        close(descriptors[1]);
        if (error != 0) {
            close(descriptors[0]);
            errno = error;
            return -1;
        }

        FILE *captured = fdopen(descriptors[0], "r");
        if (captured == NULL) {
            error = errno;
            close(descriptors[0]);
            wait_tool(child, exit_code);
            errno = error;
            return -1;
        }
        int result = copy_preview(captured, output, max_lines, 1);
        error = result != 0 ? io_error() : 0;
        if (fclose(captured) != 0 && error == 0) error = io_error();
        if (wait_tool(child, exit_code) != 0 && error == 0) error = errno;
        if (error != 0) {
            errno = error;
            return -1;
        }
        return 0;
    }
    close(input);
    close(descriptors[0]);
    close(descriptors[1]);
    errno = error;
    return -1;
}
#endif

static int run_section(FILE *stream, const char *title, char *const arguments[],
                       const char *input_path, size_t max_lines) {
    if (fprintf(stream, "\n--- [%s] ---\n", title) < 0) return -1;
    int exit_code = -1;
    if (run_tool(stream, arguments, input_path, max_lines, &exit_code) == 0) return 0;

    int error = io_error();
    int result;
    if (exit_code > 0) {
        result = fprintf(stream, ">> Công cụ '%s' thất bại (mã thoát %d).\n", arguments[0], exit_code);
    } else if (error == ENOENT) {
        result = fprintf(stream, ">> Không tìm thấy công cụ '%s' hoặc tệp đầu vào; kiểm tra PATH.\n",
                         arguments[0]);
    } else {
        result = fprintf(stream, ">> Lỗi khi chạy '%s': %s.\n", arguments[0], strerror(error));
    }
    errno = result < 0 ? EIO : error;
    return -1;
}

static int preview_text(FILE *stream, const char *path) {
    FILE *input = fopen(path, "r");
    if (input == NULL) return -1;
    int error = 0;
    if (fprintf(stream, "\n--- [Xem trước nội dung (%d dòng đầu)] ---\n", FM_PREVIEW_LINES) < 0) {
        error = io_error();
    } else if (copy_preview(input, stream, FM_PREVIEW_LINES, 0) != 0) {
        error = io_error();
    }
    if (fclose(input) != 0 && error == 0) error = io_error();
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

static int has_extension(const char *path, const char *extension) {
    const char *name = path_basename(path);
    const char *dot = strrchr(name, '.');
    return dot != NULL && dot != name && strcasecmp(dot, extension) == 0;
}

int preview_show(FILE *stream, const char *path) {
    if (stream == NULL || path == NULL || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    struct stat info;
    if (stat(path, &info) != 0) return -1;
    if (!S_ISREG(info.st_mode)) {
        errno = S_ISDIR(info.st_mode) ? EISDIR : EINVAL;
        return -1;
    }

    int error = 0;
    if (has_extension(path, ".pdf") || has_extension(path, ".docx")) {
        char full_path[FM_PATH_CAP * 2];
        if (absolute_path(full_path, sizeof(full_path), path) != 0) return -1;
        if (has_extension(path, ".pdf")) {
            char *metadata[] = {"pdfinfo", full_path, NULL};
            char *content[] = {"pdftotext", full_path, "-", NULL};
            if (run_section(stream, "Metadata PDF", metadata, NULL, 15) != 0) error = io_error();
            if (run_section(stream, "Nội dung PDF trích xuất", content, NULL, FM_PREVIEW_LINES) != 0 && error == 0) {
                error = io_error();
            }
        } else {
            char *metadata[] = {"exiftool", "-Title", "-Creator", "-CreateDate", "-WordCount", full_path, NULL};
            char *content[] = {"docx2txt", "-", "-", NULL};
            if (run_section(stream, "Metadata DOCX", metadata, NULL, 10) != 0) error = io_error();
            if (run_section(stream, "Nội dung DOCX trích xuất", content, full_path, FM_PREVIEW_LINES) != 0 && error == 0) {
                error = io_error();
            }
        }
    } else if (preview_text(stream, path) != 0) {
        error = io_error();
    }
    if (fputs("\n==========================\n", stream) == EOF && error == 0) error = io_error();
    if (fflush(stream) != 0 && error == 0) error = io_error();
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}
