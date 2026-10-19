"""Terminal rendering regression tests; run after make with python3 tests/test_terminal.py."""
import errno
import fcntl
import os
from pathlib import Path
import pty
import re
import struct
import subprocess
import tempfile
import termios
import unittest
import zlib

QRB = str(Path(os.environ.get('QRB', './qrb')).resolve())
SGR = re.compile(r'\x1b\[[0-9;]*m')


def run_qrb(args=(), columns=None, env=None, text='https://example.com'):
    environ = os.environ.copy()
    for name in ('COLUMNS', 'TERM_PROGRAM', 'WSL_INTEROP', 'WSL_DISTRO_NAME'):
        environ.pop(name, None)
    environ.update(env or {})
    command = [QRB, '-t', text, '-s', 'qr', *args]
    if columns is None:
        return subprocess.run(command, env=environ, capture_output=True)
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 50, columns, 0, 0))
    process = subprocess.Popen(command, env=environ, stdout=slave, stderr=subprocess.PIPE)
    os.close(slave)
    output = bytearray()
    try:
        while True:
            try:
                data = os.read(master, 65536)
            except OSError as error:
                if error.errno == errno.EIO:
                    break
                raise
            if not data:
                break
            output.extend(data)
    finally:
        os.close(master)
    _, errors = process.communicate(timeout=10)
    return subprocess.CompletedProcess(command, process.returncode, bytes(output), errors)


def png_matrix(text):
    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp) / 'qr.png'
        result = run_qrb(['-o', str(path), '--scale', '1'], text=text)
        assert result.returncode == 0, result.stderr
        data = path.read_bytes()
    pos, compressed = 8, bytearray()
    while pos < len(data):
        length = struct.unpack('>I', data[pos:pos + 4])[0]
        kind, payload = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        if kind == b'IHDR':
            width, height = struct.unpack('>II', payload[:8])
        elif kind == b'IDAT':
            compressed.extend(payload)
        pos += length + 12
    raw = zlib.decompress(compressed)
    rows = []
    for y in range(height):
        row = raw[y * (width + 1):(y + 1) * (width + 1)]
        assert row[0] == 0
        rows.append([pixel == 0 for pixel in row[1:]])
    return rows


def unicode_matrix(output):
    rows = []
    for line in SGR.sub('', output.decode('utf-8')).splitlines():
        cells = {' ': (False, False), '█': (True, True),
                 '▀': (True, False), '▄': (False, True)}
        pairs = [cells[c] for c in line]
        rows.extend([[p[0] for p in pairs], [p[1] for p in pairs]])
    return rows


def ansi_matrix(output):
    rows = []
    for line in output.decode('ascii').splitlines():
        rows.append([value == '0;0;0' for value in
                     re.findall(r'\x1b\[0;48;2;(0;0;0|255;255;255)m  ', line)])
        assert line.endswith('\x1b[0m')
    return rows


class TerminalTests(unittest.TestCase):
    def test_both_modes_preserve_every_module_and_quiet_zone(self):
        for text in ('HELLO', 'https://example.com', 'x' * 40, 'x' * 70):
            with self.subTest(text=text):
                expected = png_matrix(text)
                result = run_qrb(['--terminal', 'unicode'], text=text)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertNotIn(b'\x1b', result.stdout)
                rows = unicode_matrix(result.stdout)
                self.assertEqual(rows[:len(expected)], expected)
                self.assertFalse(any(rows[-1]))  # Odd final row is padded white.
                result = run_qrb(['--terminal', 'ansi'], text=text)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(ansi_matrix(result.stdout), expected)

    def test_integer_magnification_preserves_modules(self):
        expected = png_matrix('https://example.com')
        for scale in [2, 3, 8]:
            enlarged = []
            for row in expected:
                enlarged.extend([[pixel for pixel in row for _ in range(scale)]] * scale)
            for mode in ['ansi', 'unicode']:
                with self.subTest(scale=scale, mode=mode):
                    result = run_qrb(['--terminal', mode, '--terminal-scale', str(scale)])
                    self.assertEqual(result.returncode, 0, result.stderr)
                    rows = ansi_matrix(result.stdout) if mode == 'ansi' else unicode_matrix(result.stdout)
                    self.assertEqual(rows[:len(enlarged)], enlarged)
                    if len(rows) > len(enlarged):
                        self.assertFalse(any(rows[-1]))
        result = run_qrb(['--terminal', 'ansi', '--terminal-scale', '2'], columns=100,
                         env={'TERM': 'xterm'})
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b'')
        self.assertIn(b'133', result.stderr)

    def test_wsl_auto_uses_ascii_bytes_and_backgrounds(self):
        result = run_qrb(columns=100, env={'TERM': 'xterm-256color', 'WSL_DISTRO_NAME': 'Ubuntu'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(ansi_matrix(result.stdout), png_matrix('https://example.com'))

    def test_vscode_keeps_half_height_and_sets_contrast(self):
        result = run_qrb(columns=100, env={'TERM': 'xterm-256color',
                         'WSL_DISTRO_NAME': 'Ubuntu', 'TERM_PROGRAM': 'vscode'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b'38;2;0;0;0;48;2;255;255;255m', result.stdout)
        expected = png_matrix('https://example.com')
        self.assertEqual(unicode_matrix(result.stdout)[:len(expected)], expected)

    def test_narrow_terminal_fails_without_partial_output(self):
        for mode, columns in [('unicode', 33), ('ansi', 66)]:
            result = run_qrb(['--terminal', mode], columns=columns,
                             env={'TERM': 'xterm', 'COLUMNS': '999'})
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, b'')
            self.assertIn(b'Terminal too narrow', result.stderr)

    def test_terminal_width_overrides_stale_columns(self):
        result = run_qrb(['--terminal', 'unicode'], columns=80,
                         env={'TERM': 'xterm', 'COLUMNS': '10'})
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_environment_width_fallback(self):
        result = run_qrb(env={'COLUMNS': '20'})
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b'')
        result = run_qrb(env={'COLUMNS': '20junk'})
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_no_lossy_compression_or_invalid_modes(self):
        for args in (['--hscale', '2'], ['--terminal', 'bogus']):
            result = run_qrb(args)
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue(result.stderr)

    def test_redirected_wsl_auto_has_no_escape_sequences(self):
        result = run_qrb(env={'TERM': 'xterm', 'WSL_DISTRO_NAME': 'Ubuntu'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn(b'\x1b', result.stdout)

    def test_dumb_terminal_does_not_receive_ansi(self):
        result = run_qrb(columns=100, env={'TERM': 'dumb', 'WSL_DISTRO_NAME': 'Ubuntu'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn(b'\x1b', result.stdout)
        result = run_qrb(['--terminal', 'ansi'], columns=100, env={'TERM': 'dumb'})
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b'')


if __name__ == '__main__':
    unittest.main()
