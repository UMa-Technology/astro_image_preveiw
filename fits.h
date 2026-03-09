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

#ifndef _FITS_H
#define _FITS_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FITS_HEADER_BLOCK_SIZE 2880

typedef enum fits_error {
  FITS_OK = 0,
  FITS_INVALIDDATA = -1,
  FITS_INVALIDPARAM = -2
} fits_error;

typedef enum fits_headerState {
  STATE_SIMPLE,
  STATE_XTENSION,
  STATE_BITPIX,
  STATE_NAXIS,
  STATE_NAXIS_N,
  STATE_PCOUNT,
  STATE_GCOUNT,
  STATE_REST,
} fits_header_state;

// Maximum number of keywords to store
#define FITS_MAX_KEYWORDS 128

// Structure to store the header keywords in FITS file
typedef struct fits_header {
  fits_header_state state;
  unsigned naxis_index;
  int bitpix;
  int64_t blank;
  int blank_found;
  int naxis;
  int naxisn[999];
  int pcount;
  int gcount;
  int groups;
  int rgb; // 1 if file contains RGB image, 0 otherwise
  char bayerpat[5];
  int xbayeroff;
  int ybayeroff;
  int image_extension;
  char image_extension_name[9];
  double bscale;
  double bzero;
  int data_min_found;
  double data_min;
  int data_max_found;
  double data_max;
  int data_offset;
  char swmodify[512];
  char notes[128];

  // Generic keyword storage (expanded from 80 to 256)
  char name[FITS_MAX_KEYWORDS][16];
  char value[FITS_MAX_KEYWORDS][80];
  int nkeywords;

  // Numeric fields for direct access (initialized to -1 if not set)
  float gain;
  float egain;
  float offset;
  float gamma;
  float exptime;
  float ccd_temp;
  float xpixsz;
  float ypixsz;
  int xbinning;
  int ybinning;
  float aptdia;
  float focallen;
  double jd;
  char date_obs[32];
  char imagetyp[32];
  char instrume[64];
  char object[64];
  char roworder[16];
} fits_header;

void get_fits_header(fits_header *header, char *buffer);
int fits_read_header(const uint8_t *fits_data, int fits_size, fits_header *header);
int fits_get_buffer_size(fits_header *header);
int fits_process_data(const uint8_t *fits_data, int fits_size, fits_header *header, char *native_data);

#ifdef __cplusplus
}
#endif

#endif // _FITS_H