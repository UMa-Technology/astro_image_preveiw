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

#ifndef _PREVIEW_H
#define _PREVIEW_H

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common.h"
#include "lib/cJSON.h"
#include "rgb.h"
#include "stretch.h"

#define MIN_SIZE_TO_PARALLELIZE 0x3FFFF
#define DEFAULT_THREADS         3

class preview_image {
private:
  mutable std::once_flag m_init_flag;

public:
  preview_image(int width, int height, int channels = 3) : m_raw_data(nullptr),
                                                           m_width(width),
                                                           m_height(height),
                                                           m_pix_format(0),
                                                           m_strech_params(),
                                                           image_data(nullptr),
                                                           preview_width(width),
                                                           preview_height(height),
                                                           preview_channels(channels){
                                                           };

  preview_image(preview_image &image) {
    m_raw_data = image.m_raw_data;
    m_width = image.m_width;
    m_height = image.m_height;
    m_pix_format = image.m_pix_format;
    preview_width = image.preview_width;
    preview_height = image.preview_height;
    preview_channels = image.preview_channels;
    m_strech_params = image.m_strech_params;
  };

  preview_image &operator=(preview_image &image) {
    m_raw_data = image.m_raw_data;
    m_width = image.m_width;
    m_height = image.m_height;
    m_pix_format = image.m_pix_format;
    preview_width = image.preview_width;
    preview_height = image.preview_height;
    preview_channels = image.preview_channels;
    m_strech_params = image.m_strech_params;
    return *this;
  };

  bool save(const char *filename, int quality);

  void ensure_image_data_allocated() {
    std::call_once(m_init_flag, [&]() {
      image_data = new unsigned char[preview_width * preview_height * preview_channels];
    });
  }

  ~preview_image() {
    if (m_raw_data != nullptr) {
      free(m_raw_data);
      m_raw_data = nullptr;
    }

    if (image_data != nullptr) {
      delete[] image_data;
      image_data = nullptr;
    }
  };

  void setPixel(int x, int y, rgb_triplet color) {
    if (x < 0 || x >= preview_width || y < 0 || y >= preview_height)
      return;

    ensure_image_data_allocated();

    unsigned char *pixel = image_data + (y * preview_width + x) * preview_channels;

    if (preview_channels >= 1)
      pixel[0] = red(color); // R or gray
    if (preview_channels >= 2)
      pixel[1] = green(color); // G
    if (preview_channels >= 3)
      pixel[2] = blue(color); // B
  }

  int width() const { return preview_width; }
  int height() const { return preview_height; }

  char *m_raw_data;
  int m_width;
  int m_height;
  int m_pix_format;
  StretchParams m_strech_params;

  unsigned char *image_data;
  int preview_width;
  int preview_height;
  int preview_channels;
};

int get_bayer_offsets(uint32_t pix_format);

template <typename T>
static inline void debayer(T *raw, int index, int row, int column, int width, int height, int offsets, float &red, float &green, float &blue) {
  switch (offsets ^ ((column & 1) << 4 | (row & 1))) {
    case 0x00:
      red = raw[index];
      if (column == 0) {
        if (row == 0) {
          green = (raw[index + 1] + raw[index + width]) / 2.0;
          blue = raw[index + width + 1];
        } else if (row == height - 1) {
          green = (raw[index + 1] + raw[index - width]) / 2.0;
          blue = raw[index - width + 1];
        } else {
          green = (raw[index + 1] + raw[index + width] + raw[index - width]) / 3.0;
          blue = (raw[index - width + 1] + raw[index + width + 1]) / 2.0;
        }
      } else if (column == width - 1) {
        if (row == 0) {
          green = (raw[index - 1] + raw[index + width]) / 2.0;
          blue = (raw[index + width - 1] + raw[index + width + 1]) / 2.0;
        } else if (row == height - 1) {
          green = (raw[index - 1] + raw[index - width]) / 2.0;
          blue = (raw[index - width - 1] + raw[index - width + 1]) / 2.0;
        } else {
          green = (raw[index - 1] + raw[index + width] + raw[index - width]) / 3.0;
          blue = (raw[index - width - 1] + raw[index + width - 1]) / 2.0;
        }
      } else {
        if (row == 0) {
          green = (raw[index + 1] + raw[index - 1] + raw[index + width]) / 3.0;
          blue = (raw[index + width - 1] + raw[index + width + 1]) / 2.0;
        } else if (row == height - 1) {
          green = (raw[index + 1] + raw[index - 1] + raw[index - width]) / 3.0;
          blue = (raw[index - width - 1] + raw[index - width + 1]) / 2.0;
        } else {
          green = (raw[index + 1] + raw[index - 1] + raw[index + width] + raw[index - width]) / 4.0;
          blue = (raw[index - width - 1] + raw[index - width + 1] + raw[index + width - 1] + raw[index + width + 1]) / 4.0;
        }
      }
      break;
    case 0x10:
      if (column == 0) {
        red = raw[index + 1];
      } else if (column == width - 1) {
        red = raw[index - 1];
      } else {
        red = (raw[index - 1] + raw[index + 1]) / 2.0;
      }
      green = raw[index];
      if (row == 0) {
        blue = raw[index + width];
      } else if (row == height - 1) {
        blue = raw[index - width];
      } else {
        blue = (raw[index - width] + raw[index + width]) / 2.0;
      }
      break;
    case 0x01:
      if (row == 0) {
        red = raw[index + width];
      } else if (row == height - 1) {
        red = raw[index - width];
      } else {
        red = (raw[index - width] + raw[index + width]) / 2.0;
      }
      green = raw[index];
      if (column == 0) {
        blue = raw[index + 1];
      } else if (column == width - 1) {
        blue = raw[index - 1];
      } else {
        blue = (raw[index - 1] + raw[index + 1]) / 2.0;
      }
      break;
    case 0x11:
      if (column == 0) {
        if (row == 0) {
          red = raw[index + width + 1];
          green = (raw[index + 1] + raw[index + width]) / 2.0;
        } else if (row == height - 1) {
          red = raw[index - width + 1];
          green = (raw[index + 1] + raw[index - width]) / 2.0;
        } else {
          red = raw[index - width + 1];
          green = (raw[index + 1] + raw[index + width] + raw[index - width]) / 3.0;
        }
      } else if (column == width - 1) {
        if (row == 0) {
          red = raw[index + width - 1];
          green = (raw[index - 1] + raw[index + width]) / 2.0;
        } else if (row == height - 1) {
          red = raw[index - width - 1];
          green = (raw[index - 1] + raw[index - width]) / 2.0;
        } else {
          red = (raw[index - width - 1] + raw[index + width - 1]) / 2.0;
          green = (raw[index - 1] + raw[index + width] + raw[index - width]) / 3.0;
        }
      } else {
        if (row == 0) {
          red = (raw[index + width - 1] + raw[index + width + 1]) / 2.0;
          green = (raw[index + 1] + raw[index - 1] + raw[index + width]) / 3.0;
        } else if (row == height - 1) {
          red = (raw[index - width - 1] + raw[index - width + 1]) / 2.0;
          green = (raw[index + 1] + raw[index - 1] + raw[index - width]) / 3.0;
        } else {
          red = (raw[index - width - 1] + raw[index - width + 1] + raw[index + width - 1] + raw[index + width + 1]) / 4.0;
          green = (raw[index + 1] + raw[index - 1] + raw[index + width] + raw[index - width]) / 4.0;
        }
      }
      blue = raw[index];
      break;
  }
}

template <typename T>
void parallel_debayer(T *input_buffer, int width, int height, int offsets, T *output_buffer) {
  const int size = width * height;
  if (size < MIN_SIZE_TO_PARALLELIZE) {
    int input_index = 0;
    for (int row_index = 0; row_index < height; row_index++) {
      for (int column_index = 0; column_index < width; column_index++) {
        float red = 0, green = 0, blue = 0;
        int output_index = input_index * 3;
        debayer(input_buffer, input_index, row_index, column_index, width, height, offsets, red, green, blue);
        output_buffer[output_index] = red;
        output_buffer[output_index + 1] = green;
        output_buffer[output_index + 2] = blue;
        input_index++;
      }
    }
  } else {
    int max_threads = get_number_of_cores();
    max_threads = (max_threads > 0) ? max_threads : DEFAULT_THREADS;
    std::vector<std::thread> threads;
    threads.reserve(max_threads);
    for (int rank = 0; rank < max_threads; rank++) {
      const int chunk = ceil(height / (double)max_threads);
      threads.emplace_back([=]() {
				const int start = chunk * rank;
				int end = start + chunk;
				end = (end > height) ? height : end;
				int input_index = start * width;
				for (int row_index = start; row_index < end; row_index++) {
					for (int column_index = 0; column_index < width; column_index++) {
						float red = 0, green = 0, blue = 0;
						int output_index = input_index * 3;
						debayer(input_buffer, input_index, row_index, column_index, width, height, offsets, red, green, blue);
						output_buffer[output_index] = red;
						output_buffer[output_index + 1] = green;
						output_buffer[output_index + 2] = blue;
						input_index++;
					}
				} });
    }
    for (auto &thread : threads) {
      thread.join();
    }
  }
}

preview_image *create_jpeg_preview(unsigned char *jpg_buffer, unsigned long jpg_size);
preview_image *create_fits_preview(unsigned char *fits_buffer, unsigned long fits_size, const stretch_config_t sconfig);
preview_image *create_xisf_preview(unsigned char *xisf_buffer, unsigned long xisf_size, const stretch_config_t sconfig);
preview_image *create_raw_preview(unsigned char *raw_image_buffer, unsigned long raw_size, const stretch_config_t sconfig);
preview_image *main_create_preview(unsigned char *data, size_t size, const char *format, const stretch_config_t sconfig, char *image_info);
preview_image *create_preview(int width, int height, int pixel_format, char *image_data, const stretch_config_t sconfig);
void stretch_preview(preview_image *img, const stretch_config_t sconfig);

#endif /* PREVIEW_H */