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

#include "tiff.h"

void tiff_metadata_init(tiff_metadata *metadata) {
  metadata->width = 0;
  metadata->height = 0;
  metadata->bitspersample = 0;
  metadata->sampleformat = 0;
  metadata->photometric = 0;
  metadata->fillorder = 0;

  metadata->image_description[0] = '\0';
  metadata->gain = -1;
  metadata->egain = -1;
  metadata->offset = -1;
  metadata->gamma = -1;
  metadata->exptime = -1;
  metadata->ccd_temp = -273;  // Use absolute zero as "not set"
  metadata->xpixsz = -1;
  metadata->ypixsz = -1;
  metadata->xbinning = -1;
  metadata->ybinning = -1;
  metadata->aptdia = -1;
  metadata->focallen = -1;
  metadata->jd = -1;
  metadata->bzero = 0;
  metadata->bscale = 1.0;
  metadata->date_obs[0] = '\0';
  metadata->imagetyp[0] = '\0';
  metadata->instrume[0] = '\0';
  metadata->object[0] = '\0';
  metadata->bayerpat[0] = '\0';
  metadata->roworder[0] = '\0';

  metadata->nkeywords = 0;
}

// Helper function to extract value from FITS keyword line
// Format: "KEYWORD = VALUE / comment" or "KEYWORD = 'STRING' / comment"
static int parse_fits_keyword(const char *line, char *keyword, char *value) {
  keyword[0] = '\0';
  value[0] = '\0';

  int i = 0;
  while (i < 8 && line[i] && line[i] != ' ' && line[i] != '=') {
    keyword[i] = line[i];
    i++;
  }
  keyword[i] = '\0';

  // Find '=' sign
  const char *eq = strchr(line, '=');
  if (!eq) return 0;

  // Skip whitespace after '='
  const char *val_start = eq + 1;
  while (*val_start == ' ') val_start++;

  if (*val_start == '\'') {
    val_start++;
    const char *val_end = strchr(val_start, '\'');
    if (val_end) {
      int len = val_end - val_start;
      if (len > 70) len = 70;
      strncpy(value, val_start, len);
      value[len] = '\0';
      while (len > 0 && value[len - 1] == ' ') {
        value[--len] = '\0';
      }
    }
  } else {
    int j = 0;
    while (*val_start && *val_start != ' ' && *val_start != '/' && j < 70) {
      value[j++] = *val_start++;
    }
    value[j] = '\0';
  }

  return 1;
}

void tiff_parse_image_description(tiff_metadata *metadata, const char *desc) {
  if (!desc || !metadata) return;

  strncpy(metadata->image_description, desc, sizeof(metadata->image_description) - 1);
  metadata->image_description[sizeof(metadata->image_description) - 1] = '\0';

  char line[256];
  const char *p = desc;

  while (*p) {
    int i = 0;
    while (*p && *p != '\n' && i < 255) {
      line[i++] = *p++;
    }
    line[i] = '\0';
    if (*p == '\n') p++;

    if (i == 0) continue;

    char keyword[16], value[80];
    if (!parse_fits_keyword(line, keyword, value)) continue;
    if (keyword[0] == '\0') continue;

    if (metadata->nkeywords < TIFF_MAX_KEYWORDS) {
      size_t kw_len = strlen(keyword);
      if (kw_len > 15) kw_len = 15;
      memcpy(metadata->fits_names[metadata->nkeywords], keyword, kw_len);
      metadata->fits_names[metadata->nkeywords][kw_len] = '\0';
      size_t val_len = strlen(value);
      if (val_len > 79) val_len = 79;
      memcpy(metadata->fits_values[metadata->nkeywords], value, val_len);
      metadata->fits_values[metadata->nkeywords][val_len] = '\0';
      metadata->nkeywords++;
    }

    float f;
    int iv;
    double d;

    if (!strcmp(keyword, "GAIN") && sscanf(value, "%f", &f) == 1) {
      metadata->gain = f;
    } else if (!strcmp(keyword, "EGAIN") && sscanf(value, "%f", &f) == 1) {
      metadata->egain = f;
    } else if (!strcmp(keyword, "OFFSET") && sscanf(value, "%f", &f) == 1) {
      metadata->offset = f;
    } else if (!strcmp(keyword, "GAMMA") && sscanf(value, "%f", &f) == 1) {
      metadata->gamma = f;
    } else if (!strcmp(keyword, "EXPTIME") && sscanf(value, "%f", &f) == 1) {
      metadata->exptime = f;
    } else if (!strcmp(keyword, "CCD-TEMP") && sscanf(value, "%f", &f) == 1) {
      metadata->ccd_temp = f;
    } else if (!strcmp(keyword, "XPIXSZ") && sscanf(value, "%f", &f) == 1) {
      metadata->xpixsz = f;
    } else if (!strcmp(keyword, "YPIXSZ") && sscanf(value, "%f", &f) == 1) {
      metadata->ypixsz = f;
    } else if (!strcmp(keyword, "XBINNING") && sscanf(value, "%d", &iv) == 1) {
      metadata->xbinning = iv;
    } else if (!strcmp(keyword, "YBINNING") && sscanf(value, "%d", &iv) == 1) {
      metadata->ybinning = iv;
    } else if (!strcmp(keyword, "APTDIA") && sscanf(value, "%f", &f) == 1) {
      metadata->aptdia = f;
    } else if (!strcmp(keyword, "FOCALLEN") && sscanf(value, "%f", &f) == 1) {
      metadata->focallen = f;
    } else if (!strcmp(keyword, "JD") && sscanf(value, "%lf", &d) == 1) {
      metadata->jd = d;
    } else if (!strcmp(keyword, "BZERO") && sscanf(value, "%lf", &d) == 1) {
      metadata->bzero = d;
    } else if (!strcmp(keyword, "BSCALE") && sscanf(value, "%lf", &d) == 1) {
      metadata->bscale = d;
    } else if (!strcmp(keyword, "DATE-OBS")) {
      strncpy(metadata->date_obs, value, sizeof(metadata->date_obs) - 1);
      metadata->date_obs[sizeof(metadata->date_obs) - 1] = '\0';
    } else if (!strcmp(keyword, "IMAGETYP")) {
      strncpy(metadata->imagetyp, value, sizeof(metadata->imagetyp) - 1);
      metadata->imagetyp[sizeof(metadata->imagetyp) - 1] = '\0';
    } else if (!strcmp(keyword, "INSTRUME")) {
      strncpy(metadata->instrume, value, sizeof(metadata->instrume) - 1);
      metadata->instrume[sizeof(metadata->instrume) - 1] = '\0';
    } else if (!strcmp(keyword, "OBJECT")) {
      strncpy(metadata->object, value, sizeof(metadata->object) - 1);
      metadata->object[sizeof(metadata->object) - 1] = '\0';
    } else if (!strcmp(keyword, "BAYERPAT")) {
      strncpy(metadata->bayerpat, value, sizeof(metadata->bayerpat) - 1);
      metadata->bayerpat[sizeof(metadata->bayerpat) - 1] = '\0';
    } else if (!strcmp(keyword, "ROWORDER")) {
      strncpy(metadata->roworder, value, sizeof(metadata->roworder) - 1);
      metadata->roworder[sizeof(metadata->roworder) - 1] = '\0';
    }
  }
}

void get_tiff_header(tiff_metadata *header, char *buffer) {
  if (!buffer)
    return;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "SIGNATURE", "TIFF");
  cJSON_AddNumberToObject(root, "ImageWidth", header->width);
  cJSON_AddNumberToObject(root, "ImageLength", header->height);
  cJSON_AddNumberToObject(root, "BitsPerSample", header->bitspersample);
  cJSON_AddNumberToObject(root, "SampleFormat", header->sampleformat);
  cJSON_AddNumberToObject(root, "Photometric", header->photometric);
  cJSON_AddNumberToObject(root, "FillOrder", header->fillorder);

  for (int i = 0; i < header->nkeywords; i++) {
    cJSON_AddStringToObject(root, header->fits_names[i], header->fits_values[i]);
  }

  char *json_str = cJSON_PrintUnformatted(root);
  strcpy(buffer, json_str);

  cJSON_Delete(root);
  free(json_str);
}
