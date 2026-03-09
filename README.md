# Indigo Imager Viewer

A command-line tool for previewing and converting astronomical image files. Supports FITS, XISF, TIFF, and RAW image formats.
Inspired by the image preview functionality of the [indigo_imager](https://github.com/indigo-astronomy/indigo_imager) project, rewritten without Qt dependencies and with extended output capabilities.

Original components are subject to the [INDIGO Astronomy open-source license (version 2.0, December 2020)](https://github.com/indigo-astronomy/indigo_imager/blob/master/LICENSE.md). Modifications and additions by UMa Technology are licensed under the [GNU Affero General Public License v3.0 (AGPL-3.0)](https://www.gnu.org/licenses/agpl-3.0.html). Any use of this software, including deployment as a network service, requires that the complete corresponding source code be made publicly available under the same license.

## 1. Features

- JPEG preview output
- Image header information extraction（JSON output）

## 2. Build Instructions

1. Clone the repository:
```bash
git clone <repository-url>
cd indigo_imager_preview_cli
```

2. Install dependencies:
```bash
sudo apt-get install libjpeg-dev liblz4-dev libtiff-dev
```

3. Build the application:
```bash
make
```

After a successful build, the executable is located at `bin/viewer`.

## 3. Usage

- JPEG preview files are generated in the specified output directory with the same base name as the input file and a `.jpeg` extension.
- If no output path is specified, files are written to `/tmp/viewer/jpeg` by default.

Basic usage:
```bash
./bin/viewer [options] <image_absolute_path>

# Options that take an argument (optional)
./viewer -s 3        # stretch level = 3
./viewer -b 1        # balance mode = 1
./viewer -q 80       # JPEG quality = 80
./viewer -p /tmp     # output path = /tmp

# Flag options (no argument required)
./viewer -d          # enable debug output
./viewer -i          # display image header info
./viewer -h          # show help

# Combined usage
./viewer -d -s 3 -q 90 -p /tmp image.fits
```

Synopsis:

`[-s stretch] [-b balance] [-d debug] [-q quality] [-i picture header info] [-p output_jpeg_path] <image_file>`

Example:
```bash
./bin/viewer -s 3 -q 80 -b 0 -d 0 image.fits
```

## 4. Notes

This program supports Linux only.
