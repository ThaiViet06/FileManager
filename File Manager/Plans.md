# Tổng quan chức năng và hướng mở rộng File Manager

## 1. Dự án có bao nhiêu tùy chọn?

Menu hiện có **8 tùy chọn**, gồm **7 chức năng từ 1 đến 7** và **tùy chọn 0 để thoát**. Chương trình chạy trên terminal Linux.

Các mục trong menu và bảng chọn đã hỗ trợ **↑/↓ rồi Enter**, có tô sáng mục đang chọn. Esc hủy bảng chọn hoặc thoát ở menu chính. Đường dẫn, tên tệp và từ khóa vẫn nhập bằng văn bản. Khi chuyển hướng đầu vào/đầu ra hoặc dùng `TERM=dumb`, chương trình dùng menu nhập số.

Với menu có số tùy chọn từ 0 đến 9, mỗi lần nhấn số sẽ chuyển sang mục tương ứng: nhấn `2` rồi `5` sẽ chọn mục `5`, Enter xác nhận mục cuối. Bảng chọn có số từ 10 trở lên vẫn nhận nhiều chữ số để chọn các mục như `12` hoặc `100`.

| Tùy chọn | Chức năng | File xử lý chính | Hiện trạng |
|---|---|---|---|
| 1 | Chọn / đổi / hủy đường dẫn mặc định | [ui.c](src/ui.c), [directory.c](src/directory.c), [app.c](src/app.c) | Đã chọn được tệp, thư mục và liên kết; chưa có trình duyệt đi vào thư mục con |
| 2 | Xem thuộc tính và nội dung | [attributes.c](src/attributes.c), [preview.c](src/preview.c), [app.c](src/app.c) | Thuộc tính chuẩn đã hoàn thiện; PDF/DOCX cần kiểm thử với tài liệu và công cụ thật |
| 3 | Hiển thị cây thư mục | [directory.c](src/directory.c), [app.c](src/app.c) | Đã in cây có giới hạn độ sâu; chưa xử lý việc đi theo liên kết |
| 4 | Di chuyển / đổi tên tệp | [file_ops.c](src/file_ops.c), [app.c](src/app.c) | Đã có luồng thao tác trên tệp thường trong cùng hệ thống tệp |
| 5 | Sao chép tệp | [file_ops.c](src/file_ops.c), [app.c](src/app.c) | Đã sao chép tệp thường, xác nhận ghi đè và kiểm tra lỗi I/O |
| 6 | Xóa tệp | [file_ops.c](src/file_ops.c), [app.c](src/app.c) | Đã hoàn thiện phạm vi xóa tệp thường có xác nhận; chưa có khôi phục |
| 7 | Tìm kiếm theo tên / phần mở rộng | [directory.c](src/directory.c), [app.c](src/app.c) | Đã tìm đệ quy và đếm kết quả; chưa xử lý việc đi theo liên kết |
| 0 | Thoát | [app.c](src/app.c), [ui.c](src/ui.c), [terminal.c](src/terminal.c) | Đã thoát và khôi phục terminal; chưa lưu phiên làm việc |

Phần **đã làm / còn thiếu** bên dưới mô tả mã hiện tại. Phần **có thể mở rộng** là hướng phát triển chưa triển khai. Menu số 8 chưa tồn tại; tách xem nội dung thành mục 8 là một hướng mở rộng của tùy chọn 2.

## 2. Các file dùng chung giữa các tùy chọn

Một tùy chọn thường đi qua nhiều file: `app` điều phối, `ui` nhận thao tác, `terminal` điều khiển bàn phím và module nghiệp vụ thực hiện công việc.

| File | Vai trò |
|---|---|
| [src/main.c](src/main.c) | Khởi tạo `AppContext` và gọi ứng dụng |
| [src/app.c](src/app.c), [include/app.h](include/app.h) | Vòng lặp menu, điều phối các tùy chọn và quản lý `selected_path` |
| [src/ui.c](src/ui.c), [include/ui.h](include/ui.h) | Menu, bảng chọn, nhập văn bản, xác nhận và chờ xem kết quả |
| [src/terminal.c](src/terminal.c), [include/terminal.h](include/terminal.h) | Phím mũi tên/Enter, tô sáng, phân trang, đổi kích thước cửa sổ và khôi phục terminal |
| [src/path_utils.c](src/path_utils.c), [include/path_utils.h](include/path_utils.h) | Ghép đường dẫn có kiểm tra độ dài và lấy phần tên cuối đường dẫn |
| [include/config.h](include/config.h) | Giới hạn đường dẫn, số mục trong bảng chọn, số dòng xem trước và độ sâu cây |

## 3. Tùy chọn 1: Chọn / đổi / hủy đường dẫn mặc định

**File phụ trách:** [app.c](src/app.c) với `select_or_change_path()`; [ui.c](src/ui.c) với `ui_pick_path()` và `ui_selected_path_action()`; [directory.c](src/directory.c) với `directory_list()`; [app.h](include/app.h) lưu trạng thái. API liệt kê nằm trong [directory.h](include/directory.h).

**Chức năng chính đã làm:**

- Liệt kê các mục trong thư mục hiện tại, có nhãn tệp, thư mục, liên kết và loại khác.
- Dùng `lstat()` để chọn được cả liên kết hợp lệ, hỏng hoặc tự trỏ.
- Chọn bằng mũi tên/Enter hoặc nhập đường dẫn tương đối/tuyệt đối.
- Đổi hoặc hủy lựa chọn; giữ lựa chọn trước đó khi hủy, EOF hoặc chọn lỗi.
- Kiểm tra đường dẫn không tồn tại và đầu vào quá dài.

**Làm đến đâu / còn thiếu:** Đã đáp ứng việc chọn một đường dẫn. Bảng chọn hiển thị tối đa 100 mục, có phân trang và nhập tay để chọn ngoài danh sách. Chọn một thư mục hiện chỉ đặt thư mục đó làm đường dẫn mặc định; chưa mở thư mục đó để duyệt các mục con. Đường dẫn đang chọn được lưu trong bộ đệm 1024 byte, gồm ký tự kết thúc chuỗi.

**Có thể mở rộng:**

- Bổ sung duyệt vào thư mục con, quay về thư mục cha và hiển thị vị trí hiện tại.
- Sắp xếp theo tên/loại/kích thước, lọc nhanh khi gõ và hiển thị tệp ẩn theo lựa chọn.
- Thay giới hạn 100 mục bằng danh sách động; hỗ trợ chọn nhiều mục, đường dẫn yêu thích và lịch sử gần đây.

## 4. Tùy chọn 2: Xem thuộc tính và nội dung

**File phụ trách:** [app.c](src/app.c) với `show_path_info()`; [attributes.c](src/attributes.c) và [attributes.h](include/attributes.h) đọc/in thuộc tính; [preview.c](src/preview.c) và [preview.h](include/preview.h) xem nội dung.

**Chức năng chính đã làm:**

- Hiển thị loại đường dẫn, kích thước byte/KiB, quyền dạng ký tự và bát phân, gồm các bit quyền đặc biệt.
- Hiển thị UID/GID kèm tên nếu tra được, inode và số liên kết cứng.
- Hiển thị atime, mtime, ctime kèm độ lệch múi giờ; ghi rõ ctime không phải ngày tạo.
- Đọc thuộc tính của chính liên kết và chuỗi đích bằng `lstat()`/`readlink()`, kể cả liên kết hỏng; không cắt ngắn đích liên kết dài.
- Hiển thị kích thước thư mục do hệ thống trả về, không cộng dung lượng bên trong.
- Với tệp thường, xem trước tối đa 15 dòng. PDF dùng `pdfinfo`/`pdftotext`; DOCX dùng `exiftool`/`docx2txt`. Có thông báo khi công cụ thiếu hoặc trả lỗi.

**Làm đến đâu / còn thiếu:** Phần thuộc tính chuẩn đã hoàn thiện và được đối chiếu với dữ liệu hệ thống. Thuộc tính và nội dung vẫn chung menu số 2. Việc trích PDF/DOCX thật chưa được xác nhận bằng bộ tài liệu và công cụ thật. Tệp ngoài PDF/DOCX đang được đọc như văn bản, chưa có bước nhận diện tệp nhị phân.

**Có thể mở rộng:**

- Tách xem nội dung sang tùy chọn 8, để tùy chọn 2 tập trung vào thuộc tính.
- Thêm nhận diện định dạng, thông báo tệp nhị phân và kiểm thử PDF/DOCX thật.
- Cho xem nội dung theo trang, tìm trong nội dung; xuất bảng thuộc tính ra TXT/CSV/JSON.

## 5. Tùy chọn 3: Hiển thị cây thư mục

**File phụ trách:** [app.c](src/app.c) với `show_directory_tree()`; [directory.c](src/directory.c) và [directory.h](include/directory.h) với `directory_print_tree()`; [path_utils.c](src/path_utils.c) ghép đường dẫn; [config.h](include/config.h) đặt giới hạn độ sâu.

**Chức năng chính đã làm:**

- Nhận thư mục gốc; để trống để dùng thư mục hiện tại.
- Duyệt đệ quy và in cây có thụt lề, bỏ qua `.` và `..`.
- Giới hạn độ sâu bằng `FM_TREE_MAX_DEPTH`, hiện đặt là 4.
- Báo lỗi khi mở/đọc thư mục, lấy thông tin hoặc xuất kết quả thất bại.

**Làm đến đâu / còn thiếu:** Đã in được cây thông thường. Phần duyệt vẫn dùng `stat()`, nên còn đi theo liên kết tới thư mục; liên kết hỏng hoặc lỗi truy cập có thể làm dừng việc in cây. Đây là phần cần hoàn thiện tiếp.

**Có thể mở rộng:**

- Dùng `lstat()` để hiển thị liên kết nhưng không đi theo liên kết; tiếp tục với các nhánh khác khi một nhánh lỗi.
- Cho chọn thư mục gốc bằng bảng chọn, chọn độ sâu và lọc các loại mục cần hiển thị.
- Bổ sung sắp xếp, phân trang kết quả và mở/thu gọn nhánh bằng bàn phím.

## 6. Tùy chọn 4: Di chuyển / đổi tên tệp

**File phụ trách:** [app.c](src/app.c) với `move_file()`; [file_ops.c](src/file_ops.c) và [file_ops.h](include/file_ops.h) với `file_ops_move()`; [ui.c](src/ui.c) chọn đích/xác nhận; [path_utils.c](src/path_utils.c) lấy tên và ghép đường dẫn đích.

**Chức năng chính đã làm:**

- Dùng tệp đang chọn hoặc chọn tệp nguồn khác khi chưa có lựa chọn.
- Chọn thư mục đích và nhập tên mới; để trống tên để giữ tên nguồn.
- Chỉ nhận tệp thường; chặn nguồn/đích cùng tệp, kể cả hard link.
- Hỏi trước khi ghi đè; giao diện mũi tên mặc định Hủy.
- Chỉ cập nhật đường dẫn đang chọn sau khi di chuyển thành công; giữ nguyên trạng thái khi hủy hoặc lỗi.

**Làm đến đâu / còn thiếu:** Đã hoàn thành luồng di chuyển/đổi tên tệp thường trong cùng hệ thống tệp bằng `rename()` trên Linux. Khi khác hệ thống tệp, thao tác trả lỗi `EXDEV`; chưa có bước sao chép rồi xóa nguồn. Chưa hỗ trợ di chuyển thư mục, liên kết hoặc nhiều tệp cùng lúc.

**Có thể mở rộng:**

- Hỗ trợ khác hệ thống tệp bằng sao chép, xác nhận sao chép thành công rồi mới xóa nguồn.
- Thêm đổi tên hàng loạt và chọn nhiều tệp, hiển thị kết quả từng mục.
- Mở rộng xử lý thư mục/liên kết với quy tắc riêng; thêm theo dõi tiến độ và hủy tác vụ khi thao tác lớn.

## 7. Tùy chọn 5: Sao chép tệp

**File phụ trách:** [app.c](src/app.c) với `copy_file()`; [file_ops.c](src/file_ops.c) và [file_ops.h](include/file_ops.h) với `file_ops_copy()`; [ui.c](src/ui.c) chọn đích/xác nhận; [path_utils.c](src/path_utils.c) tạo đường dẫn đích.

**Chức năng chính đã làm:**

- Sao chép tệp thường theo từng khối byte, dùng được cho nội dung văn bản và nhị phân.
- Chọn thư mục đích, nhập tên mới; để trống tên để thêm tiền tố `copy_`.
- Chặn nguồn/đích cùng tệp hoặc hard link; hỏi trước khi ghi đè và mặc định Hủy.
- Mở nguồn trước khi mở đích, tạo đích mới độc quyền, kiểm tra lỗi đọc/ghi/đóng tệp.
- Dọn tệp đích vừa tạo nếu sao chép thất bại; giữ nguyên lựa chọn nguồn.

**Làm đến đâu / còn thiếu:** Đã hoàn thành luồng sao chép nội dung tệp thường. Khi đã cho phép ghi đè đích có sẵn, lỗi I/O có thể để lại đích chưa đầy đủ. Chưa có ghi qua tệp tạm để thay thế đích sau cùng, chưa sao chép quyền/thời gian của nguồn và chưa có tiến độ/hủy tác vụ.

**Có thể mở rộng:**

- Sao chép qua tệp tạm rồi thay thế đích sau khi hoàn tất để cải thiện xử lý ghi đè.
- Cho chọn giữ quyền/thời gian, kiểm tra nội dung sau sao chép và hiển thị tiến độ.
- Sao chép nhiều tệp hoặc cả thư mục; thêm tạm dừng/hủy và báo cáo kết quả từng mục.

## 8. Tùy chọn 6: Xóa tệp

**File phụ trách:** [app.c](src/app.c) với `delete_file()`; [file_ops.c](src/file_ops.c) và [file_ops.h](include/file_ops.h) với `file_ops_delete()`; [ui.c](src/ui.c) với `ui_confirm_delete()`.

**Chức năng chính đã làm:**

- Dùng tệp đang chọn hoặc chọn tệp cần xóa khi chưa có lựa chọn.
- Từ chối thư mục, liên kết và các loại không phải tệp thường.
- Yêu cầu xác nhận; giao diện mũi tên mặc định Hủy. Chế độ nhập dòng chỉ đồng ý khi nhận đúng `y` hoặc `Y`.
- Xử lý hủy/EOF/lỗi, báo kết quả và chỉ hủy lựa chọn sau khi xóa thành công.

**Làm đến đâu / còn thiếu:** Đã hoàn thiện phạm vi xóa một tệp thường có xác nhận. Hiện xóa trực tiếp bằng `unlink()`, chưa có thùng rác, hoàn tác hoặc xóa nhiều mục.

**Có thể mở rộng:**

- Chuyển tệp vào thùng rác và hỗ trợ khôi phục.
- Chọn nhiều tệp, xem danh sách trước khi thực hiện và báo kết quả từng tệp.
- Thêm lịch sử thao tác; hỗ trợ xóa thư mục rỗng như một chức năng riêng.

## 9. Tùy chọn 7: Tìm kiếm theo tên / phần mở rộng

**File phụ trách:** [app.c](src/app.c) với `search_file()`; [directory.c](src/directory.c) và [directory.h](include/directory.h) với `directory_search()`; [path_utils.c](src/path_utils.c) ghép đường dẫn khi duyệt.

**Chức năng chính đã làm:**

- Nhận từ khóa như `demo`, `.txt`, `.PDF`; từ chối từ khóa rỗng.
- Nhận thư mục bắt đầu, để trống để tìm từ thư mục hiện tại.
- Tìm tên chứa từ khóa, không phân biệt hoa/thường thông qua `strncasecmp()`.
- Duyệt đệ quy, hiển thị đường dẫn và đếm kết quả; không bị giới hạn bởi 100 mục của bảng chọn.

**Làm đến đâu / còn thiếu:** Đã tìm được trong cây thư mục thông thường. Phần duyệt còn dùng `stat()`, nên đi theo liên kết tới thư mục, có thể gặp vòng liên kết và dừng khi gặp liên kết hỏng/lỗi truy cập. Chưa có lọc theo thuộc tính, hủy tìm kiếm hoặc chọn trực tiếp một kết quả. Việc không phân biệt hoa/thường chưa xử lý đầy đủ tên Unicode.

**Có thể mở rộng:**

- Dùng `lstat()` để tránh đi theo liên kết, tiếp tục các nhánh khác khi một nhánh lỗi.
- Lọc theo loại, kích thước, thời gian, độ sâu; bổ sung xử lý hoa/thường Unicode.
- Cho duyệt kết quả bằng mũi tên rồi chuyển sang xem thuộc tính/sao chép/di chuyển/xóa; hỗ trợ tìm trên cây lớn với tiến độ và hủy tác vụ.

## 10. Tùy chọn 0: Thoát

**File phụ trách:** [app.c](src/app.c) trong `app_run()`; [ui.c](src/ui.c) nhận lựa chọn; [terminal.c](src/terminal.c) khôi phục terminal; [main.c](src/main.c) trả mã thoát.

**Chức năng chính đã làm:**

- Thoát bằng mục 0 hoặc Esc/Ctrl+D ở menu chính; kết thúc khi đầu vào chuyển hướng hết dữ liệu.
- Khôi phục chế độ nhập, con trỏ và màn hình khi kết thúc bảng chọn.
- Xử lý Ctrl+C và các tín hiệu kết thúc được hỗ trợ để khôi phục terminal trước khi chuyển lại cho cơ chế xử lý tín hiệu.

**Làm đến đâu / còn thiếu:** Đã hoàn thành việc thoát trong phạm vi ứng dụng hiện tại. Đường dẫn đang chọn chỉ tồn tại trong phiên chạy; chưa lưu cấu hình hoặc phiên làm việc.

**Có thể mở rộng:**

- Lưu và khôi phục thư mục, đường dẫn gần đây và lựa chọn giao diện giữa các phiên.
- Chuẩn hóa mã thoát để phân biệt người dùng thoát, EOF và lỗi vận hành khi tích hợp vào script.
