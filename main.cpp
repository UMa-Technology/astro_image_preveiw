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

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "common.h"
#include "preview.h"

using namespace std;

void print_usage() {
  printf("Viewer Program v%s\n", VERSION);
  printf("Usage: viewer [-s stretch level] [-b balance mode] [-d] [-q quality] [-i metadata] [-p path JPEG save path] filename\n");
  printf("  -s stretch: stretch level (0-4), default: %d\n", STRETCH_NORMAL);
  printf("  -b balance: color balance (0 AUTO, 1 NONE), default: %d\n", CB_AUTO);
  printf("  -q quality: JPEG quality (10-100), default: %d\n", JPEG_DEFAULT_QUALITY);
  printf("  -i display image info\n");
  printf("  -p path: JPEG save path, default: %s\n", JPEG_SAVE_PATH);
  printf("  -h show this help\n");
}

static int parse_arguments(int argc, char *argv[], conf_t *conf, int *quality, int *display_info, char *dst_path, char **file_name) {
  int opt;

  memset(conf, 0, sizeof(conf_t));
  conf->preview_stretch_level = STRETCH_NORMAL;
  conf->preview_color_balance = CB_AUTO;
  conf->preview_bayer_pattern = 0;
  *quality = JPEG_DEFAULT_QUALITY;
  *display_info = 0;
  strncpy(dst_path, JPEG_SAVE_PATH, PATH_LEN - 1);
  dst_path[PATH_LEN - 1] = '\0';

  while ((opt = getopt(argc, argv, "s:b:dq:ip:h")) != -1) {
    switch (opt) {
      case 's':
        {
          int s_val = atoi(optarg);
          if (s_val < 0 || s_val > 4) {
            fprintf(stderr, "Invalid stretch level (0-4)\n");
            return -1;
          }
          conf->preview_stretch_level = static_cast<preview_stretch>(s_val);
          break;
        }
      case 'b':
        {
          int b_val = atoi(optarg);
          if (b_val < CB_AUTO || b_val > CB_NONE) {
            fprintf(stderr, "Invalid color balance value\n");
            return -1;
          }
          conf->preview_color_balance = static_cast<color_balance>(b_val);
          break;
        }
      case 'q':
        {
          *quality = atoi(optarg);
          if (*quality < JPEG_QUALITY_MIN || *quality > JPEG_QUALITY_MAX) {
            fprintf(stderr, "JPEG quality must be %d-%d\n", JPEG_QUALITY_MIN, JPEG_QUALITY_MAX);
            return -1;
          } else if (*quality < JPEG_QUALITY_MIN) {
            *quality = 10;
          }
          break;
        }
      case 'i':
        *display_info = 1;
        break;
      case 'p':
        strncpy(dst_path, optarg, 255);
        dst_path[255] = '\0';
        break;
      case 'h':
        print_usage();
        return 1;
      default:
        fprintf(stderr, "Usage: %s [-s stretch level] [-b balance mode] [-d] [-q quality] [-i metadata] [-p path JPEG save path] filename\n", argv[0]);
        print_usage();
        return -1;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Expected filename argument\n");
    print_usage();
    return -1;
  }
  *file_name = argv[optind];

  return 0;
}

static unsigned char *load_image_file(const char *file_name, size_t *image_size, char *image_path) {
  if (strlen(file_name) == 0) {
    fprintf(stderr, "Wrong file name, empty\n");
    return nullptr;
  }

  strncpy(image_path, file_name, PATH_LEN - 1);
  image_path[PATH_LEN - 1] = '\0';

  FILE *file = fopen(image_path, "rb");
  if (!file) {
    fprintf(stderr, "Open file failed. Return\n");
    return nullptr;
  }

  fseek(file, 0, SEEK_END);
  *image_size = (size_t)ftell(file);
  fseek(file, 0, SEEK_SET);

  unsigned char *image_data = (unsigned char *)malloc(*image_size + 1);
  if (!image_data) {
    fprintf(stderr, "Alloc memory for image failed. Return\n");
    fclose(file);
    return nullptr;
  }

  size_t _size = fread(image_data, 1, *image_size, file);
  if (_size != *image_size) {
    log_message("Read file:%u image_size:%u failed. Return", _size, *image_size);
    fclose(file);
    return nullptr;
  }
  fclose(file);

  return image_data;
}

static char *get_image_format(char *image_path) {
  char *format = strrchr(image_path, '.');
  if (format == nullptr) {
    fprintf(stderr, "file format error, no format suffix\n");
    return nullptr;
  }
  *format = 0;
  return format;
}

static int process_image(unsigned char *image_data, size_t image_size, const char *format,
                         const conf_t *conf, int display_info, int quality,
                         const char *dst_file, char *image_info) {
  const stretch_config_t sc = {
      (uint8_t)conf->preview_stretch_level,
      (uint8_t)conf->preview_color_balance,
      conf->preview_bayer_pattern};

  preview_image *preview_image = main_create_preview(image_data, image_size, format, sc, display_info ? image_info : nullptr);

  if (display_info) {
    printf("%s\n", image_info);
    return 0;
  }

  if (preview_image == nullptr) {
    log_message("%s(): create_preview failed", __FUNCTION__);
    return -1;
  }

  if (preview_image->save(dst_file, quality)) {
    log_message("JPEG file saved successfully to: %s", dst_file);
    delete preview_image;
    return 0;
  } else {
    log_message("Failed to save JPEG file to: %s", dst_file);
    delete preview_image;
    return -1;
  }
}

int main(int argc, char *argv[]) {
  conf_t conf;
  char *file_name = nullptr;
  int display_info = 0;
  int quality = JPEG_DEFAULT_QUALITY;
  char dst_path[PATH_MAX] = {0};
  char image_info[10240] = {0};
  char dst_file[PATH_MAX] = {0};
  char image_path[PATH_MAX] = {0};

  int parse_result = parse_arguments(argc, argv, &conf, &quality, &display_info, dst_path, &file_name);
  if (parse_result != 0) {
    return parse_result == 1 ? 0 : -1;
  }

  size_t image_size = 0;
  unsigned char *image_data = load_image_file(file_name, &image_size, image_path);
  if (!image_data) {
    return -1;
  }

  char *format = get_image_format(image_path);
  if (!format) {
    free(image_data);
    return -1;
  }

  const char *base_name = strrchr(file_name, '/');
  base_name = base_name ? base_name + 1 : file_name;

  char clean_name[256];
  strncpy(clean_name, base_name, sizeof(clean_name) - 1);
  clean_name[sizeof(clean_name) - 1] = '\0';
  char *dot = strrchr(clean_name, '.');
  if (dot)
    *dot = '\0';

  snprintf(dst_file, sizeof(dst_file), "%s/%s.jpeg", dst_path, clean_name);

  int result = process_image(image_data, image_size, format, &conf, display_info, quality, dst_file, image_info);

  free(image_data);
  return result;
}