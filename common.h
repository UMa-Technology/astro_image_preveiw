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

#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <lz4.h>
#include <math.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>


#include "lib/cJSON.h"
#include "lib/xml.h"

#define VERSION "1.0.4"

#define PATH_LEN 256
#define LOG_PATH "/tmp/viewer.log"

#define JPEG_SAVE_PATH       "/tmp/viewer/jpeg"
#define JPEG_DEFAULT_QUALITY 80
#define JPEG_QUALITY_MIN     10
#define JPEG_QUALITY_MAX     100
#define JPEG_MAX_FILES       50 // Max number of files limit
#define JPEG_DELETE_COUNT    10 // Max number of files to delete limit

static inline int get_number_of_cores() {
  int n = sysconf(_SC_NPROCESSORS_ONLN);
  return (n > 1) ? (n - 1) : 1;
}

// BAYER_PAT_XXXX & PIX_FMT_XXXX
#define PACK_PIXEL_FORMAT(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define BAYER_PAT_GBRG PACK_PIXEL_FORMAT('G', 'B', 'R', 'G')
#define BAYER_PAT_GRBG PACK_PIXEL_FORMAT('G', 'R', 'B', 'G')
#define BAYER_PAT_RGGB PACK_PIXEL_FORMAT('R', 'G', 'G', 'B')
#define BAYER_PAT_BGGR PACK_PIXEL_FORMAT('B', 'G', 'G', 'R')
#define BAYER_PAT_NONE PACK_PIXEL_FORMAT('N', 'O', 'N', 'E')
#define BAYER_PAT_AUTO PACK_PIXEL_FORMAT('A', 'U', 'T', 'O')

#define PIX_FMT_INDEX  PACK_PIXEL_FORMAT('I', 'N', 'D', ' ')
#define PIX_FMT_PJPG   PACK_PIXEL_FORMAT('P', 'J', 'P', 'G')
#define PIX_FMT_Y8     PACK_PIXEL_FORMAT('Y', '0', '1', ' ')
#define PIX_FMT_Y16    PACK_PIXEL_FORMAT('Y', '0', '2', ' ')
#define PIX_FMT_Y32    PACK_PIXEL_FORMAT('Y', '0', '4', ' ')
#define PIX_FMT_F32    PACK_PIXEL_FORMAT('F', '0', '4', ' ')
#define PIX_FMT_RGB24  PACK_PIXEL_FORMAT('R', 'G', 'B', '3')
#define PIX_FMT_RGB48  PACK_PIXEL_FORMAT('R', 'G', 'B', '4')
#define PIX_FMT_RGB96  PACK_PIXEL_FORMAT('R', 'G', 'B', '8')
#define PIX_FMT_RGBF   PACK_PIXEL_FORMAT('R', 'G', 'B', 'F')
#define PIX_FMT_3RGB24 PACK_PIXEL_FORMAT('R', 'G', '3', '3')
#define PIX_FMT_3RGB48 PACK_PIXEL_FORMAT('R', 'G', '3', '4')
#define PIX_FMT_3RGB96 PACK_PIXEL_FORMAT('R', 'G', '3', '8')
#define PIX_FMT_3RGBF  PACK_PIXEL_FORMAT('R', 'G', '3', 'F')

#define PIX_FMT_SGBRG8 PACK_PIXEL_FORMAT('G', 'B', 'R', 'G')
#define PIX_FMT_SGRBG8 PACK_PIXEL_FORMAT('G', 'R', 'B', 'G')
#define PIX_FMT_SRGGB8 PACK_PIXEL_FORMAT('R', 'G', 'G', 'B')
#define PIX_FMT_SBGGR8 PACK_PIXEL_FORMAT('B', 'G', 'G', 'R')

#define PIX_FMT_SBGGR12 PACK_PIXEL_FORMAT('B', 'G', '1', '2') // 12  BGBG GRGR
#define PIX_FMT_SGBRG12 PACK_PIXEL_FORMAT('G', 'B', '1', '2') // 12  GBGB RGRG
#define PIX_FMT_SGRBG12 PACK_PIXEL_FORMAT('B', 'A', '1', '2') // 12  GRGR BGBG
#define PIX_FMT_SRGGB12 PACK_PIXEL_FORMAT('R', 'G', '1', '2') // 12  RGRG GBGB

#define PIX_FMT_SBGGR16 PACK_PIXEL_FORMAT('B', 'G', '1', '6') // 16  BGBG GRGR
#define PIX_FMT_SGBRG16 PACK_PIXEL_FORMAT('G', 'B', '1', '6') // 16  GBGB RGRG
#define PIX_FMT_SGRBG16 PACK_PIXEL_FORMAT('G', 'R', '1', '6') // 16  GRGR BGBG
#define PIX_FMT_SRGGB16 PACK_PIXEL_FORMAT('R', 'G', '1', '6') // 16  RGRG GBGB

#define PIX_FMT_SBGGR32 PACK_PIXEL_FORMAT('B', 'G', '3', '2') // 16  BGBG GRGR
#define PIX_FMT_SGBRG32 PACK_PIXEL_FORMAT('G', 'B', '3', '2') // 16  GBGB RGRG
#define PIX_FMT_SGRBG32 PACK_PIXEL_FORMAT('G', 'R', '3', '2') // 16  GRGR BGBG
#define PIX_FMT_SRGGB32 PACK_PIXEL_FORMAT('R', 'G', '3', '2') // 16  RGRG GBGB

#define PIX_FMT_SBGGRF PACK_PIXEL_FORMAT('B', 'G', 'F', ' ') // 16  BGBG GRGR
#define PIX_FMT_SGBRGF PACK_PIXEL_FORMAT('G', 'B', 'F', ' ') // 16  GBGB RGRG
#define PIX_FMT_SGRBGF PACK_PIXEL_FORMAT('G', 'R', 'F', ' ') // 16  GRGR BGBG
#define PIX_FMT_SRGGBF PACK_PIXEL_FORMAT('R', 'G', 'F', ' ') // 16  RGRG GBGB

typedef enum {
  STRETCH_NONE = 0,
  STRETCH_SLIGHT = 1,
  STRETCH_MODERATE = 2,
  STRETCH_NORMAL = 3,
  STRETCH_HARD = 4,
} preview_stretch;

typedef enum {
  SHOW_FWHM = 0,
  SHOW_HFD = 1,
} focuser_display_data;

typedef enum {
  SHOW_RA_DEC_DRIFT = 0,
  SHOW_RA_DEC_PULSE = 1,
  SHOW_X_Y_DRIFT = 2,
} guider_display_data;

typedef enum {
  CB_AUTO = 0,
  CB_NONE
} color_balance;

typedef struct
{
  preview_stretch preview_stretch_level;
  color_balance preview_color_balance;
  uint32_t preview_bayer_pattern;
} conf_t;

typedef enum {
  PREVIEW_STRETCH_NONE = 0,
  PREVIEW_STRETCH_SLIGHT,
  PREVIEW_STRETCH_MODERATE,
  PREVIEW_STRETCH_NORMAL,
  PREVIEW_STRETCH_HARD,
  PREVIEW_STRETCH_COUNT
} preview_stretch_level;

typedef enum {
  COLOR_BALANCE_AUTO = 0,
  COLOR_BALANCE_NONE,
  COLOR_BALANCE_COUNT
} color_balance_t;

typedef enum {
  DEBAYER_AUTO = 0,
  DEBAYER_NONE,
  DEBAYER_GBRG,
  DEBAYER_GRBG,
  DEBAYER_RGGB,
  DEBAYER_BGGR,
  DEBAYER_COUNT
} debayer_t;

typedef struct {
  double clip_black;
  double clip_white;
} preview_stretch_t;

typedef struct {
  uint8_t stretch_level;
  uint8_t balance;        // 0 = AWB, 1 = red, 2 = green, 3 = blue;
  uint32_t bayer_pattern; // BAYER_PAT_XXXX
} stretch_config_t;

typedef struct {
  float brightness;
  float contrast;
} stretch_input_params_t;

static const preview_stretch_t stretch_linear_lut[] = {
    {0.01, 0.001},
    {0.01, 0.07},
    {0.01, 0.25},
    {0.01, 0.75},
    {0.01, 1.30},
};

static const stretch_input_params_t stretch_params_lut[] = {
    {0, 0}, // Does not matter
    {0.05, -2.8},
    {0.15, -2.8},
    {0.25, -2.8},
    {0.40, -2.5}};

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int debug_enabled = 0;

static inline void log_message(const char *format, ...) {
  if (!debug_enabled)
    return;

  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char timestamp[20];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

  va_list args;
  va_start(args, format);
  char message[2048] = {0};
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  pthread_mutex_lock(&log_mutex);
  FILE *fp = fopen(LOG_PATH, "a");
  if (fp) {
    fprintf(fp, "[%s] [DEBUG] %s\n", timestamp, message);
    fflush(fp);
    fclose(fp);
  }
  pthread_mutex_unlock(&log_mutex);
}


#endif // COMMON_H