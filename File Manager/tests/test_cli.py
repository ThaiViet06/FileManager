#!/usr/bin/env python3
"""Kiểm thử menu và thao tác tệp trong thư mục tạm, chỉ dùng thư viện chuẩn."""

import os
import re
import datetime
import grp
import pwd
import socket
import stat
from pathlib import Path
import subprocess
import tempfile
import unittest


BINARY = Path(os.environ.get("FM_BINARY", Path(__file__).resolve().parents[1] / "file_manager")).resolve()


class CliTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="fm-cli-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / "test_files").mkdir()
        (self.root / "dest").mkdir()
        self.source = self.root / "test_files" / "tệp mẫu.txt"
        self.content = b"File Manager demo\nsecond line\n"
        self.source.write_bytes(self.content)

    def cli(self, *lines):
        data = "\n".join(lines) + ("\n" if lines else "")
        result = subprocess.run(
            [str(BINARY)], input=data, text=True, capture_output=True,
            cwd=self.root, timeout=5, env={**os.environ, "TZ": "UTC"},
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout + result.stderr

    def selected_cli(self, *lines):
        return self.cli("1", "0", "test_files/tệp mẫu.txt", *lines)

    def number_for(self, name):
        output = self.cli("1")
        match = re.search(r"^\s+\[(\d+)\] \[[A-Z ]+\]\s+" + re.escape(name) + r"(?=\s|$)",
                          output, re.MULTILINE)
        self.assertIsNotNone(match, output)
        return match.group(1)

    def test_exit_and_eof(self):
        self.assertIn("Đã thoát chương trình.", self.cli("0"))
        self.assertIn("MIDTERM FILE MANAGER", self.cli())

    def test_invalid_menu_choices(self):
        output = self.cli("abc", "8", "-1", " " * 4, "9" * 40, "0")
        self.assertEqual(output.count("không hợp lệ"), 5)
        self.assertIn("Đã thoát chương trình.", output)

    def test_overlong_menu_line_is_discarded(self):
        output = self.cli("x" * 63 + "6", "0")
        self.assertIn("Dữ liệu nhập quá dài", output)
        self.assertNotIn("Chọn file cần xóa", output)
        self.assertIn("Đã thoát chương trình.", output)

    def test_select_and_preview(self):
        output = self.selected_cli("2", "0")
        self.assertIn("[Đường dẫn đang chọn: test_files/tệp mẫu.txt]", output)
        self.assertIn("File Manager demo", output)

    def test_manual_absolute_path_and_clear_selection(self):
        output = self.cli("1", "0", str(self.source), "2", "1", "2", "0")
        self.assertIn(f"[Đường dẫn đang chọn: {self.source}]", output)
        self.assertIn("File Manager demo", output)
        self.assertIn("Đã hủy chọn đường dẫn mặc định", output)
        self.assertIn("[Chưa chọn đường dẫn mặc định]", output.split("Đã hủy chọn đường dẫn mặc định")[-1])

    def test_change_selection_and_go_back(self):
        other = self.root / "other.txt"
        other.write_text("other content\n")
        output = self.selected_cli("1", "0", "1", "1", "0", str(other), "2", "0")
        self.assertIn(f"[Đường dẫn đang chọn: {other}]", output)
        self.assertIn("other content", output)
        self.assertTrue(self.source.exists())

    def test_invalid_file_and_directory_choices(self):
        output = self.cli("1", "abc", "-1", "999", "0", "test_files/tệp mẫu.txt", "5", "0", "dest", "copy.txt", "0")
        self.assertEqual(output.count("Số chọn không hợp lệ"), 3)
        self.assertEqual((self.root / "dest" / "copy.txt").read_bytes(), self.content)
        output = self.selected_cli("5", "bad", "999", "0", "dest", "other.txt", "0")
        self.assertEqual(output.count("Số chọn không hợp lệ"), 2)
        self.assertEqual((self.root / "dest" / "other.txt").read_bytes(), self.content)

    def test_copy_default_name(self):
        output = self.selected_cli("5", "0", "dest", "", "0")
        self.assertIn("Đã sao chép thành công", output)
        self.assertEqual((self.root / "dest" / "copy_tệp mẫu.txt").read_bytes(), self.content)
        self.assertEqual(self.source.read_bytes(), self.content)

    def test_copy_to_numbered_base_directory(self):
        self.selected_cli("5", "1", "copy.txt", "0")
        self.assertEqual((self.root / "copy.txt").read_bytes(), self.content)

    def test_copy_to_numbered_child_directory(self):
        # Tệp nguồn ở ngoài để bảng chọn thư mục con chỉ có duy nhất dest.
        self.source.rename(self.root / "source.txt")
        (self.root / "test_files").rmdir()
        output = self.cli("1", "0", "source.txt", "5", "2", "copy.txt", "0")
        self.assertIn("Đã sao chép thành công", output)
        self.assertEqual((self.root / "dest" / "copy.txt").read_bytes(), self.content)

    def test_overwrite_cancel_and_confirm(self):
        destination = self.root / "dest" / "existing.txt"
        destination.write_bytes(b"original")
        output = self.selected_cli("5", "0", "dest", "existing.txt", "n", "0")
        self.assertIn("Đã hủy sao chép", output)
        self.assertEqual(destination.read_bytes(), b"original")
        output = self.selected_cli("5", "0", "dest", "existing.txt", "Y", "0")
        self.assertIn("Đã sao chép thành công", output)
        self.assertEqual(destination.read_bytes(), self.content)

    def test_same_file_and_hard_link_are_rejected(self):
        os.link(self.source, self.root / "dest" / "hard.txt")
        output = self.selected_cli("5", "0", "test_files", self.source.name, "0")
        self.assertIn("Lỗi sao chép", output)
        output = self.selected_cli("5", "0", "dest", "hard.txt", "0")
        self.assertIn("Lỗi sao chép", output)
        self.assertEqual(self.source.read_bytes(), self.content)

    def test_move_updates_selected_path(self):
        output = self.selected_cli("4", "0", "dest", "renamed.txt", "2", "0")
        self.assertIn("[Đường dẫn đang chọn: dest/renamed.txt]", output)
        self.assertIn("Đang dùng đường dẫn mặc định: dest/renamed.txt", output)
        self.assertFalse(self.source.exists())
        self.assertEqual((self.root / "dest" / "renamed.txt").read_bytes(), self.content)

    def test_cancelled_move_keeps_selection(self):
        destination = self.root / "dest" / "existing.txt"
        destination.write_text("original")
        output = self.selected_cli("4", "0", "dest", "existing.txt", "n", "2", "0")
        self.assertIn("Đã hủy di chuyển", output)
        self.assertIn("Đang dùng đường dẫn mặc định: test_files/tệp mẫu.txt", output)
        self.assertEqual(destination.read_text(), "original")
        self.assertTrue(self.source.exists())

    def test_delete_clears_selection(self):
        output = self.selected_cli("6", "y", "0")
        self.assertIn("Đã xóa file", output)
        self.assertFalse(self.source.exists())
        self.assertIn("[Chưa chọn đường dẫn mặc định]", output.split("Đã xóa file")[-1])

    def test_delete_requires_exact_confirmation(self):
        for answer in ["n", "yes", "y" * 16 + "6"]:
            with self.subTest(answer=answer):
                output = self.selected_cli("6", answer, "0")
                self.assertIn("Đã hủy thao tác xóa", output)
                self.assertTrue(self.source.exists())
                self.assertIn("Đã thoát chương trình.", output)

    def test_overlong_overwrite_confirmation_preserves_destination(self):
        destination = self.root / "dest" / "existing.txt"
        destination.write_bytes(b"original")
        output = self.selected_cli("5", "0", "dest", "existing.txt", "y" * 16 + "6", "0")
        self.assertIn("Đã hủy sao chép", output)
        self.assertEqual(destination.read_bytes(), b"original")
        self.assertTrue(self.source.exists())
        self.assertIn("Đã thoát chương trình.", output)

    def test_cancel_manual_selection_returns_to_menu(self):
        output = self.cli("1", "0", "", "0")
        self.assertEqual(output.count("Danh sách đường dẫn trong '.'"), 1)
        self.assertIn("Đã thoát chương trình.", output)

    def test_overlong_manual_path_aborts_selection(self):
        output = self.cli("1", "0", "x" * 1024, "0")
        self.assertIn("Dữ liệu nhập quá dài", output)
        self.assertEqual(output.count("Danh sách đường dẫn trong '.'"), 1)
        self.assertIn("Đã thoát chương trình.", output)
        self.assertTrue(self.source.exists())

    def test_overlong_destination_name_aborts_operation(self):
        for action in ["4", "5"]:
            with self.subTest(action=action):
                output = self.selected_cli(action, "0", "dest", "x" * 256, "2", "0")
                self.assertIn("Dữ liệu nhập quá dài", output)
                self.assertIn("Đang dùng đường dẫn mặc định: test_files/tệp mẫu.txt", output)
                self.assertTrue(self.source.exists())
                self.assertEqual(list((self.root / "dest").iterdir()), [])

    def test_exact_capacity_destination_name(self):
        name = "x" * 255
        output = self.selected_cli("5", "0", "dest", name, "0")
        self.assertIn("Đã sao chép thành công", output)
        self.assertEqual((self.root / "dest" / name).read_bytes(), self.content)

    def test_tree_and_case_insensitive_search(self):
        output = self.cli("3", "", "7", ".TXT", "", "0")
        self.assertIn("├── test_files", output)
        self.assertIn("test_files/tệp mẫu.txt", output)
        self.assertIn("Tìm thấy: 1 mục.", output)

    def test_eof_at_submenus(self):
        cases = [
            ("1",), ("1", "0"), ("2",), ("3",), ("4",), ("5",), ("6",), ("7",),
            ("7", ".txt"), ("1", "0", "test_files/tệp mẫu.txt", "1"), ("1", "0", "test_files/tệp mẫu.txt", "4"),
            ("1", "0", "test_files/tệp mẫu.txt", "4", "0"), ("1", "0", "test_files/tệp mẫu.txt", "4", "0", "dest"),
            ("1", "0", "test_files/tệp mẫu.txt", "5"), ("1", "0", "test_files/tệp mẫu.txt", "5", "0", "dest"), ("1", "0", "test_files/tệp mẫu.txt", "6"),
        ]
        for lines in cases:
            with self.subTest(lines=lines):
                self.cli(*lines)
                self.assertTrue(self.source.exists())
                self.assertEqual(list((self.root / "dest").iterdir()), [])

    def test_failed_operations_preserve_selection(self):
        output = self.selected_cli("4", "0", "missing", "2", "0")
        self.assertIn("Không thể chọn đường dẫn", output)
        self.assertIn("Đang dùng đường dẫn mặc định: test_files/tệp mẫu.txt", output)
        self.assertEqual(self.source.read_bytes(), self.content)

    def make_links(self):
        (self.root / "file-link").symlink_to(self.source)
        (self.root / "dir-link").symlink_to("dest", target_is_directory=True)
        (self.root / "broken-link").symlink_to("missing")
        (self.root / "loop-link").symlink_to("loop-link")

    def test_current_directory_listing_labels_all_types(self):
        self.make_links()
        (self.root / "local.txt").write_text("local content\n")
        os.mkfifo(self.root / "named-pipe")
        output = self.cli("1")
        for kind, name in [("FILE", "local.txt"), ("DIR", "dest"),
                           ("DIR", "test_files"), ("LINK", "file-link"),
                           ("LINK", "dir-link"), ("LINK", "broken-link"),
                           ("LINK", "loop-link"), ("OTHER", "named-pipe")]:
            with self.subTest(name=name):
                self.assertRegex(output, r"\[" + kind + r" *\]\s+" + re.escape(name))
        self.assertIn("Danh sách đường dẫn trong '.'", output)
        self.assertNotIn("Không thể mở thư mục", output)

    def test_no_demo_directory_required(self):
        self.source.rename(self.root / "local.txt")
        (self.root / "test_files").rmdir()
        output = self.cli("1", "0", "local.txt", "2", "0")
        self.assertIn("[Đường dẫn đang chọn: local.txt]", output)
        self.assertIn("File Manager demo", output)
        self.assertNotIn("Không thể mở thư mục", output)

    def test_numbered_regular_file_selection(self):
        name = "tệp ở thư mục hiện tại.txt"
        (self.root / name).write_bytes(self.content)
        output = self.cli("1", self.number_for(name), "2", "0")
        self.assertIn(f"[Đường dẫn đang chọn: ./{name}]", output)
        self.assertIn("File Manager demo", output)

    def test_numbered_directory_selection_and_info(self):
        output = self.cli("1", self.number_for("dest"), "2", "0")
        self.assertIn("[Đường dẫn đang chọn: ./dest]", output)
        self.assertIn("Đang dùng đường dẫn mặc định: ./dest", output)
        self.assertIn("THÔNG TIN HỆ THỐNG", output)
        self.assertNotIn("Xem trước nội dung", output)

    def test_manual_directory_selection(self):
        for path in [".", "dest", str(self.root / "dest")]:
            with self.subTest(path=path):
                output = self.cli("1", "0", path, "0")
                self.assertIn(f"[Đường dẫn đang chọn: {path}]", output)

    def test_link_selection_by_number_and_absolute_path(self):
        self.make_links()
        for name in ["file-link", "dir-link", "broken-link", "loop-link"]:
            with self.subTest(name=name):
                output = self.cli("1", self.number_for(name), "0")
                self.assertIn(f"[Đường dẫn đang chọn: ./{name}]", output)
                output = self.cli("1", "0", str(self.root / name), "0")
                self.assertIn(f"[Đường dẫn đang chọn: {self.root / name}]", output)
                self.assertTrue(os.path.lexists(self.root / name))

    def test_invalid_new_selection_preserves_previous_selection(self):
        output = self.selected_cli("1", "1", "0", "missing", "2", "0")
        self.assertIn("Không thể chọn đường dẫn", output)
        self.assertNotIn("[Đường dẫn đang chọn: missing]", output)
        self.assertIn("Đang dùng đường dẫn mặc định: test_files/tệp mẫu.txt", output)

    def test_info_picker_accepts_directory_without_default_selection(self):
        output = self.cli("2", self.number_for("dest"), "0")
        self.assertIn("THÔNG TIN HỆ THỐNG", output)
        self.assertIn("./dest", output)
        self.assertNotIn("Xem trước nội dung", output)

    def test_selected_nonregular_paths_reject_file_operations(self):
        self.make_links()
        os.mkfifo(self.root / "named-pipe")
        for name in ["dest", "file-link", "dir-link", "broken-link", "loop-link", "named-pipe"]:
            for action in ["4", "5", "6"]:
                with self.subTest(name=name, action=action):
                    output = self.cli("1", "0", name, action, "0")
                    self.assertIn("Thao tác này chỉ hỗ trợ tệp thường", output)
                    self.assertNotIn("Danh sách thư mục đích", output)
                    self.assertNotIn("Bạn có chắc chắn muốn xóa", output)
                    self.assertIn(f"[Đường dẫn đang chọn: {name}]", output)
                    self.assertTrue(os.path.lexists(self.root / name))
                    self.assertEqual(self.source.read_bytes(), self.content)

    def test_operation_picker_rejects_manual_nonregular_source(self):
        self.make_links()
        for name in ["dest", "file-link", "broken-link"]:
            for action in ["4", "5", "6"]:
                with self.subTest(name=name, action=action):
                    output = self.cli(action, "0", name, "0")
                    self.assertIn("Chỉ chấp nhận tệp thường", output)
                    self.assertTrue(os.path.lexists(self.root / name))
        output = self.cli("5")
        self.assertNotIn("[LINK]", output)
        self.assertNotIn("[DIR ]", output)

    def test_destination_picker_rejects_directory_link(self):
        self.make_links()
        output = self.selected_cli("5", "0", "dir-link", "0")
        self.assertIn("Chỉ chấp nhận thư mục", output)
        self.assertTrue(self.source.exists())
        self.assertEqual(list((self.root / "dest").iterdir()), [])

    def test_truncated_listing_still_allows_manual_selection(self):
        names = [f"item-{number:03}.txt" for number in range(105)]
        for name in names:
            (self.root / name).write_text("sample\n")
        output = self.cli("1")
        self.assertIn("Danh sách giới hạn 100 mục", output)
        omitted = next(name for name in names if name not in output)
        output = self.cli("1", "0", omitted, "2", "0")
        self.assertIn(f"[Đường dẫn đang chọn: {omitted}]", output)
        self.assertIn("sample", output)

    def test_overlong_picker_choice_discards_remainder(self):
        output = self.cli("1", "x" * 64 + "6", "0", "test_files/tệp mẫu.txt", "0")
        self.assertIn("Dữ liệu nhập quá dài", output)
        self.assertIn("[Đường dẫn đang chọn: test_files/tệp mẫu.txt]", output)
        self.assertTrue(self.source.exists())

    def check_attributes(self, path, kind):
        before = path.lstat()
        output = self.cli("1", "0", str(path), "2", "0")
        fields = {}
        for label in ["Loại", "Dung lượng", "Quyền", "Chủ sở hữu", "Nhóm", "Inode",
                      "Liên kết cứng", "Truy cập", "Sửa đổi", "Trạng thái"]:
            match = re.search(r"^" + re.escape(label) + r"\s+: (.*)$", output, re.MULTILINE)
            self.assertIsNotNone(match, output)
            fields[label] = match.group(1)
        self.assertEqual(fields["Loại"], kind)
        self.assertEqual(fields["Dung lượng"], f"{before.st_size} byte ({before.st_size / 1024:.2f} KiB)")
        self.assertEqual(fields["Quyền"], f"{stat.filemode(before.st_mode)[1:]} ({stat.S_IMODE(before.st_mode):04o})")
        self.assertEqual(fields["Inode"], str(before.st_ino))
        self.assertEqual(fields["Liên kết cứng"], str(before.st_nlink))
        try:
            expected_owner = f"{before.st_uid} ({pwd.getpwuid(before.st_uid).pw_name})"
        except KeyError:
            expected_owner = str(before.st_uid)
        try:
            expected_group = f"{before.st_gid} ({grp.getgrgid(before.st_gid).gr_name})"
        except KeyError:
            expected_group = str(before.st_gid)
        self.assertEqual(fields["Chủ sở hữu"], expected_owner)
        self.assertEqual(fields["Nhóm"], expected_group)
        for label, value, suffix in [("Truy cập", before.st_atime, "atime"),
                                      ("Sửa đổi", before.st_mtime, "mtime"),
                                      ("Trạng thái", before.st_ctime, "ctime; không phải ngày tạo")]:
            expected = datetime.datetime.fromtimestamp(value, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S %z")
            self.assertEqual(fields[label], f"{expected} ({suffix})")
        self.assertNotIn("Lỗi đọc thông tin đường dẫn", output)
        return fields, output

    def test_standard_attributes_match_stat_and_ls(self):
        self.source.chmod(0o640)
        os.link(self.source, self.root / "hard-link.txt")
        os.utime(self.source, (1700000000, 1700000123))
        system = subprocess.check_output(
            ["stat", "--printf=%s|%a|%u|%g|%i|%h", "--", str(self.source)], text=True,
        ).split("|")
        listing = subprocess.check_output(["ls", "-ldn", "--", str(self.source)], text=True).split()
        fields, _ = self.check_attributes(self.source, "Tệp thường")
        self.assertEqual(fields["Dung lượng"].split()[0], system[0])
        self.assertIn(f"({int(system[1], 8):04o})", fields["Quyền"])
        self.assertEqual(fields["Quyền"].split()[0], listing[0][1:10])
        self.assertEqual(fields["Chủ sở hữu"].split()[0], system[2])
        self.assertEqual(fields["Nhóm"].split()[0], system[3])
        self.assertEqual(fields["Inode"], system[4])
        self.assertEqual(fields["Liên kết cứng"], system[5])

    def test_empty_file_attributes(self):
        path = self.root / "empty.txt"
        path.touch()
        fields, _ = self.check_attributes(path, "Tệp thường")
        self.assertEqual(fields["Dung lượng"], "0 byte (0.00 KiB)")

    def test_large_sparse_file_attributes(self):
        path = self.root / "large.txt"
        with path.open("wb") as stream:
            # Giới hạn xem trước được đạt trước vùng trống của tệp thưa.
            stream.write(b"\n" * 16)
            stream.truncate(5 * 1024 ** 3 + 123)
        self.check_attributes(path, "Tệp thường")

    def test_directory_size_is_not_recursive_total(self):
        child = self.root / "dest" / "child.txt"
        with child.open("wb") as stream:
            stream.truncate(1024 * 1024)
        fields, output = self.check_attributes(self.root / "dest", "Thư mục")
        self.assertNotEqual(int(fields["Dung lượng"].split()[0]), child.stat().st_size)
        self.assertIn("không cộng dung lượng bên trong", output)
        self.assertNotIn("Xem trước nội dung", output)

    def test_symlink_attributes_describe_link_including_broken_and_loop(self):
        self.make_links()
        for name in ["file-link", "dir-link", "broken-link", "loop-link"]:
            with self.subTest(name=name):
                path = self.root / name
                _, output = self.check_attributes(path, "Liên kết tượng trưng")
                self.assertIn(f"Đích liên kết : {os.readlink(path)}\n", output)
                self.assertNotIn("Xem trước nội dung", output)

    def test_long_symlink_target_is_not_truncated(self):
        for index, target in enumerate(["x" * 128, "x" * 129, "đích/" * 300]):
            with self.subTest(length=len(os.fsencode(target))):
                path = self.root / f"long-link-{index}"
                path.symlink_to(target)
                _, output = self.check_attributes(path, "Liên kết tượng trưng")
                self.assertIn(f"Đích liên kết : {target}\n", output)

    def test_special_permission_bits(self):
        for mode in [0o4755, 0o4644, 0o2755, 0o2644, 0o1777, 0o1666]:
            with self.subTest(mode=oct(mode)):
                self.source.chmod(mode)
                self.assertEqual(stat.S_IMODE(self.source.stat().st_mode), mode)
                self.check_attributes(self.source, "Tệp thường")

    def test_fifo_attributes_do_not_open_pipe(self):
        path = self.root / "pipe"
        os.mkfifo(path)
        _, output = self.check_attributes(path, "FIFO (ống có tên)")
        self.assertNotIn("Xem trước nội dung", output)

    def test_socket_attributes(self):
        server = socket.socket(socket.AF_UNIX)
        self.addCleanup(server.close)
        path = self.root / "socket"
        server.bind(str(path))
        self.check_attributes(path, "Socket")

    def test_character_device_attributes(self):
        self.check_attributes(Path("/dev/null"), "Thiết bị ký tự")


if __name__ == "__main__":
    unittest.main(verbosity=2)
