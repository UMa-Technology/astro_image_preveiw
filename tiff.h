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

#ifndef _TIFF_H
#define _TIFF_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIFF_MAX_KEYWORDS 128

typedef struct tiff_metadata {
  uint32_t width;
  uint32_t height;
  uint16_t bitspersample;
  uint16_t sampleformat;
  uint16_t photometric;
  uint16_t fillorder;

  char image_description[8640];
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
  double bzero;
  double bscale;
  char date_obs[32];
  char imagetyp[32];
  char instrume[64];
  char object[64];
  char bayerpat[10];
  char roworder[16];

  char fits_names[TIFF_MAX_KEYWORDS][16];
  char fits_values[TIFF_MAX_KEYWORDS][80];
  int nkeywords;
} tiff_metadata;

void tiff_metadata_init(tiff_metadata *metadata);
void tiff_parse_image_description(tiff_metadata *metadata, const char *desc);
void get_tiff_header(tiff_metadata *header, char *buffer);

#ifdef __cplusplus
}
#endif

#endif // _TIFF_H
