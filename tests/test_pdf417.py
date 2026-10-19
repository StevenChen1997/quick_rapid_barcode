"""Integration tests. Optional cross-checks require pillow, zxing-cpp and pdf417gen."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

try:
    from PIL import Image
    import zxingcpp
    import pdf417gen
    HAS_EXTERNAL = True
except ImportError:
    HAS_EXTERNAL = False

QRB = str(Path(os.environ.get('QRB', './qrb')).resolve())


class Pdf417Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_qrb(self, *args):
        return subprocess.run([QRB, *map(str, args)], capture_output=True, timeout=30)

    def generate(self, data, *options):
        source, output = self.root / 'payload.txt', self.root / 'code.png'
        source.write_bytes(data)
        result = self.run_qrb('--input-file', source, '-s', 'pdf417', '-o', output, *options)
        self.assertEqual(result.returncode, 0, result.stderr)
        return output

    def test_direct_string_and_exact_stdout(self):
        path = self.root / 'direct.png'
        data = '  M1TEST/PASSENGER    AB123  '
        result = self.run_qrb('-t', data, '-s', 'pdf417', '-o', path)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = self.run_qrb('--decode', path)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, data.encode())

    def test_text_and_file_roundtrips(self):
        for data in [b'A', b'0123456789' * 100, b'  TEST/PASSENGER    \r\n\t ',
                     b'embedded\x00byte', 'https://example.com/登機證/測試'.encode()]:
            with self.subTest(data=data[:30]):
                path = self.generate(data)
                out = self.root / 'decoded.bin'
                result = self.run_qrb('--decode', path, '-o', out)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, b'')
                self.assertEqual(out.read_bytes(), data)

    def test_ecc_and_column_settings(self):
        for ecc in [0, 2, 5, 8]:
            path = self.generate(b'TEST PAYLOAD 12345', '--pdf417-ecc', ecc)
            result = self.run_qrb('--decode', path)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, b'TEST PAYLOAD 12345')
        for columns in [1, 6, 30]:
            path = self.generate(b'COLUMN TEST', '--pdf417-columns', columns)
            result = self.run_qrb('--decode', path)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, b'COLUMN TEST')

    def test_bad_input_and_argument_conflicts(self):
        image = self.generate(b'VALID')
        for args in [('--decode', image, '-t', 'conflict'), ('--decode', image, '-s', 'pdf417'),
                     ('--decode', image, '--scale', '2'),
                     ('--input-file', self.root / 'payload.txt', '-t', 'conflict', '-s', 'pdf417'),
                     ('-t', 'x', '-s', 'pdf417', '--pdf417-ecc', '9'),
                     ('-t', 'x', '-s', 'pdf417', '--pdf417-columns', '31'),
                     ('-t', 'x', '-s', 'pdf417', '--pdf417-ecc', '2x'),
                     ('-t', 'x', '-s', 'pdf417'), ('--decode', self.root / 'missing.png')]:
            with self.subTest(args=args):
                result = self.run_qrb(*args)
                self.assertNotEqual(result.returncode, 0)
        for data in [b'', b'\xff\xfe', b'x' * 10001, b'x' * 5000]:
            src = self.root / 'bad.txt'; src.write_bytes(data)
            target = self.root / 'must-not-exist.png'
            result = self.run_qrb('--input-file', src, '-s', 'pdf417', '-o', target)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(target.exists())

    def test_invalid_images_and_output_failure(self):
        for data in [b'not an image', b'\x89PNG\r\n\x1a\n', b'\xff\xd8\xff']:
            path = self.root / 'bad.png'; path.write_bytes(data)
            result = self.run_qrb('--decode', path)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, b'')
        image = self.generate(b'VALID')
        result = self.run_qrb('--decode', image, '-o', self.root / 'missing' / 'out.txt')
        self.assertNotEqual(result.returncode, 0)
        if Path('/dev/full').exists():
            result = self.run_qrb('-t', 'VALID', '-s', 'pdf417', '-f', 'png', '-o', '/dev/full')
            self.assertNotEqual(result.returncode, 0)
            result = self.run_qrb('--decode', image, '-o', '/dev/full')
            self.assertNotEqual(result.returncode, 0)

    def test_existing_qr_can_be_decoded(self):
        path = self.root / 'qr.png'
        result = self.run_qrb('-t', 'https://example.com', '-s', 'qr', '-o', path)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = self.run_qrb('--decode', path)
        self.assertEqual(result.stdout, b'https://example.com')
        self.assertEqual(result.returncode, 0, result.stderr)

    @unittest.skipUnless(HAS_EXTERNAL, 'requires pillow, zxing-cpp, pdf417gen')
    def test_independent_decoding_of_qrb_output(self):
        for data in [b'  M1TEST/PASSENGER         ABC123  ', 'UTF-8 測試'.encode(), b'123456789' * 100]:
            path = self.generate(data)
            decoded = zxingcpp.read_barcode(Image.open(path))
            self.assertIsNotNone(decoded)
            self.assertEqual(decoded.bytes, data)
            self.assertEqual(decoded.format, zxingcpp.BarcodeFormat.PDF417)

    @unittest.skipUnless(HAS_EXTERNAL, 'requires pillow, zxing-cpp, pdf417gen')
    def test_external_png_jpeg_rotation_and_transparency(self):
        data = b'  M1TEST/USER          ABC123   '
        external = pdf417gen.render_image(pdf417gen.encode(data, columns=6), scale=3)
        images = [(external, 'external.png'), (external, 'external.jpg'),
                  (external.rotate(90, expand=True), 'rotated.png')]
        transparent = external.convert('RGBA')
        transparent.putdata([(*p[:3], 0 if p[:3] == (255, 255, 255) else 255)
                             for p in transparent.get_flattened_data()])
        images.append((transparent, 'transparent.png'))
        for image, name in images:
            with self.subTest(name=name):
                path = self.root / name; image.save(path)
                result = self.run_qrb('--decode', path)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, data)

    @unittest.skipUnless(HAS_EXTERNAL, 'requires pillow, zxing-cpp, pdf417gen')
    def test_blank_and_multiple_symbols(self):
        path = self.root / 'blank.png'
        Image.new('RGB', (300, 200), 'white').save(path)
        result = self.run_qrb('--decode', path)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b'')
        images = [pdf417gen.render_image(pdf417gen.encode(text), scale=3)
                  for text in [b'FIRST PAYLOAD', b'SECOND PAYLOAD']]
        canvas = Image.new('RGB', (max(i.width for i in images), sum(i.height for i in images) + 80), 'white')
        canvas.paste(images[0], (0, 0)); canvas.paste(images[1], (0, images[0].height + 80))
        canvas.save(path)
        result = self.run_qrb('--decode', path)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'Multiple', result.stderr)
        self.assertEqual(result.stdout, b'')


if __name__ == '__main__':
    unittest.main()
