# Kế hoạch hoàn thiện chương trình xem thuộc tính tệp và thư mục

## 1. Mục tiêu và hiện trạng

Trong [src/main.c](src/main.c), chức năng xem thông tin hiện mới hiển thị đường dẫn, kích thước và thời gian sửa đổi. Chương trình đã có các tiện ích quản lý tệp nhưng phần thuộc tính thư mục còn thiếu.

Mục tiêu tiếp theo là hoàn thiện chức năng chính theo đề tài trên **Linux/WSL**, đồng thời giữ các tiện ích hiện có làm phần mở rộng.

## 2. Các bước hoàn thiện chức năng

Thực hiện việc chia module theo mục 5 trước, sau đó hoàn thiện các chức năng bên dưới trên cấu trúc mã nguồn mới.

### Bước 1 — Ổn định việc biên dịch

- Giữ tên chương trình `file_manager`; cập nhật Makefile để dùng C11, bật cảnh báo và khai báo các API POSIX cần thiết.
- Thay `strcasestr` bằng hàm tìm chuỗi không phân biệt hoa/thường tự viết, tránh phụ thuộc GNU. GCC hiện có trên Windows đang báo lỗi tại lời gọi này.
- Kiểm tra biên dịch bằng GCC/MSYS2 trên Windows sau mỗi bước; người dùng sẽ xác nhận bản dựng Linux sau.

### Bước 2 — Cho phép chọn cả tệp và thư mục

- Đổi trạng thái “tệp đang chọn” thành “đường dẫn đang chọn”.
- Menu số 1 liệt kê tệp, thư mục và liên kết tượng trưng, kèm nhãn phân loại; vẫn cho nhập đường dẫn tuyệt đối hoặc tương đối.
- Dùng thư mục hiện tại làm điểm bắt đầu, tránh phụ thuộc vào `test_files`.
- Kiểm tra EOF, lựa chọn sai, đầu vào quá dài và lỗi ghép đường dẫn; báo lỗi thay vì âm thầm cắt ngắn.

### Bước 3 — Hoàn thiện thuộc tính chuẩn

Tách việc lấy dữ liệu khỏi việc trình bày, sử dụng `struct stat` và `lstat()`. Với liên kết tượng trưng, hiển thị thuộc tính của chính liên kết và đường dẫn đích. [Tài liệu Linux về `lstat`](https://man7.org/linux/man-pages/man2/stat.2.html).

| Nhóm | Thông tin hiển thị |
|---|---|
| Nhận diện | Đường dẫn, loại tệp/thư mục/liên kết |
| Kích thước | Số byte và giá trị quy đổi KiB |
| Quyền | Dạng `rwxr-xr-x` và dạng bát phân |
| Sở hữu | UID, GID, tên người dùng và nhóm; dùng ID khi không tra được tên |
| Hệ thống tệp | Inode và số liên kết cứng |
| Thời gian | Truy cập, sửa nội dung, thay đổi trạng thái |

Ghi rõ `ctime` là thời gian thay đổi trạng thái; không hiển thị thành ngày tạo. Kích thước thư mục là giá trị hệ thống trả về, không cộng dung lượng bên trong. [Tài liệu Linux về thuộc tính inode](https://man7.org/linux/man-pages/man7/inode.7.html).

### Bước 4 — Tích hợp với menu và tiện ích

- Menu số 2 chỉ hiển thị thuộc tính; chuyển phần xem nội dung TXT/PDF/DOCX sang mục số 8.
- Giữ cây thư mục, tìm kiếm, sao chép, di chuyển và xóa; các thao tác quản lý tệp chỉ chấp nhận tệp thường.
- Cây thư mục và tìm kiếm không duyệt theo liên kết tượng trưng, tránh vòng lặp.
- Kiểm tra lỗi đọc/ghi khi sao chép, chặn nguồn và đích cùng tệp, yêu cầu xác nhận trước khi ghi đè.
- Phần xem tài liệu truyền đối số trực tiếp qua tiến trình con, báo rõ khi thiếu công cụ hoặc xử lý thất bại.

## 3. Kiểm thử và tiêu chí hoàn thành

- `make clean` và `make` chạy thành công trên Linux, không có cảnh báo.
- Đối chiếu kết quả với `stat` và `ls -ld`: loại, kích thước, quyền, UID/GID, inode, số liên kết và thời gian.
- Thử tệp rỗng, tệp lớn, thư mục rỗng, tên tiếng Việt, tên chứa khoảng trắng, đường dẫn tương đối và tuyệt đối.
- Thử liên kết hợp lệ, liên kết hỏng và liên kết tạo vòng lặp.
- Thử đường dẫn không tồn tại, thư mục cha thiếu quyền truy cập, nhập sai và EOF; chương trình báo lỗi rồi tiếp tục hoạt động.
- Kiểm thử thao tác sao chép/di chuyển/xóa trong thư mục tạm; xác nhận thiếu công cụ PDF/DOCX vẫn xem được thuộc tính.

## 4. Giả định và bàn giao

- Tiếp tục dùng menu terminal tiếng Việt; chia mã nguồn theo chức năng trong `src/` và `include/` như mục 5 để dễ phát triển, kiểm thử và giải thích khi bảo vệ.
- Chỉ thực hiện thuộc tính chuẩn theo lựa chọn đã chốt; không bổ sung thống kê dung lượng đệ quy.
- Môi trường phát triển hiện tại là Windows; trợ lý thực hiện các kiểm tra phù hợp bằng GCC/MinGW và MSYS2, đồng thời viết mã theo yêu cầu Linux/POSIX.
- Người dùng phụ trách chạy và kiểm thử trên Linux sau. Việc kiểm thử Linux được thực hiện sau và không chặn các bước phát triển tiếp theo trên Windows.
- Tạo bộ dữ liệu demo mới và README hướng dẫn biên dịch, sử dụng, ý nghĩa thuộc tính và các tình huống trình diễn.

## 5. Kế hoạch chia module và tổ chức thư mục dự án C

### 5.1. Hướng tổ chức

Chia mã nguồn **theo chức năng**, dùng hai thư mục `src/` và `include/` phẳng để dễ tìm kiếm và trình bày khi bảo vệ.

`main.c` chỉ khởi tạo trạng thái và gọi ứng dụng. Thực hiện việc chia module trước, sau đó hoàn thiện các chức năng đã thống nhất ở mục 2.

### 5.2. Cây thư mục đề xuất

Thư mục gốc là `File Manager/` đang chứa `main.c`, `Makefile` và `Plans.md`.

```text
File Manager/
├── Makefile
├── README.md
├── Plans.md
├── .gitignore
│
├── include/                # Khai báo dùng giữa các module
│   ├── config.h
│   ├── app.h
│   ├── ui.h
│   ├── attributes.h
│   ├── directory.h
│   ├── file_ops.h
│   ├── preview.h
│   └── path_utils.h
│
├── src/                    # Phần cài đặt
│   ├── main.c
│   ├── app.c
│   ├── ui.c
│   ├── attributes.c
│   ├── directory.c
│   ├── file_ops.c
│   ├── preview.c
│   └── path_utils.c
│
├── tests/
│   └── test_cli.sh         # Kiểm thử chương trình qua terminal
│
├── test_files/             # Dữ liệu mẫu để trình diễn
│   ├── sample.txt
│   ├── demo_text.txt
│   └── demo_folder/
│       └── nested.txt
│
├── build/                  # File .o và .d được sinh khi biên dịch
└── file_manager            # Chương trình thực thi được sinh
```

Các tài liệu PDF/DOCX hợp lệ có thể bổ sung vào dữ liệu demo. Kiểm thử tự động tạo tệp, liên kết và thư mục tạm riêng.

### 5.3. Trách nhiệm của từng module

| Module | Trách nhiệm |
|---|---|
| `main.c` | Tạo `AppContext`, gọi `app_run()` và trả mã thoát. |
| `app` | Chạy vòng lặp menu, điều phối chức năng, quản lý đường dẫn đang chọn và cập nhật trạng thái sau di chuyển/xóa. |
| `ui` | Hiển thị menu, nhập liệu, xác nhận thao tác và bảng chọn đường dẫn. |
| `attributes` | Đọc thuộc tính bằng `lstat()`, định dạng quyền/thời gian/chủ sở hữu và xuất bảng thuộc tính bằng các hàm riêng. |
| `directory` | Liệt kê mục trong thư mục, in cây và tìm kiếm đệ quy; không duyệt theo liên kết tượng trưng. |
| `file_ops` | Sao chép, di chuyển/đổi tên và xóa tệp; kiểm tra điều kiện và lỗi đọc/ghi. |
| `preview` | Xem trước TXT/PDF/DOCX, chạy công cụ ngoài bằng tiến trình con và giới hạn nội dung hiển thị. |
| `path_utils` | Ghép đường dẫn có kiểm tra độ dài, lấy tên cuối đường dẫn và các thao tác đường dẫn dùng chung. |
| `config.h` | Chứa các giới hạn dùng chung, đặt tên có tiền tố `FM_`, như độ dài đường dẫn và số dòng xem trước. |

Các quy tắc giao tiếp giữa module:

- Thay `g_selected_file` bằng `AppContext` trong `app.h`, chứa `selected_path`; chuỗi rỗng biểu thị chưa chọn. Chỉ tầng điều phối quản lý trạng thái này.
- `ui` lấy danh sách qua API của `directory`; không tự viết thêm một vòng duyệt thư mục.
- `attributes` dùng `struct stat`, với hàm đọc dữ liệu và hàm xuất kết quả tách biệt.
- `file_ops` nhận đường dẫn và lựa chọn cho phép ghi đè qua tham số; không đọc bàn phím. `app` kết hợp xác nhận từ `ui` với thao tác tương ứng.
- Các hàm xử lý hệ thống trả trạng thái thành công/thất bại và giữ thông tin lỗi để bên gọi báo lỗi.
- Hàm chỉ dùng trong một file phải khai báo `static`. Mỗi header có include guard; không `#include` file `.c` và không tạo phụ thuộc vòng.

Các hàm nhỏ như xóa ký tự xuống dòng, kiểm tra đuôi tệp và tìm chuỗi không phân biệt hoa/thường nằm riêng trong module sử dụng chúng, tránh tạo một `utils.c` chứa mọi thứ.

### 5.4. Trình tự chuyển đổi và hệ thống build

1. Thống nhất cấu trúc thư mục và trách nhiệm các module theo kế hoạch trong mục 5 này.
2. Tạo `src/`, `include/` và các header; cập nhật Makefile để hỗ trợ nhiều file nguồn.
3. Tách lần lượt phần đường dẫn, thuộc tính, duyệt thư mục, thao tác tệp và xem nội dung; biên dịch sau mỗi nhóm.
4. Tách nhập liệu sang `ui`, điều phối sang `app`, rồi chuyển điểm khởi chạy vào `src/main.c`.
5. Hoàn thiện thuộc tính chuẩn, menu số 8 và các xử lý lỗi theo kế hoạch chức năng ở mục 2.
6. Bổ sung kiểm thử, dữ liệu demo và README mô tả kiến trúc cùng cách chạy.

Makefile sẽ:

- Biên dịch `src/*.c` thành các file đối tượng trong `build/`, dùng `-Iinclude`.
- Dùng C11, các khai báo POSIX cần thiết và cảnh báo `-Wall -Wextra -Wpedantic`.
- Sinh phụ thuộc header bằng `-MMD -MP`.
- Giữ các lệnh `make`, `make clean`, bổ sung `make test`; giữ tên chương trình `file_manager`.

`.gitignore` loại trừ `build/` và chương trình thực thi. `make clean` chỉ xóa sản phẩm biên dịch.

### 5.5. Kiểm thử và tiêu chí hoàn thành sau khi chia module

- Chương trình liên kết thành công từ nhiều file nguồn, không có cảnh báo khi kiểm tra bằng GCC/MSYS2; người dùng xác nhận thêm trên Linux/WSL sau.
- Sửa một header sẽ biên dịch lại các module phụ thuộc.
- `main.c` chỉ còn phần khởi chạy; không chứa menu, nhập liệu, duyệt thư mục hoặc thao tác tệp.
- Kiểm thử qua terminal xác nhận việc chọn đường dẫn và cập nhật trạng thái sau di chuyển/xóa.
- Đối chiếu thuộc tính với `stat`; kiểm tra thư mục, liên kết hỏng và vòng liên kết.
- Kiểm tra sao chép cùng tệp, xác nhận ghi đè, EOF và thiếu công cụ xem tài liệu trong thư mục tạm.
- Giữ Linux/WSL, menu tiếng Việt và phạm vi thuộc tính chuẩn đã thống nhất.

## 6. Tiến độ triển khai

### Bước đầu — Tạo khung thư mục và cập nhật Makefile

- [x] Tạo `src/` và `include/`; ban đầu dùng `.gitkeep`, sau đó thay bằng header thật khi tách module.
- [x] Chuyển mã hiện tại từ `main.c` sang `src/main.c`.
- [x] Cập nhật Makefile để biên dịch `src/*.c`, lưu file `.o` và `.d` trong `build/`, dùng C11 và các khai báo POSIX.
- [x] Thêm `.gitignore` cho sản phẩm biên dịch.
- [x] Thay lời gọi `strcasestr` bằng hàm `contains_case_insensitive` dùng `strncasecmp` để khắc phục lỗi biên dịch hiện có.
- [x] Biên dịch sạch cảnh báo bằng GCC/MinGW và Make của MSYS2 với `make TARGET=file_manager.exe clean` rồi `make TARGET=file_manager.exe`; xác nhận lần gọi `make` tiếp theo không biên dịch lại.
- [x] Kiểm tra menu, tìm kiếm `.C` khớp `src/main.c`, thoát bằng lựa chọn `0` và xử lý EOF.
- [ ] Người dùng xác nhận `make clean` và `make` trên Linux/WSL sau; trợ lý tiếp tục các bước phát triển và kiểm tra trên Windows theo thỏa thuận ở mục 4.

### Bước 2 — Tách module xử lý đường dẫn

- [x] Tạo `include/path_utils.h` có include guard và `src/path_utils.c`.
- [x] Bổ sung `path_join()` trả trạng thái và `errno`, không để bên gọi dùng kết quả bị cắt ngắn; `path_basename()` lấy tên cuối theo dấu `/` của POSIX.
- [x] Dùng module mới trong bảng chọn file/thư mục, cây thư mục, tìm kiếm, sao chép và di chuyển; báo lỗi khi ghép đường dẫn thất bại.
- [x] Giới hạn đường dẫn đích của sao chép/di chuyển theo bộ đệm đường dẫn đang chọn, tránh cắt ngắn trạng thái sau di chuyển.
- [x] Biên dịch và liên kết hai file nguồn bằng GCC/MinGW và Make của MSYS2, không có cảnh báo.
- [x] Kiểm tra kết quả vừa khít bộ đệm, thiếu chỗ chứa kết quả, đầu vào không hợp lệ, chuỗi rỗng và dấu `/` cuối.
- [x] Kiểm tra qua menu: chọn file, sao chép với tên mặc định, di chuyển/đổi tên, cập nhật file đang chọn, cây thư mục và tìm kiếm không phân biệt hoa/thường; dữ liệu kiểm tra dùng tên chứa khoảng trắng và thư mục có dấu `/` cuối.
- [x] Xác nhận file sao chép và di chuyển giữ nguyên nội dung; Makefile theo dõi `path_utils.h` cho cả hai file nguồn.

### Bước 3 — Tách module thuộc tính

- [x] Tạo `include/attributes.h` có include guard và `src/attributes.c`.
- [x] Tách `attributes_read()` để đọc vào `struct stat`, trả trạng thái và giữ thông tin lỗi qua `errno` cho bên gọi.
- [x] Tách `attributes_print()` để xuất đường dẫn, kích thước và thời gian sửa đổi ra `FILE *`; định dạng kích thước bằng `intmax_t` và kiểm tra lỗi thời gian/ghi dữ liệu.
- [x] Cập nhật `show_file_info()` trong `main.c` để gọi module mới; phần chọn đường dẫn và xem nội dung sẽ được tách ở các bước tương ứng.
- [x] Giữ `stat()` và ba thuộc tính hiện có ở bước tách module này. Việc chuyển sang `lstat()` và bổ sung quyền, chủ sở hữu, inode, các mốc thời gian thuộc giai đoạn hoàn thiện chức năng sau khi chia module.
- [x] Biên dịch sạch cảnh báo bằng GCC/MinGW và Make của MSYS2, liên kết đủ ba file nguồn hiện có.
- [x] Đối chiếu đầu ra file văn bản trước/sau khi tách module: khớp hoàn toàn; kiểm tra file đang chọn, file rỗng, thư mục và đường dẫn không tồn tại đều đạt.
- [x] Xác nhận Makefile theo dõi `attributes.h` cho `main.c` và `attributes.c`, không yêu cầu biên dịch lại `path_utils.c` khi header này thay đổi.

### Bước 4 — Tách module thư mục

- [x] Tạo `include/directory.h` có include guard và `src/directory.c`.
- [x] Tạo `include/config.h`, chuyển các giới hạn đường dẫn, số mục chọn, số dòng xem trước và độ sâu cây sang các hằng có tiền tố `FM_` dùng chung.
- [x] Bổ sung `DirectoryEntry`, `DirectoryFilter` và `directory_list()` để cung cấp danh sách có lọc, số mục đã đọc và trạng thái còn mục ngoài giới hạn; phần giao diện vẫn phụ trách hiển thị và nhập lựa chọn.
- [x] Tách `directory_print_tree()` và `directory_search()` để xuất kết quả ra `FILE *`, trả trạng thái và bảo toàn `errno` khi đóng thư mục sau lỗi.
- [x] Chuyển các thao tác `opendir`/`readdir`/`closedir` và hàm tìm tên không phân biệt hoa/thường khỏi `main.c`; bảng chọn dùng API của module mới.
- [x] Giữ giới hạn độ sâu cây hiện có; tìm kiếm duyệt toàn bộ cây và không bị giới hạn bởi sức chứa bảng chọn. Bảng chọn báo khi vượt 100 mục, vẫn cho nhập đường dẫn bằng tay.
- [x] Báo lỗi khi mở/đọc thư mục, lấy thuộc tính hoặc xuất kết quả thất bại; xử lý EOF ở các câu hỏi về cây thư mục và tìm kiếm.
- [x] Bước tách module này giữ cách dùng `stat()` hiện tại. Chuyển sang đọc thuộc tính liên kết và tránh duyệt theo liên kết tượng trưng thuộc giai đoạn hoàn thiện chức năng sau khi chia module.
- [x] Biên dịch sạch cảnh báo bằng GCC/MinGW và Make của MSYS2, liên kết đủ bốn file nguồn hiện có; xác nhận phụ thuộc `directory.h` và `config.h`.
- [x] Đối chiếu đầu ra cây thư mục, tìm kiếm và chọn file theo số trước/sau khi tách: khớp hoàn toàn.
- [x] Kiểm tra thư mục rỗng, đường dẫn không tồn tại, EOF, chọn thư mục theo số, sao chép/di chuyển giữ nguyên nội dung và cập nhật trạng thái file đang chọn.
- [x] Kiểm tra danh sách hơn 100 file/thư mục, nhập tay ngoài danh sách, tìm đủ 106 file và tìm ở độ sâu lớn hơn giới hạn hiển thị cây.

### Bước 5 — Tách module thao tác tệp

- [x] Tạo `include/file_ops.h` có include guard và `src/file_ops.c`, cung cấp `file_ops_copy()`, `file_ops_move()` và `file_ops_delete()`; các hàm trả trạng thái/`errno` và không đọc bàn phím hoặc in giao diện.
- [x] Kiểm tra nguồn và đích là tệp thường; từ chối thư mục và liên kết tượng trưng/reparse point. Nhánh Linux dùng `lstat()`, nhánh Windows kiểm tra thuộc tính reparse point để phục vụ môi trường phát triển hiện tại.
- [x] Chặn nguồn và đích cùng tệp, kể cả đường dẫn khác nhau hoặc hard link; dùng device/inode trên POSIX và [định danh qua handle trên Windows](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileinformationbyhandle).
- [x] Đích tồn tại mặc định trả `EEXIST`; giao diện hỏi xác nhận và chỉ gọi lại với quyền ghi đè khi nhận đúng `y` hoặc `Y`.
- [x] Mở được nguồn trước khi mở đích sao chép; tạo đích mới bằng chế độ độc quyền, kiểm tra lỗi đọc/ghi/đóng tệp, đóng đủ tài nguyên và dọn đích vừa tạo khi lỗi.
- [x] Ghi rõ giới hạn sao chép: nếu cho phép ghi đè tệp có sẵn rồi xảy ra lỗi I/O, đích có thể chứa dữ liệu chưa đầy đủ; thao tác vẫn trả lỗi. Di chuyển không sao chép dự phòng khi khác hệ thống tệp (`EXDEV`).
- [x] Giữ nhập liệu, xác nhận và thông báo trong `main.c`; chỉ cập nhật đường dẫn đang chọn khi di chuyển thành công, chỉ hủy chọn khi xóa thành công.
- [x] Xử lý EOF ở tên đích và câu hỏi xác nhận; câu trả lời quá dài bị từ chối và phần còn lại của dòng được bỏ qua.
- [x] Biên dịch sạch cảnh báo bằng GCC/MinGW và Make của MSYS2, liên kết đủ năm file nguồn hiện có; xác nhận phụ thuộc `file_ops.h` chỉ tác động tới các module liên quan.
- [x] Kiểm tra sao chép nhị phân nhiều khối, cùng tệp/cùng hard link, ghi đè được phép/bị từ chối, di chuyển, xóa, nguồn thiếu, thư mục và reparse point.
- [x] Kiểm tra lỗi mở nguồn/đích: đích giữ nguyên khi nguồn không mở được và nguồn không bị giữ mở sau khi thao tác thất bại.
- [x] Giả lập lỗi đọc, ghi và đóng tệp trên bản kiểm tra riêng: nhận đúng lỗi, đóng đủ hai stream, dọn đích mới và giữ nguyên nguồn; với đích có sẵn, không báo thành công khi xảy ra lỗi.
- [x] Kiểm tra qua menu việc xác nhận/hủy ghi đè, EOF, xác nhận quá dài và trạng thái file đang chọn sau khi hủy hoặc hoàn thành di chuyển/xóa.

### Bước 6 — Tách module xem trước nội dung

- [x] Tạo `include/preview.h` có include guard và `src/preview.c`, cung cấp `preview_show(FILE *, const char *)`, trả trạng thái/`errno` và không đọc bàn phím.
- [x] Chuyển phần kiểm tra đuôi tệp, đọc văn bản, metadata và trích xuất PDF/DOCX khỏi `main.c`; phần điều phối gọi `preview_show()` sau khi in thuộc tính.
- [x] Loại bỏ `popen()` và chuỗi lệnh shell. Linux dùng `posix_spawnp()` với mảng đối số và pipe; Windows dùng `CreateProcessA()` cùng [quy tắc quote đối số của Windows CRT](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?view=msvc-170).
- [x] Truyền đường dẫn tuyệt đối cho công cụ để tên tệp bắt đầu bằng dấu `-` không bị hiểu thành tùy chọn. Công cụ metadata/PDF nhận stdin rỗng; `docx2txt - -` nhận dữ liệu tệp qua stdin và xuất stdout theo [giao diện docx2txt của Debian](https://manpages.debian.org/trixie/docx2txt/docx2txt.1.en.html).
- [x] Giới hạn nội dung theo số dòng thực, giữ nguyên dòng dài; chỉ báo thu gọn khi có dữ liệu vượt giới hạn. Đọc hết đầu ra công cụ trước khi lấy mã thoát, đồng thời giới hạn phần hiển thị để tránh kẹt pipe.
- [x] Thu stderr của công cụ cùng đầu ra, báo rõ công cụ thiếu hoặc mã thoát lỗi, đóng pipe/handle và chờ tiến trình hoàn tất. Vẫn thử trích nội dung nếu công cụ metadata thất bại.
- [x] Biên dịch sạch cảnh báo bằng GCC/MinGW và Make của MSYS2, liên kết đủ sáu file nguồn; xác nhận `preview.h` chỉ tác động tới các file phụ thuộc.
- [x] Đối chiếu văn bản ngắn, tệp rỗng và đúng 15 dòng với bản trước khi tách: khớp hoàn toàn. Kiểm tra dòng dài 1500 ký tự vẫn được tính là một dòng; thư mục và đường dẫn thiếu trả lỗi.
- [x] Dùng công cụ giả lập trên Windows để kiểm tra argv với tên chứa khoảng trắng, `&`, `%`, `$`, dấu nháy đơn và đuôi viết hoa; stdin DOCX giữ nguyên 1024 byte nhị phân.
- [x] Kiểm tra 40000 dòng đầu ra từ tiến trình: chỉ hiển thị phần giới hạn, không kẹt pipe; vẫn nhận đúng mã thoát lỗi nằm sau phần đã thu gọn. Kiểm tra giới hạn riêng cho metadata và nội dung DOCX.
- [x] Kiểm tra thiếu công cụ, công cụ trả lỗi và chỉ có công cụ trích nội dung: phần thuộc tính và vòng lặp menu vẫn hoạt động.

Ở bước này menu số 2 vẫn hiển thị cả thuộc tính và nội dung như trước. Việc tách nội dung sang menu số 8 thuộc giai đoạn hoàn thiện chức năng sau khi chia module. Máy chưa có các công cụ PDF/DOCX thật; các kiểm tra tiến trình ở trên dùng chương trình giả lập, còn việc chuyển đổi tài liệu thật trên Linux do người dùng thực hiện sau.

Bước tiếp theo là tách phần menu, nhập liệu, xác nhận và bảng chọn đường dẫn sang `ui.c` và `ui.h`. Sau đó tách điều phối/trạng thái sang `app`, để `main.c` chỉ còn khởi chạy.
