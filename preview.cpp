// Copyright (c) 2021 Rumen G.Bogdanovski & David Hulse
// All rights reserved.
//
// You can use this software under the terms of 'INDIGO Astronomy
// open-source license' (see LICENSE.md).
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS 'AS IS' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Current Maintainer / Author
// UMa Technology
// Contact: larry.ji@umainf.com
// Since: 2026-03-09
// Version 1.0.0

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "preview.h"

#include "fits.h"
#include "raw.h"
#include "tiff.h"
#include "xisf.h"

#include <tiffio.h>
#ifdef __cplusplus
#include <filesystem>
#include <tiffio.hxx>
#endif

extern "C" {
#include <jpeglib.h>
}

bool preview_image::save(const char *filename, int quality) {
  ensure_image_data_allocated();

  try {
    std::filesystem::path file_path(filename);
    if (file_path.has_parent_path()) {
      std::filesystem::create_directories(file_path.parent_path());
    }
  } catch (const std::filesystem::filesystem_error &e) {
    log_message("Failed to create directory: %s", e.what());
  }

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE *outfile;
  JSAMPROW row_pointer[1];
  int row_stride;

  if ((outfile = fopen(filename, "wb")) == NULL) {
    log_message("Cannot open output file: %s", filename);
    return false;
  }

  cinfo.err = jpeg_std_error(&jerr);

  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, outfile);

  cinfo.image_width = preview_width;
  cinfo.image_height = preview_height;
  cinfo.input_components = preview_channels;
  cinfo.in_color_space = (preview_channels == 1) ? JCS_GRAYSCALE : JCS_RGB;

  jpeg_set_defaults(&cinfo);

  jpeg_set_quality(&cinfo, quality, TRUE);

  jpeg_start_compress(&cinfo, TRUE);

  row_stride = preview_width * preview_channels;

  while (cinfo.next_scanline < cinfo.image_height) {
    unsigned char *row_data = new unsigned char[row_stride];
    int y = cinfo.next_scanline;

    for (int x = 0; x < preview_width; x++) {
      int src_idx = (y * preview_width + x) * preview_channels;
      int dst_idx = x * preview_channels;

      for (int c = 0; c < preview_channels; c++) {
        row_data[dst_idx + c] = image_data[src_idx + c];
      }
    }

    row_pointer[0] = row_data;
    jpeg_write_scanlines(&cinfo, row_pointer, 1);

    delete[] row_data;
  }

  jpeg_finish_compress(&cinfo);

  jpeg_destroy_compress(&cinfo);
  fclose(outfile);

  std::thread cleanup_thread([filename]() {
    try {
      std::filesystem::path file_path(filename);
      if (file_path.has_parent_path()) {
        std::string dir_path = file_path.parent_path().string();

        size_t jpeg_count = 0;
        for (const auto &entry : std::filesystem::directory_iterator(dir_path)) {
          if (entry.is_regular_file() && entry.path().extension() == ".jpeg") {
            jpeg_count++;
          }
        }

        if (jpeg_count > JPEG_MAX_FILES) {
          std::vector<std::pair<std::string, std::filesystem::file_time_type>> files;
          files.reserve(jpeg_count);

          for (const auto &entry : std::filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".jpeg") {
              files.emplace_back(entry.path().string(), entry.last_write_time());
            }
          }

          std::sort(files.begin(), files.end(),
                    [](const auto &a, const auto &b) {
                      return a.second < b.second;
                    });

          const size_t batch_size = JPEG_DELETE_COUNT;
          size_t files_to_remove = std::min(batch_size, files.size());

          for (size_t i = 0; i < files_to_remove; i++) {
            try {
              std::filesystem::remove(files[i].first);
              log_message("Async cleanup: removed %s", files[i].first.c_str());
            } catch (const std::filesystem::filesystem_error &e) {
              log_message("Async cleanup failed to remove %s: %s",
                          files[i].first.c_str(), e.what());
            }
          }

          log_message("Async cleanup completed: removed %zu files (batch delete)", files_to_remove);
        }
      }
    } catch (const std::exception &e) {
      log_message("Async cleanup error: %s", e.what());
    }
  });

  cleanup_thread.detach();

  return true;
}

int get_bayer_offsets(uint32_t pix_format) {
  switch (pix_format) {
    case PIX_FMT_SBGGR8:
    case PIX_FMT_SBGGR12:
    case PIX_FMT_SBGGR16:
    case PIX_FMT_SBGGR32:
    case PIX_FMT_SBGGRF:
      return 0x11;

    case PIX_FMT_SGBRG8:
    case PIX_FMT_SGBRG12:
    case PIX_FMT_SGBRG16:
    case PIX_FMT_SGBRG32:
    case PIX_FMT_SGBRGF:
      return 0x01;

    case PIX_FMT_SGRBG8:
    case PIX_FMT_SGRBG12:
    case PIX_FMT_SGRBG16:
    case PIX_FMT_SGRBG32:
    case PIX_FMT_SGRBGF:
      return 0x10;

    case PIX_FMT_SRGGB8:
    case PIX_FMT_SRGGB12:
    case PIX_FMT_SRGGB16:
    case PIX_FMT_SRGGB32:
    case PIX_FMT_SRGGBF:
      return 0x00;

    default:
      return -1;
  }
}

static unsigned int bayer_to_pix_format(const char *image_bayer_pat, const int bitpix, uint32_t prefered_bayer_pat) {
  char bayerpat[5] = {0};

  if (prefered_bayer_pat == BAYER_PAT_AUTO || prefered_bayer_pat == 0) {
    memcpy(bayerpat, image_bayer_pat, 4);
  } else {
    memcpy(bayerpat, &prefered_bayer_pat, 4);
  }
  bayerpat[4] = '\0';

  if ((!strcmp(bayerpat, "BGGR") || !strcmp(bayerpat, "BGRG")) && (bitpix == 8)) {
    return PIX_FMT_SBGGR8;
  } else if ((!strcmp(bayerpat, "GBRG") || !strcmp(bayerpat, "GBGR")) && (bitpix == 8)) {
    return PIX_FMT_SGBRG8;
  } else if ((!strcmp(bayerpat, "GRBG") || !strcmp(bayerpat, "GRGB")) && (bitpix == 8)) {
    return PIX_FMT_SGRBG8;
  } else if ((!strcmp(bayerpat, "RGGB") || !strcmp(bayerpat, "RGBG")) && (bitpix == 8)) {
    return PIX_FMT_SRGGB8;
  } else if ((!strcmp(bayerpat, "BGGR") || !strcmp(bayerpat, "BGRG")) && (bitpix == 16)) {
    return PIX_FMT_SBGGR16;
  } else if ((!strcmp(bayerpat, "GBRG") || !strcmp(bayerpat, "GBGR")) && (bitpix == 16)) {
    return PIX_FMT_SGBRG16;
  } else if ((!strcmp(bayerpat, "GRBG") || !strcmp(bayerpat, "GRGB")) && (bitpix == 16)) {
    return PIX_FMT_SGRBG16;
  } else if ((!strcmp(bayerpat, "RGGB") || !strcmp(bayerpat, "RGBG")) && (bitpix == 16)) {
    return PIX_FMT_SRGGB16;
  } else if ((!strcmp(bayerpat, "BGGR") || !strcmp(bayerpat, "BGRG")) && (bitpix == 32)) {
    return PIX_FMT_SBGGR32;
  } else if ((!strcmp(bayerpat, "GBRG") || !strcmp(bayerpat, "GBGR")) && (bitpix == 32)) {
    return PIX_FMT_SGBRG32;
  } else if ((!strcmp(bayerpat, "GRBG") || !strcmp(bayerpat, "GRGB")) && (bitpix == 32)) {
    return PIX_FMT_SGRBG32;
  } else if ((!strcmp(bayerpat, "RGGB") || !strcmp(bayerpat, "RGBG")) && (bitpix == 32)) {
    return PIX_FMT_SRGGB32;
  } else if ((!strcmp(bayerpat, "BGGR") || !strcmp(bayerpat, "BGRG")) && (bitpix == -32)) {
    return PIX_FMT_SBGGRF;
  } else if ((!strcmp(bayerpat, "GBRG") || !strcmp(bayerpat, "GBGR")) && (bitpix == -32)) {
    return PIX_FMT_SGBRGF;
  } else if ((!strcmp(bayerpat, "GRBG") || !strcmp(bayerpat, "GRGB")) && (bitpix == -32)) {
    return PIX_FMT_SGRBGF;
  } else if ((!strcmp(bayerpat, "RGGB") || !strcmp(bayerpat, "RGBG")) && (bitpix == -32)) {
    return PIX_FMT_SRGGBF;
  }
  return 0;
}

preview_image *create_tiff_preview(unsigned char *tiff_data, unsigned long tiff_size, const stretch_config_t sconfig, char *image_info) {
  struct tiff_mem_stream {
    unsigned char *data;
    tmsize_t size;
    tmsize_t pos;
  };

  tiff_mem_stream *mem = new tiff_mem_stream{tiff_data, (tmsize_t)tiff_size, 0};
  tiff_metadata metadata;
  tiff_metadata_init(&metadata);

  // Define the I/O functions for memory stream
  auto read_proc = [](thandle_t handle, void *buf, tmsize_t size) -> tmsize_t {
    tiff_mem_stream *stream = (tiff_mem_stream *)handle;
    if (stream->pos + size > stream->size) {
      size = stream->size - stream->pos;
    }
    if (size > 0) {
      memcpy(buf, stream->data + stream->pos, size);
      stream->pos += size;
    } else {
      log_message("TIFF: Failed to read data, size: %ld", size);
    }
    return size;
  };

  auto write_proc = [](thandle_t handle, void *buf, tmsize_t size) -> tmsize_t {
    (void)handle;
    (void)buf;
    (void)size;
    return 0;
  };

  auto seek_proc = [](thandle_t handle, toff_t off, int whence) -> toff_t {
    tiff_mem_stream *stream = (tiff_mem_stream *)handle;
    switch (whence) {
      case SEEK_SET:
        stream->pos = off;
        break;
      case SEEK_CUR:
        stream->pos += off;
        break;
      case SEEK_END:
        stream->pos = stream->size + off;
        break;
    }
    return stream->pos;
  };

  auto close_proc = [](thandle_t handle) -> int {
    (void)handle;
    return 0;
  };

  auto size_proc = [](thandle_t handle) -> toff_t {
    return ((tiff_mem_stream *)handle)->size;
  };

  auto map_proc = [](thandle_t, void **base, toff_t *size) -> int {
    (void)base;
    (void)size;
    return 0;
  };

  auto unmap_proc = [](thandle_t, void *base, toff_t size) {
    (void)base;
    (void)size;
  };

  TIFF *tiff = TIFFClientOpen(
      "MemoryTIFF", "r", (thandle_t)mem,
      read_proc, write_proc, seek_proc, close_proc, size_proc, map_proc, unmap_proc);

  if (!tiff) {
    log_message("TIFF: Failed to open memory stream");
    delete mem;
    return nullptr;
  }

  uint32_t width = 0, height = 0;
  uint16_t bitspersample = 8;
  uint16_t sampleformat = SAMPLEFORMAT_UINT;
  uint16_t photometric = PHOTOMETRIC_MINISBLACK;

  if (!TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width) ||
      !TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height)) {
    log_message("TIFF: Failed to read image dimensions");
    TIFFClose(tiff);
    delete mem;
    return nullptr;
  }

  TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitspersample);
  TIFFGetField(tiff, TIFFTAG_SAMPLEFORMAT, &sampleformat);
  TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photometric);

  char *image_desc = nullptr;
  if (TIFFGetField(tiff, TIFFTAG_IMAGEDESCRIPTION, &image_desc) && image_desc) {
    tiff_parse_image_description(&metadata, image_desc);
  }

  metadata.width = width;
  metadata.height = height;
  metadata.bitspersample = bitspersample;
  metadata.sampleformat = sampleformat;
  metadata.photometric = photometric;

  log_message("TIFF parameters: %dx%d %d-bit %s format",
              width, height, bitspersample,
              photometric == PHOTOMETRIC_RGB ? "RGB" : "MONO");

  tsize_t scanline = TIFFScanlineSize(tiff);
  tdata_t scan_buf = _TIFFmalloc(scanline * height);
  if (!scan_buf) {
    log_message("TIFF: Memory allocation failed");
    TIFFClose(tiff);
    delete mem;
    return nullptr;
  }

  for (uint32_t row = 0; row < height; row++) {
    if (TIFFReadScanline(tiff, (char *)scan_buf + row * scanline, row) < 0) {
      log_message("TIFF: Failed to read scanline %d", row);
      _TIFFfree(scan_buf);
      TIFFClose(tiff);
      delete mem;
      return nullptr;
    }
  }

  uint8_t *buffer = (uint8_t *)scan_buf;
  log_message("Raw data header bytes: %02X %02X %02X %02X %02X %02X",
              buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);

  uint16_t fillorder;
  TIFFGetField(tiff, TIFFTAG_FILLORDER, &fillorder);
  // bool swapBytes = (fillorder != HOST_FILLORDER);
  metadata.fillorder = fillorder;

  uint16_t extrasamples = 0;
  uint16_t *extrasamples_types = nullptr;
  if (!TIFFGetField(tiff, TIFFTAG_EXTRASAMPLES, &extrasamples, &extrasamples_types)) {
    extrasamples = 0;
  }

  log_message("extrasamples: %d", extrasamples);

  get_tiff_header(&metadata, image_info);
  if (image_info) {
    _TIFFfree(scan_buf);
    TIFFClose(tiff);
    delete mem;
    return nullptr;
  }

  char *image_data = nullptr;
  unsigned int pix_format = PIX_FMT_RGB24;
  const uint32_t pixel_count = width * height;

  if (photometric == PHOTOMETRIC_RGB) {
    if (bitspersample == 16) {
      pix_format = PIX_FMT_RGB48;
      const int bytes_per_pixel = 3 * sizeof(uint16_t);
      const int valid_bytes_per_row = width * bytes_per_pixel;
      const int padding_bytes = scanline - (3 + extrasamples) * width * sizeof(uint16_t);

      image_data = (char *)malloc(height * valid_bytes_per_row);
      uint16_t *src = (uint16_t *)scan_buf;

      log_message("Processing 16-bit RGB data, valid bytes per row: %d, padding: %d, scanline:%d",
                  valid_bytes_per_row, padding_bytes, scanline);

      for (uint32_t row = 0; row < height; row++) {
        uint16_t *row_src = src + (row * (scanline / sizeof(uint16_t)));
        uint16_t *row_dst = (uint16_t *)image_data + row * width * 3;

        for (uint32_t col = 0; col < width; col++) {
          row_dst[col * 3] = row_src[col * (3 + extrasamples)];         // R
          row_dst[col * 3 + 1] = row_src[col * (3 + extrasamples) + 1]; // G
          row_dst[col * 3 + 2] = row_src[col * (3 + extrasamples) + 2]; // B
        }
      }
    } else {
      const int bytes_per_pixel = 3;
      const int valid_bytes_per_row = width * bytes_per_pixel;
      const int padding_bytes = scanline - (3 + extrasamples) * width;

      image_data = (char *)malloc(height * valid_bytes_per_row);
      uint8_t *src = (uint8_t *)scan_buf;

      log_message("Processing 8-bit RGB data, valid bytes per row: %d, padding: %d, scanline:%d",
                  valid_bytes_per_row, padding_bytes, scanline);

      for (uint32_t row = 0; row < height; row++) {
        uint8_t *row_src = src + row * scanline;
        uint8_t *row_dst = (uint8_t *)image_data + row * valid_bytes_per_row;

        for (uint32_t col = 0; col < width; col++) {
          row_dst[col * 3] = row_src[col * (3 + extrasamples)];         // R
          row_dst[col * 3 + 1] = row_src[col * (3 + extrasamples) + 1]; // G
          row_dst[col * 3 + 2] = row_src[col * (3 + extrasamples) + 2]; // B
        }
      }
    }
  } else { // Handle grayscale images
    const int actual_channels = 1 + extrasamples;
    const int bytes_per_pixel = (bitspersample == 16) ? actual_channels * sizeof(uint16_t) : actual_channels * sizeof(uint8_t);
    const int valid_bytes_per_row = width * bytes_per_pixel;
    const int padding_bytes = scanline - valid_bytes_per_row;

    log_message("Processing %d-bit GRAYSCALE with %d extra channels, valid bytes per row: %d, padding: %d, scanline:%d",
                bitspersample, extrasamples, valid_bytes_per_row, padding_bytes, scanline);

    image_data = (char *)malloc(width * height * (bitspersample == 16 ? 2 : 1));
    pix_format = (bitspersample == 16) ? PIX_FMT_Y16 : PIX_FMT_Y8;

    if (bitspersample == 16) {
      uint16_t *src = (uint16_t *)scan_buf;
      for (uint32_t row = 0; row < height; row++) {
        uint16_t *row_src = src + row * (scanline / sizeof(uint16_t));
        uint16_t *row_dst = (uint16_t *)image_data + row * width;

        for (uint32_t col = 0; col < width; col++) {
          row_dst[col] = row_src[col * actual_channels];
        }
      }
    } else {
      uint8_t *src = (uint8_t *)scan_buf;
      for (uint32_t row = 0; row < height; row++) {
        uint8_t *row_src = src + row * scanline;
        uint8_t *row_dst = (uint8_t *)image_data + row * width;

        for (uint32_t col = 0; col < width; col++) {
          row_dst[col] = row_src[col * actual_channels];
        }
      }
    }
  }

  // For single-channel TIFF, check for Bayer pattern (same as FITS/XISF path)
  if (photometric != PHOTOMETRIC_RGB) {
    int bitpix;
    if (sampleformat == SAMPLEFORMAT_IEEEFP) {
      bitpix = -32;
    } else if (bitspersample == 32) {
      bitpix = 32;
    } else if (bitspersample == 16) {
      bitpix = 16;
    } else {
      bitpix = 8;
    }
    int bayer_pix_fmt = bayer_to_pix_format(metadata.bayerpat, bitpix, sconfig.bayer_pattern);
    if (bayer_pix_fmt != 0) {
      pix_format = bayer_pix_fmt;
      log_message("TIFF: Bayer pattern detected (%s), pix_format updated", metadata.bayerpat);
    }
  }

  _TIFFfree(scan_buf);
  TIFFClose(tiff);
  delete mem;

  // Debug: show processed header pixels
  if (image_data && pixel_count > 0) {
    log_message("Processed header pixels: R:%02X G:%02X B:%02X",
                (uint8_t)image_data[0], (uint8_t)image_data[1], (uint8_t)image_data[2]);
  }

  preview_image *img = create_preview(width, height, pix_format, image_data, sconfig);
  free(image_data);
  return img;
}

preview_image *create_fits_preview(unsigned char *raw_fits_buffer, unsigned long fits_size, const stretch_config_t sconfig, char *image_info) {
  fits_header header;
  memset(&header, 0, sizeof(header));
  unsigned int pix_format = 0;

  int res = fits_read_header(raw_fits_buffer, fits_size, &header);
  if (res != FITS_OK) {
    return nullptr;
  }
  get_fits_header(&header, image_info);
  if (image_info) {
    return nullptr;
  }

  if ((header.bitpix == -32) && (header.naxis == 2)) {
    pix_format = PIX_FMT_F32;
  } else if ((header.bitpix == 32) && (header.naxis == 2)) {
    pix_format = PIX_FMT_Y32;
  } else if ((header.bitpix == 16) && (header.naxis == 2)) {
    pix_format = PIX_FMT_Y16;
  } else if ((header.bitpix == 8) && (header.naxis == 2)) {
    pix_format = PIX_FMT_Y8;
  } else if ((header.bitpix == -32) && (header.naxis == 3)) {
    pix_format = PIX_FMT_3RGBF;
  } else if ((header.bitpix == 32) && (header.naxis == 3)) {
    pix_format = PIX_FMT_3RGB96;
  } else if ((header.bitpix == 16) && (header.naxis == 3)) {
    pix_format = PIX_FMT_3RGB48;
  } else if ((header.bitpix == 8) && (header.naxis == 3)) {
    pix_format = PIX_FMT_3RGB24;
  } else {
    return nullptr;
  }

  char *fits_data = (char *)malloc(fits_get_buffer_size(&header));

  res = fits_process_data(raw_fits_buffer, fits_size, &header, fits_data);
  if (res != FITS_OK) {
    log_message("FITS: Error processing data");
    free(fits_data);
    return nullptr;
  }

  if (header.naxis == 2) {
    int bayer_pix_fmt = bayer_to_pix_format(header.bayerpat, header.bitpix, sconfig.bayer_pattern);
    if (bayer_pix_fmt != 0)
      pix_format = bayer_pix_fmt;
  }

  preview_image *img = create_preview(header.naxisn[0], header.naxisn[1],
                                      pix_format, fits_data, sconfig);

  log_message("FITS_END: fits_data = %p", fits_data);
  free(fits_data);
  return img;
}

preview_image *create_xisf_preview(unsigned char *xisf_buffer, unsigned long xisf_size, const stretch_config_t sconfig, char *image_info) {
  xisf_metadata *header = (xisf_metadata *)calloc(1, sizeof(xisf_metadata));
  if (!header) {
    log_message("XISF: Failed to allocate metadata");
    return nullptr;
  }
  unsigned int pix_format = 0;
  preview_image *img = nullptr;

  log_message("XISF_START");
  int res = xisf_read_metadata(xisf_buffer, xisf_size, header);
  if (res != XISF_OK) {
    log_message("XISF: Error parsing header res=%d", res);
    free(header);
    return nullptr;
  }

  xisf_get_header(header, image_info);
  if (image_info) {
    free(header);
    return NULL;
  }

  if ((header->bitpix == -32) && (header->channels == 1)) {
    pix_format = PIX_FMT_F32;
  } else if ((header->bitpix == 32) && (header->channels == 1)) {
    pix_format = PIX_FMT_Y32;
  } else if ((header->bitpix == 16) && (header->channels == 1)) {
    pix_format = PIX_FMT_Y16;
  } else if ((header->bitpix == 8) && (header->channels == 1)) {
    pix_format = PIX_FMT_Y8;
  } else if ((header->bitpix == -32) && (header->channels == 3)) {
    pix_format = (header->normal_pixel_storage) ? PIX_FMT_RGBF : PIX_FMT_3RGBF;
  } else if ((header->bitpix == 32) && (header->channels == 3)) {
    pix_format = (header->normal_pixel_storage) ? PIX_FMT_RGB96 : PIX_FMT_3RGB96;
  } else if ((header->bitpix == 16) && (header->channels == 3)) {
    pix_format = (header->normal_pixel_storage) ? PIX_FMT_RGB48 : PIX_FMT_3RGB48;
  } else if ((header->bitpix == 8) && (header->channels == 3)) {
    pix_format = (header->normal_pixel_storage) ? PIX_FMT_RGB24 : PIX_FMT_3RGB24;
  } else {
    log_message("XISF: Unsupported bitpix (BITPIX= %d)", header->bitpix);
    free(header);
    return nullptr;
  }

  if (header->compression[0] == '\0') {
    log_message("XISF: file_size = %d, required_size = %d", xisf_size, header->data_offset + header->data_size);
    if ((unsigned long)(header->data_offset + header->data_size) > xisf_size) {
      log_message("XISF: Wrong size (file_size = %d, required_size = %d)", xisf_size, header->data_offset + header->data_size);
      free(header);
      return nullptr;
    }

    if (header->channels == 1) {
      int bayer_pix_fmt = bayer_to_pix_format(header->bayer_pattern, header->bitpix, sconfig.bayer_pattern);
      if (bayer_pix_fmt != 0)
        pix_format = bayer_pix_fmt;
    }

    img = create_preview(header->width, header->height, pix_format, (char *)xisf_buffer + header->data_offset, sconfig);
  } else {
    char *xisf_data = (char *)malloc(header->uncompressed_data_size);
    int res = xisf_decompress(xisf_buffer, header, (uint8_t *)xisf_data);
    if (res != XISF_OK) {
      log_message("XISF: Decompression failed res = %d", res);
      free(xisf_data);
      free(header);
      return nullptr;
    }

    if (header->channels == 1) {
      int bayer_pix_fmt = bayer_to_pix_format(header->bayer_pattern, header->bitpix, sconfig.bayer_pattern);
      if (bayer_pix_fmt != 0)
        pix_format = bayer_pix_fmt;
    }

    img = create_preview(header->width, header->height, pix_format, (char *)xisf_data, sconfig);
    free(xisf_data);
  }
  log_message("XISF_END");
  free(header);
  return img;
}

static int raw_read_keyword_value(const char *ptr8, char *keyword, char *value) {
  int i;
  int length = strlen(ptr8);

  for (i = 0; i < 8 && ptr8[i] != ' '; i++) {
    keyword[i] = ptr8[i];
  }
  keyword[i] = '\0';

  if (ptr8[8] == '=') {
    i++;
    while (i < length && ptr8[i] == ' ') {
      i++;
    }

    if (i < length) {
      *value++ = ptr8[i];
      i++;
      if (ptr8[i - 1] == '\'') {
        for (; i < length && ptr8[i] != '\''; i++) {
          *value++ = ptr8[i];
        }
        *value++ = '\'';
      } else if (ptr8[i - 1] == '(') {
        for (; i < length && ptr8[i] != ')'; i++) {
          *value++ = ptr8[i];
        }
        *value++ = ')';
      } else {
        for (; i < length && ptr8[i] != ' ' && ptr8[i] != '/'; i++) {
          *value++ = ptr8[i];
        }
      }
    }
  }
  *value = '\0';
  return 0;
}

preview_image *create_raw_preview(unsigned char *raw_image_buffer, unsigned long raw_size, const stretch_config_t sconfig, char *image_info) {
  unsigned int pix_format = 0;

  if (sizeof(raw_header) > raw_size) {
    log_message("RAW: Image buffer is too short: can not fit the header (%dB)", raw_size);
    return nullptr;
  }

  log_message("RAW_START");
  raw_header *header = (raw_header *)raw_image_buffer;
  char *raw_data = (char *)raw_image_buffer + sizeof(raw_header);
  int bitpix = 0;

  switch (header->signature) {
    case RAW_MONO8:
      pix_format = PIX_FMT_Y8;
      bitpix = 8;
      break;
    case RAW_MONO16:
      pix_format = PIX_FMT_Y16;
      bitpix = 16;
      break;
    case RAW_RGB24:
      pix_format = PIX_FMT_RGB24;
      bitpix = 24;
      break;
    case RAW_RGB48:
      pix_format = PIX_FMT_RGB48;
      bitpix = 48;
      break;
    default:
      log_message("RAW: Unsupported image format (%d)", header->signature);
      return nullptr;
  }

  /* use embedded keywords */
  uint32_t embedded_bayer_pix_fmt = 0;
  const uint32_t pixel_count = header->width * header->height;
  int data_size = pixel_count * bitpix / 8;
  int extension_length = raw_size - data_size - sizeof(raw_header);
  log_message("RAW: extension_length = %d", extension_length);

  if (image_info) {
    const char *appendix_ptr = (extension_length > 9) ? (raw_data + data_size) : NULL;
    get_raw_header(header, appendix_ptr, extension_length, image_info);
    return NULL;
  }
  if (extension_length > 9) {
    char *extension = (char *)safe_malloc(extension_length + 1);
    char *extension_start = extension;
    strncpy(extension, raw_data + data_size, extension_length);
    extension[extension_length] = '\0';
    if (!strncmp(extension_start, "SIMPLE=T;", 9)) {
      extension_start += 9;
      extension_length -= 9;
      char *pos = NULL;
      while ((pos = strchr(extension_start, ';'))) {
        char keyword[80];
        char value[80];
        *pos = '\0';
        raw_read_keyword_value(extension_start, keyword, value);
        extension_start = pos + 1;
        if (!strcmp(keyword, "BAYERPAT")) {
          log_message("RAW: BAYERPAT = %s", value);
          char *value_start = value + 1;               // get rid of the start quote
          value_start[strlen(value_start) - 1] = '\0'; // get rid of the end quote
          embedded_bayer_pix_fmt = bayer_to_pix_format(value_start, bitpix, sconfig.bayer_pattern);
        }
      }
    }
    safe_free(extension);
  }

  if ((sconfig.bayer_pattern == BAYER_PAT_AUTO || sconfig.bayer_pattern == 0) && bitpix != 0 && embedded_bayer_pix_fmt != 0) {
    pix_format = embedded_bayer_pix_fmt;
  } else if (sconfig.bayer_pattern != BAYER_PAT_AUTO && sconfig.bayer_pattern != 0 && bitpix != 0) {
    int bayer_pix_fmt = bayer_to_pix_format(0, bitpix, sconfig.bayer_pattern);
    if (bayer_pix_fmt != 0)
      pix_format = bayer_pix_fmt;
  }

  preview_image *img = create_preview(header->width, header->height,
                                      pix_format, raw_data, sconfig);

  log_message("RAW_END: raw_data = %p", raw_data);
  return img;
}

int get_final_channels(int pix_format) {
  switch (pix_format) {
    case PIX_FMT_Y8:
    case PIX_FMT_Y16:
    case PIX_FMT_Y32:
    case PIX_FMT_F32:
      return 1;

    case PIX_FMT_RGB24:
    case PIX_FMT_RGB48:
    case PIX_FMT_RGB96:
    case PIX_FMT_RGBF:
    case PIX_FMT_3RGB24:
    case PIX_FMT_3RGB48:
    case PIX_FMT_3RGB96:
    case PIX_FMT_3RGBF:
    case PIX_FMT_SBGGR8:
    case PIX_FMT_SGBRG8:
    case PIX_FMT_SGRBG8:
    case PIX_FMT_SRGGB8:
    case PIX_FMT_SBGGR12:
    case PIX_FMT_SGBRG12:
    case PIX_FMT_SGRBG12:
    case PIX_FMT_SRGGB12:
    case PIX_FMT_SBGGR16:
    case PIX_FMT_SGBRG16:
    case PIX_FMT_SGRBG16:
    case PIX_FMT_SRGGB16:
    case PIX_FMT_SBGGR32:
    case PIX_FMT_SGBRG32:
    case PIX_FMT_SGRBG32:
    case PIX_FMT_SRGGB32:
    case PIX_FMT_SBGGRF:
    case PIX_FMT_SGBRGF:
    case PIX_FMT_SGRBGF:
    case PIX_FMT_SRGGBF:
      return 3;

    default:
      return 3;
  }
}

preview_image *create_preview(int width, int height, int pix_format, char *image_data, const stretch_config_t sconfig) {
  int channels = get_final_channels(pix_format);
  preview_image *img = new preview_image(width, height, channels);
  if (pix_format == PIX_FMT_Y8) {
    uint8_t *buf = (uint8_t *)image_data;
    uint8_t *pixmap_data = (uint8_t *)malloc(sizeof(uint8_t) * height * width);
    memcpy(pixmap_data, buf, sizeof(uint8_t) * height * width);
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_Y8;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_Y16) {
    uint16_t *buf = (uint16_t *)image_data;
    uint16_t *pixmap_data = (uint16_t *)malloc(sizeof(uint16_t) * height * width);
    memcpy(pixmap_data, buf, sizeof(uint16_t) * height * width);
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_Y16;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_Y32) {
    uint32_t *buf = (uint32_t *)image_data;
    uint32_t *pixmap_data = (uint32_t *)malloc(sizeof(uint32_t) * height * width);
    memcpy(pixmap_data, buf, sizeof(uint32_t) * height * width);
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_Y32;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_F32) {
    float *buf = (float *)image_data;
    float *pixmap_data = (float *)malloc(sizeof(float) * height * width);
    memcpy(pixmap_data, buf, sizeof(float) * height * width);
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_F32;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_3RGB24) {
    int channel_offest = width * height;
    uint8_t *buf = (uint8_t *)image_data;
    uint8_t *pixmap_data = (uint8_t *)malloc(sizeof(uint8_t) * height * width * 3);
    int index = 0;
    int index2 = 0;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        pixmap_data[index2 + 0] = buf[index];
        pixmap_data[index2 + 1] = buf[index + channel_offest];
        pixmap_data[index2 + 2] = buf[index + 2 * channel_offest];
        index++;
        index2 += 3;
      }
    }
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_RGB24;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_3RGB48) {
    int channel_offest = width * height;
    uint16_t *buf = (uint16_t *)image_data;
    uint16_t *pixmap_data = (uint16_t *)malloc(sizeof(uint16_t) * height * width * 3);
    int index = 0;
    int index2 = 0;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        pixmap_data[index2 + 0] = buf[index];
        pixmap_data[index2 + 1] = buf[index + channel_offest];
        pixmap_data[index2 + 2] = buf[index + 2 * channel_offest];
        index++;
        index2 += 3;
      }
    }
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_RGB48;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_3RGB96) {
    int channel_offest = width * height;
    uint32_t *buf = (uint32_t *)image_data;
    uint32_t *pixmap_data = (uint32_t *)malloc(sizeof(uint32_t) * height * width * 3);
    int index = 0;
    int index2 = 0;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        pixmap_data[index2 + 0] = buf[index];
        pixmap_data[index2 + 1] = buf[index + channel_offest];
        pixmap_data[index2 + 2] = buf[index + 2 * channel_offest];
        index++;
        index2 += 3;
      }
    }
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_RGB96;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_3RGBF) {
    int channel_offest = width * height;
    float *buf = (float *)image_data;
    float *pixmap_data = (float *)malloc(sizeof(float) * height * width * 3);
    int index = 0;
    int index2 = 0;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        pixmap_data[index2 + 0] = buf[index];
        pixmap_data[index2 + 1] = buf[index + channel_offest];
        pixmap_data[index2 + 2] = buf[index + 2 * channel_offest];
        index++;
        index2 += 3;
      }
    }
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_RGBF;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_RGB24) {
    uint8_t *buf = (uint8_t *)image_data;
    uint8_t *pixmap_data = (uint8_t *)malloc(sizeof(uint8_t) * height * width * 3);
    memcpy(pixmap_data, buf, sizeof(uint8_t) * height * width * 3);
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_RGB24;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_RGB48) {
    uint16_t *buf = (uint16_t *)image_data;
    uint16_t *pixmap_data = (uint16_t *)malloc(sizeof(uint16_t) * height * width * 3);
    memcpy(pixmap_data, buf, sizeof(uint16_t) * height * width * 3);
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_RGB48;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_RGB96) {
    uint32_t *buf = (uint32_t *)image_data;
    uint32_t *pixmap_data = (uint32_t *)malloc(sizeof(uint32_t) * height * width * 3);
    memcpy(pixmap_data, buf, sizeof(uint32_t) * height * width * 3);
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_RGB96;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if (pix_format == PIX_FMT_RGBF) {
    float *buf = (float *)image_data;
    float *pixmap_data = (float *)malloc(sizeof(float) * height * width * 3);
    memcpy(pixmap_data, buf, sizeof(float) * height * width * 3);
    img->m_raw_data = (char *)pixmap_data;
    img->m_pix_format = PIX_FMT_RGBF;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if ((pix_format == PIX_FMT_SBGGR8) || (pix_format == PIX_FMT_SGBRG8) ||
             (pix_format == PIX_FMT_SGRBG8) || (pix_format == PIX_FMT_SRGGB8)) {
    uint8_t *rgb_data = (uint8_t *)malloc(width * height * 3);
    parallel_debayer((uint8_t *)image_data, width, height, get_bayer_offsets(pix_format), rgb_data);

    img->m_raw_data = (char *)rgb_data;
    img->m_pix_format = PIX_FMT_RGB24;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if ((pix_format == PIX_FMT_SBGGR16) || (pix_format == PIX_FMT_SGBRG16) ||
             (pix_format == PIX_FMT_SGRBG16) || (pix_format == PIX_FMT_SRGGB16)) {
    uint16_t *rgb_data = (uint16_t *)malloc(width * height * 6);
    parallel_debayer((uint16_t *)image_data, width, height, get_bayer_offsets(pix_format), rgb_data);

    img->m_raw_data = (char *)rgb_data;
    img->m_pix_format = PIX_FMT_RGB48;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if ((pix_format == PIX_FMT_SBGGR32) || (pix_format == PIX_FMT_SGBRG32) ||
             (pix_format == PIX_FMT_SGRBG32) || (pix_format == PIX_FMT_SRGGB32)) {
    uint32_t *rgb_data = (uint32_t *)malloc(width * height * 12);
    parallel_debayer((uint32_t *)image_data, width, height, get_bayer_offsets(pix_format), rgb_data);

    img->m_raw_data = (char *)rgb_data;
    img->m_pix_format = PIX_FMT_RGB96;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else if ((pix_format == PIX_FMT_SBGGRF) || (pix_format == PIX_FMT_SGBRGF) ||
             (pix_format == PIX_FMT_SGRBGF) || (pix_format == PIX_FMT_SRGGBF)) {
    float *rgb_data = (float *)malloc(width * height * 12);
    parallel_debayer((float *)image_data, width, height, get_bayer_offsets(pix_format), rgb_data);

    img->m_raw_data = (char *)rgb_data;
    img->m_pix_format = PIX_FMT_RGBF;
    img->m_height = height;
    img->m_width = width;

    stretch_preview(img, sconfig);
  } else {
    char *c = (char *)&pix_format;
    log_message("%s(): Unsupported pixel format (%c%c%c%c)", __FUNCTION__, c[0], c[1], c[2], c[3]);
    delete img;
    img = nullptr;
  }
  return img;
}

void stretch_preview(preview_image *img, const stretch_config_t sconfig) {
  if (
      img->m_pix_format == PIX_FMT_Y8 ||
      img->m_pix_format == PIX_FMT_Y16 ||
      img->m_pix_format == PIX_FMT_Y32 ||
      img->m_pix_format == PIX_FMT_F32 ||
      img->m_pix_format == PIX_FMT_RGB24 ||
      img->m_pix_format == PIX_FMT_RGB48 ||
      img->m_pix_format == PIX_FMT_RGB96 ||
      img->m_pix_format == PIX_FMT_RGBF) {
    Stretcher s(img->m_width, img->m_height, img->m_pix_format);
    StretchParams sp;
    sp.grey_red.highlights = sp.green.highlights = sp.blue.highlights = 0.9;
    if (sconfig.stretch_level > 0) {
      sp = s.computeParams((const uint8_t *)img->m_raw_data, stretch_params_lut[sconfig.stretch_level].brightness, stretch_params_lut[sconfig.stretch_level].contrast);
    }
    log_message("Stretch level: %d, params %f %f %f\n", sconfig.stretch_level, sp.grey_red.shadows, sp.grey_red.midtones, sp.grey_red.highlights);

    if (sconfig.balance) {
      sp.refChannel = &sp.green;
    }
    s.setParams(sp);
    s.stretch((const uint8_t *)img->m_raw_data, img, 1);
  } else {
    char *c = (char *)&img->m_pix_format;
    log_message("%s(): Unsupported pixel format (%c%c%c%c)", __FUNCTION__, c[0], c[1], c[2], c[3]);
  }
}

preview_image *main_create_preview(unsigned char *data, size_t size, const char *format, const stretch_config_t sconfig, char *image_info) {
  preview_image *preview = nullptr;

  if (data != NULL && format != NULL) {
    char header_prefix[11];
    for (int i = 0; i < 10; i++) {
      header_prefix[i] = *(data + i);
    }
    header_prefix[10] = '\0';

    log_message("First 10 characters: %s\n", header_prefix);

    if (!strncmp((const char *)data, "SIMPLE", 6)) {
      preview = create_fits_preview(data, size, sconfig, image_info);
    } else if ((data[0] == 0x49 && data[1] == 0x49) || // II
               (data[0] == 0x4D && data[1] == 0x4D)) { // MM
      preview = create_tiff_preview(data, size, sconfig, image_info);
    } else if (!strncmp((const char *)data, "XISF0100", 8)) {
      preview = create_xisf_preview(data, size, sconfig, image_info);
    } else if (!strncmp((const char *)data, "RAW", 3)) {
      preview = create_raw_preview(data, size, sconfig, image_info);
    }
  } else {
    log_message("data or format is NULL");
  }

  return preview;
}