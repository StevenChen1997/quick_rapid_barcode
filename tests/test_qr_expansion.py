"""Independent QR checks: requires qrcode, pillow, zxing-cpp (test-only)."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

import qrcode
from qrcode import base, constants, util
from PIL import Image
import zxingcpp

QRB = str(Path(os.environ.get('QRB', './qrb')).resolve())
EXTRA = ['--sse'] if os.environ.get('QRB_TEST_SSE') else []


class ExpansionTests(unittest.TestCase):
    def generate(self, text, version=0, scale=3):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'qr.png'
            result = subprocess.run([QRB, '-s', 'qr', '-t', text,
                                     '--qr-version', str(version), '--scale', str(scale),
                                     '-o', str(output), *EXTRA], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            return Image.open(output).copy()

    def test_all_versions_at_capacity_against_independent_encoder_and_decoder(self):
        for version in range(1, 41):
            with self.subTest(version=version):
                count = sum(b.data_count for b in base.rs_blocks(version, constants.ERROR_CORRECT_L))
                capacity = count - (2 if version < 10 else 3)
                text = ('https://example.com/path?a=1&b=2-' * 100)[:capacity]
                image = self.generate(text, version)
                expected = qrcode.QRCode(version=version, error_correction=constants.ERROR_CORRECT_L,
                                         border=4, box_size=1, mask_pattern=0)
                expected.add_data(util.QRData(text.encode(), mode=util.MODE_8BIT_BYTE))
                expected.make(fit=False)
                matrix = expected.get_matrix()
                self.assertEqual(image.size, (len(matrix) * 3, len(matrix) * 3))
                actual = [[image.getpixel((x * 3 + 1, y * 3 + 1)) == 0
                           for x in range(len(matrix))] for y in range(len(matrix))]
                self.assertEqual(actual, matrix)
                decoded = zxingcpp.read_barcode(image)
                self.assertIsNotNone(decoded)
                self.assertEqual(decoded.bytes, text.encode())
                auto = self.generate(text, 0, scale=1)
                self.assertEqual(auto.size, (len(matrix), len(matrix)))
                result = subprocess.run([QRB, '-s', 'qr', '-t', text + 'x',
                                         '--qr-version', str(version), *EXTRA], capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b'')

    def test_utf8_url_and_long_url_roundtrip(self):
        for text in ['https://example.com/' + '頁面' * 40,
                     'https://example.com/search?q=' + 'long-query-' * 150]:
            decoded = zxingcpp.read_barcode(self.generate(text))
            self.assertIsNotNone(decoded)
            self.assertEqual(decoded.bytes, text.encode())

    def test_invalid_options(self):
        for flag, values in [('--qr-version', ['-1', '41', '2junk', '99999999999999999999']),
                             ('--terminal-scale', ['0', '-1', '9', '1.5', 'two'])]:
            for value in values:
                with self.subTest(flag=flag, value=value):
                    result = subprocess.run([QRB, '-t', 'HELLO', '-s', 'qr', flag, value],
                                            capture_output=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertTrue(result.stderr)


if __name__ == '__main__':
    unittest.main()
