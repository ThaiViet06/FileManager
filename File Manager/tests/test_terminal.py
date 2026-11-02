#!/usr/bin/env python3
"""Kiểm thử bàn phím và chế độ terminal bằng pseudo-terminal Linux."""

import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import tempfile
import termios
import time
import unittest


BINARY = Path(os.environ.get("FM_BINARY", Path(__file__).resolve().parents[1] / "file_manager")).resolve()
UP = b"\x1b[A"
DOWN = b"\x1b[B"
HOME = b"\x1b[H"
END = b"\x1b[F"
ENTER = b"\r"
ESC = b"\x1b"


class TerminalSession:
    def __init__(self, root, term="xterm-256color"):
        self.master, self.slave = pty.openpty()
        self.resize(12, 100)
        self.original = termios.tcgetattr(self.slave)
        self.pending = b""
        self.output = b""

        def controlling_terminal():
            os.setsid()
            fcntl.ioctl(self.slave, termios.TIOCSCTTY, 0)

        self.process = subprocess.Popen(
            [str(BINARY)], stdin=self.slave, stdout=self.slave, stderr=self.slave,
            cwd=root, env={**os.environ, "TERM": term, "TZ": "UTC"},
            preexec_fn=controlling_terminal,
        )

    def resize(self, rows, columns):
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))

    def send(self, data):
        os.write(self.master, data)

    def expect(self, text):
        expected = text.encode() if isinstance(text, str) else text
        deadline = time.monotonic() + 5
        while expected not in self.pending:
            if time.monotonic() >= deadline:
                raise AssertionError(f"Không thấy {expected!r}; đầu ra cuối: {self.output[-3000:]!r}")
            if select.select([self.master], [], [], .1)[0]:
                try:
                    data = os.read(self.master, 65536)
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
                    data = b""
                if not data:
                    raise AssertionError(f"Terminal đóng trước {expected!r}: {self.output[-3000:]!r}")
                self.pending += data
                self.output += data
        end = self.pending.index(expected) + len(expected)
        self.pending = self.pending[end:]

    def finish(self, expected_code=0):
        code = self.process.wait(timeout=5)
        if code != expected_code:
            raise AssertionError(f"Mã thoát {code}, cần {expected_code}: {self.output[-3000:]!r}")
        if termios.tcgetattr(self.slave) != self.original:
            raise AssertionError("Chế độ terminal không được khôi phục")

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=5)
        os.close(self.master)
        os.close(self.slave)


class TerminalTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="fm-tty-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "tệp mẫu.txt"
        self.source.write_text("Nội dung xem trước\n")

    def start(self, term="xterm-256color"):
        session = TerminalSession(self.root, term)
        self.addCleanup(session.close)
        session.expect("FILE MANAGER")
        return session

    def pause_and_return(self, session):
        session.expect("Nhấn Enter để quay lại menu")
        self.assertEqual(termios.tcgetattr(session.slave), session.original)
        session.send(ENTER)
        session.expect("FILE MANAGER")

    def select_source(self, session):
        session.send(ENTER)
        session.expect("Chọn đường dẫn")
        # End là Quay lại; mục trước đó cho nhập đường dẫn bằng văn bản.
        session.send(END + UP + ENTER)
        session.expect("Nhập đường dẫn:")
        self.assertEqual(termios.tcgetattr(session.slave), session.original)
        session.send(self.source.name.encode() + ENTER)
        session.expect("làm đường dẫn mặc định thành công")
        self.pause_and_return(session)

    def exit_menu(self, session):
        session.send(UP + ENTER)
        session.finish()

    def test_up_wraps_to_exit_and_restores_terminal(self):
        session = self.start()
        session.expect(b"\x1b[7m > 1.")
        self.exit_menu(session)

    def test_escape_and_ctrl_d_exit_main_menu(self):
        for key in [ESC, b"\x04"]:
            with self.subTest(key=key):
                session = self.start()
                session.send(key)
                session.finish()

    def test_arrow_file_selection_then_view_results(self):
        session = self.start()
        session.send(ENTER)
        session.expect("Chọn đường dẫn")
        session.send(ENTER)
        session.expect("làm đường dẫn mặc định thành công")
        self.pause_and_return(session)
        session.send(DOWN + ENTER)
        session.expect("THÔNG TIN HỆ THỐNG")
        session.expect("Nội dung xem trước")
        self.pause_and_return(session)
        self.exit_menu(session)

    def test_selected_path_actions_use_arrows(self):
        session = self.start()
        self.select_source(session)
        session.send(ENTER)
        session.expect("Đường dẫn đang chọn:")
        session.send(DOWN + ENTER)
        session.expect("Đã hủy chọn đường dẫn mặc định")
        self.pause_and_return(session)
        session.expect("Chưa chọn đường dẫn")
        self.exit_menu(session)

    def test_escape_cancels_path_picker(self):
        session = self.start()
        session.send(ENTER)
        session.expect("Chọn đường dẫn")
        session.send(ESC)
        self.pause_and_return(session)
        session.expect("Chưa chọn đường dẫn")
        self.exit_menu(session)

    def test_delete_defaults_to_cancel_and_requires_arrow_confirmation(self):
        session = self.start()
        self.select_source(session)
        session.send(DOWN * 5 + ENTER)
        session.expect("Xác nhận xóa:")
        session.expect(b"\x1b[7m > " + "Hủy".encode())
        session.send(ENTER)
        session.expect("Đã hủy thao tác xóa")
        self.pause_and_return(session)
        self.assertTrue(self.source.exists())
        session.send(DOWN * 5 + ENTER)
        session.expect("Xác nhận xóa:")
        session.send(DOWN + ENTER)
        session.expect("Đã xóa file")
        self.pause_and_return(session)
        self.assertFalse(self.source.exists())
        self.exit_menu(session)

    def test_copy_destination_and_overwrite_confirmation(self):
        destination = self.root / "dest"
        destination.mkdir()
        target = destination / "copy.txt"
        target.write_text("original")
        session = self.start()
        self.select_source(session)
        for overwrite in [False, True]:
            session.send(DOWN * 4 + ENTER)
            session.expect("Chọn thư mục đích")
            session.send(DOWN + ENTER)
            session.expect("Đặt tên cho file sao chép")
            session.send(b"copy.txt\r")
            session.expect("Đích đã tồn tại:")
            session.send((DOWN if overwrite else b"") + ENTER)
            session.expect("Đã sao chép thành công" if overwrite else "Đã hủy sao chép")
            self.pause_and_return(session)
            self.assertEqual(target.read_text(), self.source.read_text() if overwrite else "original")
        self.exit_menu(session)

    def test_long_listing_pages_and_resizes(self):
        self.source.unlink()
        for index in range(35):
            (self.root / f"item-{index:02}.txt").write_text("sample\n")
        session = self.start()
        session.send(ENTER)
        session.expect("Chọn đường dẫn")
        session.send(b"\x1b[6~")
        session.expect("Mục 9/37")
        session.resize(8, 60)
        session.process.send_signal(signal.SIGWINCH)
        session.expect(b"\x1b[8;1H")
        session.send(END + UP * 2 + ENTER)
        session.expect("làm đường dẫn mặc định thành công")
        self.pause_and_return(session)
        self.exit_menu(session)

    def test_fragmented_and_application_arrow_sequences(self):
        session = self.start()
        session.send(ESC)
        time.sleep(.02)
        session.send(b"O")
        time.sleep(.02)
        session.send(b"A")
        session.expect(b"\x1b[7m > 0.")
        session.send(ENTER)
        session.finish()

    def test_invalid_numeric_shortcut_does_not_activate_current_item(self):
        session = self.start()
        session.send(b"999\r")
        session.expect(b"\a")
        self.assertIsNone(session.process.poll())
        session.send(UP + ENTER)
        session.finish()

    def test_numeric_shortcut_switches_to_latest_option(self):
        session = self.start()
        for number in [b"2", b"5", b"0", b"7", b"2"]:
            session.send(number)
            session.expect(b"\x1b[7m > " + number + b".")
        session.send(b"5" + ENTER)
        session.expect("Chọn tệp thường")
        session.send(ESC)
        self.pause_and_return(session)
        self.exit_menu(session)

    def test_invalid_shortcut_can_be_replaced_with_valid_option(self):
        session = self.start()
        session.send(b"9" * 40 + ENTER)
        session.expect(b"\a")
        self.assertIsNone(session.process.poll())
        session.send(b"0")
        session.expect(b"\x1b[7m > 0.")
        session.send(ENTER)
        session.finish()

    def test_selected_path_action_accepts_replacement_digit(self):
        session = self.start()
        self.select_source(session)
        session.send(ENTER)
        session.expect("Đường dẫn đang chọn:")
        session.send(b"1")
        session.expect(b"\x1b[7m > " + "Chọn đường dẫn khác".encode())
        session.send(b"2" + ENTER)
        session.expect("Đã hủy chọn đường dẫn mặc định")
        self.pause_and_return(session)
        self.exit_menu(session)

    def test_delete_confirmation_can_switch_from_yes_to_no(self):
        session = self.start()
        self.select_source(session)
        session.send(b"6" + ENTER)
        session.expect("Xác nhận xóa:")
        session.send(b"1")
        session.expect(b"\x1b[7m > " + "Xóa tệp".encode())
        session.send(b"0" + ENTER)
        session.expect("Đã hủy thao tác xóa")
        self.assertTrue(self.source.exists())
        self.pause_and_return(session)
        self.exit_menu(session)

    def test_multidigit_file_shortcut_still_selects_twelve(self):
        for index in range(15):
            (self.root / f"item-{index:02}.txt").write_text("sample\n")
        session = self.start()
        session.send(ENTER)
        session.expect("Chọn đường dẫn")
        session.send(b"12")
        session.expect(b"\x1b[7m > [12] [FILE]")
        session.send(ENTER)
        session.expect("làm đường dẫn mặc định thành công")
        self.pause_and_return(session)
        self.exit_menu(session)

    def test_overflowing_file_index_is_rejected(self):
        for index in range(15):
            (self.root / f"item-{index:02}.txt").write_text("sample\n")
        session = self.start()
        session.send(ENTER)
        session.expect("Chọn đường dẫn")
        session.send(b"9" * 40 + b"0\r")
        session.expect(b"\a")
        self.assertIsNone(session.process.poll())
        session.send(ESC)
        self.pause_and_return(session)
        self.exit_menu(session)

    def test_control_characters_in_filename_do_not_break_menu(self):
        self.source.unlink()
        (self.root / "file\x1b[2J.txt").write_text("sample\n")
        session = self.start()
        session.send(ENTER)
        session.expect("file?[2J.txt")
        self.assertNotIn(b"file\x1b[2J.txt", session.output)
        session.send(ESC)
        self.pause_and_return(session)
        self.exit_menu(session)

    def test_signals_restore_terminal(self):
        for caught in [signal.SIGINT, signal.SIGTERM, signal.SIGHUP]:
            with self.subTest(signal=caught):
                session = self.start()
                session.process.send_signal(caught)
                session.finish(-caught)

    def test_ctrl_c_restores_terminal(self):
        session = self.start()
        session.send(b"\x03")
        session.finish(-signal.SIGINT)

    def test_dumb_terminal_uses_numbered_menu(self):
        session = self.start("dumb")
        session.expect("Chọn chức năng [0-7]")
        self.assertEqual(termios.tcgetattr(session.slave), session.original)
        session.send(b"0\r")
        session.finish()


if __name__ == "__main__":
    unittest.main(verbosity=2)
