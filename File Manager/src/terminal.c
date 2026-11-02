#include "terminal.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

static volatile sig_atomic_t pending_signal;
static volatile sig_atomic_t resized;

static void on_signal(int signal_number) {
    if (signal_number == SIGWINCH) resized = 1;
    else pending_signal = signal_number;
}

int terminal_available(void) {
    const char *term = getenv("TERM");
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) &&
           (term == NULL || strcmp(term, "dumb") != 0);
}

/* Đọc từng byte để không giữ phím của ô nhập tiếp theo trong bộ đệm stdio. */
static int read_byte(unsigned char *byte, int timeout) {
    struct pollfd input = {STDIN_FILENO, POLLIN, 0};
    int ready = poll(&input, 1, timeout);
    if (ready <= 0) return ready;
    ssize_t length = read(STDIN_FILENO, byte, 1);
    if (length == 0) return -2;
    return length < 0 ? -1 : 1;
}

enum { KEY_NONE = 256, KEY_UP, KEY_DOWN, KEY_HOME, KEY_END, KEY_PAGE_UP,
       KEY_PAGE_DOWN, KEY_CANCEL, KEY_ERROR };

static int read_key(void) {
    unsigned char byte;
    int result = read_byte(&byte, 100);
    if (result == -2) return KEY_CANCEL;
    if (result < 0) return errno == EINTR ? KEY_NONE : KEY_ERROR;
    if (result == 0) return KEY_NONE;
    if (byte == 3 || byte == 26) {
        pending_signal = byte == 3 ? SIGINT : SIGTSTP;
        return KEY_NONE;
    }
    if (byte == 4) return KEY_CANCEL;
    if (byte != 27) return byte;

    result = read_byte(&byte, 100);
    if (result <= 0) return pending_signal ? KEY_NONE : KEY_CANCEL;
    if (byte != '[' && byte != 'O') return KEY_NONE;
    char sequence[24];
    size_t length = 0;
    while (length < sizeof(sequence) - 1) {
        result = read_byte(&byte, 100);
        if (result <= 0) return KEY_NONE;
        sequence[length++] = (char)byte;
        if (byte >= '@' && byte <= '~') break;
    }
    sequence[length] = '\0';
    if (length == 0) return KEY_NONE;
    switch (sequence[length - 1]) {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'H': return KEY_HOME;
        case 'F': return KEY_END;
        default: break;
    }
    if (strcmp(sequence, "5~") == 0) return KEY_PAGE_UP;
    if (strcmp(sequence, "6~") == 0) return KEY_PAGE_DOWN;
    if (strcmp(sequence, "1~") == 0 || strcmp(sequence, "7~") == 0) return KEY_HOME;
    if (strcmp(sequence, "4~") == 0 || strcmp(sequence, "8~") == 0) return KEY_END;
    return KEY_NONE;
}

/* Cắt theo độ rộng hiển thị UTF-8; không cho ký tự điều khiển phá bố cục. */
static void print_text(const char *text, int columns) {
    mbstate_t state = {0};
    while (*text != '\0' && columns > 0) {
        wchar_t character;
        size_t length = mbrtowc(&character, text, MB_CUR_MAX, &state);
        if (length == (size_t)-1 || length == (size_t)-2) {
            state = (mbstate_t){0};
            putchar('?');
            text++;
            columns--;
            continue;
        }
        if (length == 0) break;
        int width = wcwidth(character);
        if (width < 0) {
            putchar('?');
            width = 1;
        } else {
            if (width > columns) break;
            fwrite(text, 1, length, stdout);
        }
        columns -= width;
        text += length;
    }
}

static size_t draw_menu(const char *title, const TerminalOption *options,
                        size_t count, size_t selected) {
    struct winsize window = {0};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    int rows = window.ws_row >= 5 ? window.ws_row : 24;
    int columns = window.ws_col >= 8 ? window.ws_col : 80;
    size_t visible = (size_t)(rows - 4);
    size_t start = selected / visible * visible;
    size_t end = start + visible < count ? start + visible : count;
    fputs("\033[H\033[2J\033[1m", stdout);
    print_text(title, columns - 1);
    fputs("\033[0m\n", stdout);
    print_text("↑/↓: chọn | Enter: xác nhận | Esc: quay lại | Home/End, PgUp/PgDn", columns - 1);
    putchar('\n');
    for (size_t i = start; i < end; i++) {
        if (i == selected) fputs("\033[7m > ", stdout);
        else fputs("   ", stdout);
        print_text(options[i].label, columns - 4);
        fputs("\033[0m\033[K\n", stdout);
    }
    printf("\033[%d;1H", rows);
    char status[120];
    snprintf(status, sizeof(status), "Mục %zu/%zu | Đang hiển thị %zu-%zu | Có thể nhập số rồi Enter",
             selected + 1, count, start + 1, end);
    print_text(status, columns - 1);
    fflush(stdout);
    return visible;
}

int terminal_select(const char *title, const TerminalOption *options, size_t count,
                    size_t initial, int *value) {
    if (title == NULL || options == NULL || count == 0 || initial >= count || value == NULL) {
        errno = EINVAL;
        return -1;
    }
retry:
    ;
    struct termios original;
    if (tcgetattr(STDIN_FILENO, &original) != 0) return -1;
    /* Lần đầu được gọi trước mọi lần đọc stdin trong ứng dụng. */
    static int prepared;
    if (!prepared) {
        setvbuf(stdin, NULL, _IONBF, 0);
        setlocale(LC_CTYPE, "");
        if (MB_CUR_MAX == 1) setlocale(LC_CTYPE, "C.UTF-8");
        prepared = 1;
    }

    const int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGTSTP, SIGWINCH};
    struct sigaction previous[sizeof(signals) / sizeof(signals[0])];
    struct sigaction action = {0};
    sigemptyset(&action.sa_mask);
    action.sa_handler = on_signal;
    size_t installed = 0;
    pending_signal = 0;
    resized = 0;
    for (; installed < sizeof(signals) / sizeof(signals[0]); installed++) {
        if (sigaction(signals[installed], &action, &previous[installed]) != 0) break;
    }
    int result = -1;
    int error = errno;
    int active = 0;
    size_t selected = initial;
    int multi_digit_options = 0;
    for (size_t i = 0; i < count; i++) {
        if (options[i].value >= 10) multi_digit_options = 1;
    }
    if (installed == sizeof(signals) / sizeof(signals[0])) {
        struct termios input = original;
        input.c_lflag &= (tcflag_t)~(ICANON | ECHO | IEXTEN);
        input.c_iflag &= (tcflag_t)~(IXON | IXOFF | ICRNL | INLCR | IGNCR);
        input.c_cc[VMIN] = 1;
        input.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &input) == 0) {
            active = 1;
            fputs("\033[?1049h\033[?25l", stdout);
            size_t page = draw_menu(title, options, count, selected);
            int typed = -1;
            while (!pending_signal) {
                if (resized) {
                    resized = 0;
                    page = draw_menu(title, options, count, selected);
                }
                int key = read_key();
                if (key == KEY_NONE) continue;
                if (key == KEY_ERROR) { error = errno; break; }
                if (key == KEY_CANCEL) { result = 0; break; }
                if (key == '\r' || key == '\n') {
                    if (typed >= 0 && options[selected].value != typed) {
                        putchar('\a');
                        fflush(stdout);
                        continue;
                    }
                    *value = options[selected].value;
                    result = 1;
                    break;
                }
                if (key >= '0' && key <= '9') {
                    /* Menu một chữ số: phím mới thay lựa chọn trước đó. */
                    if (!multi_digit_options || typed < 0) typed = key - '0';
                    else if (typed > (INT_MAX - 9) / 10) typed = INT_MAX;
                    else typed = typed * 10 + key - '0';
                    for (size_t i = 0; i < count; i++) {
                        if (options[i].value == typed) selected = i;
                    }
                } else {
                    typed = -1;
                    if (key == KEY_UP) selected = selected == 0 ? count - 1 : selected - 1;
                    else if (key == KEY_DOWN) selected = (selected + 1) % count;
                    else if (key == KEY_HOME) selected = 0;
                    else if (key == KEY_END) selected = count - 1;
                    else if (key == KEY_PAGE_UP) selected = selected > page ? selected - page : 0;
                    else if (key == KEY_PAGE_DOWN) selected = selected + page < count ? selected + page : count - 1;
                    else continue;
                }
                page = draw_menu(title, options, count, selected);
            }
        } else error = errno;
    }

    if (active) {
        fputs("\033[0m\033[?25h\033[?1049l", stdout);
        fflush(stdout);
        int restored;
        do { restored = tcsetattr(STDIN_FILENO, TCSANOW, &original); }
        while (restored != 0 && errno == EINTR);
        if (restored != 0) { error = errno; result = -1; }
    }
    int caught = pending_signal;
    while (installed > 0) {
        installed--;
        sigaction(signals[installed], &previous[installed], NULL);
    }
    if (caught) {
        raise(caught);
        if (caught == SIGTSTP) {
            initial = selected;
            goto retry;
        }
        error = EINTR;
        result = -1;
    }
    errno = error;
    return result;
}
