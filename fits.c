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

#include "fits.h"

static int fits_header_init(fits_header *header, fits_header_state state) {
  header->state = state;
  header->naxis_index = 0;
  header->blank_found = 0;
  header->pcount = 0;
  header->gcount = 1;
  header->groups = 0;
  header->rgb = 0;
  header->bayerpat[0] = '\0';
  header->xbayeroff = 3;
  header->ybayeroff = 4;
  header->image_extension = 0;
  header->bscale = 1.0;
  header->bzero = 0;
  header->data_min = 0;
  header->data_min_found = 0;
  header->data_max = 0;
  header->data_max_found = 0;
  header->data_offset = 0;
  header->nkeywords = 0;

  header->gain = -1;
  header->egain = -1;
  header->offset = -1;
  header->gamma = -1;
  header->exptime = -1;
  header->ccd_temp = -1;
  header->xpixsz = -1;
  header->ypixsz = -1;
  header->xbinning = -1;
  header->ybinning = -1;
  header->aptdia = -1;
  header->focallen = -1;
  header->jd = -1;
  header->date_obs[0] = '\0';
  header->imagetyp[0] = '\0';
  header->instrume[0] = '\0';
  header->object[0] = '\0';
  header->roworder[0] = '\0';
  return 0;
}

static int read_keyword_value(const uint8_t *ptr8, char *keyword, char *value) {
  int i;

  for (i = 0; i < 8 && ptr8[i] != ' '; i++) {
    keyword[i] = ptr8[i];
  }
  keyword[i] = '\0';

  if (ptr8[8] == '=') {
    i = 10;
    while (i < 80 && ptr8[i] == ' ') {
      i++;
    }

    if (i < 80) {
      *value++ = ptr8[i];
      i++;
      if (ptr8[i - 1] == '\'') {
        for (; i < 80 && ptr8[i] != '\''; i++) {
          *value++ = ptr8[i];
        }
        *value++ = '\'';
      } else if (ptr8[i - 1] == '(') {
        for (; i < 80 && ptr8[i] != ')'; i++) {
          *value++ = ptr8[i];
        }
        *value++ = ')';
      } else {
        for (; i < 80 && ptr8[i] != ' ' && ptr8[i] != '/'; i++) {
          *value++ = ptr8[i];
        }
      }
    }
  }
  *value = '\0';
  return 0;
}

#define CHECK_KEYWORD(key)                                                    \
  if (strcmp(keyword, key)) {                                                 \
    log_message("Expected %s keyword, found %s = %s\n", key, keyword, value); \
    return FITS_INVALIDDATA;                                                  \
  }

#define CHECK_VALUE(key, val)                                                   \
  if (sscanf(value, "%d", &header->val) != 1) {                                 \
    log_message("Invalid value of %s keyword, %s = %s\n", key, keyword, value); \
    return FITS_INVALIDDATA;                                                    \
  }

static int fits_header_parse_line(fits_header *header, const uint8_t line[80]) {
  int dim_no, ret;
  int64_t t;
  double d;
  char keyword[10], value[72], c;

  read_keyword_value(line, keyword, value);
  int inx = header->nkeywords;

  if (inx >= FITS_MAX_KEYWORDS) {
    if (!strcmp(keyword, "END")) {
      return 1;
    }
    return FITS_OK;
  }

  char *val = header->value[inx];
  if (strcmp(keyword, "END")) {
    strncpy(header->name[inx], keyword, 16);
    header->name[inx][15] = '\0';
    strncpy(header->value[inx], value, 80);
    header->value[inx][79] = '\0';
    size_t i = 0, j = 0;
    for (; j < strlen(value); j++) {
      if (value[j] != '\'') {
        val[i++] = value[j];
      }
    }
    val[i] = '\0';
    header->nkeywords++;
  }

  switch (header->state) {
    case STATE_SIMPLE:
      CHECK_KEYWORD("SIMPLE");

      if (value[0] == 'F') {
        log_message("not a standard FITS file\n");
      } else if (value[0] != 'T') {
        log_message("invalid value of SIMPLE keyword, SIMPLE = %c\n", value[0]);
        return FITS_INVALIDDATA;
      }

      header->state = STATE_BITPIX;
      break;
    case STATE_XTENSION:
      CHECK_KEYWORD("XTENSION");

      if (!strcmp(value, "'IMAGE   '")) {
        header->image_extension = 1;
      }
      strncpy(header->image_extension_name, value, 8);
      header->image_extension_name[8] = '\0';

      header->state = STATE_BITPIX;
      break;
    case STATE_BITPIX:
      CHECK_KEYWORD("BITPIX");
      CHECK_VALUE("BITPIX", bitpix);

      switch (header->bitpix) {
        case 8:
        case 16:
        case 32:
        case -32:
        case 64:
        case -64:
          break;
        default:
          log_message("invalid value of BITPIX %d\n", header->bitpix);
          return FITS_INVALIDDATA;
      }

      header->state = STATE_NAXIS;
      break;
    case STATE_NAXIS:
      CHECK_KEYWORD("NAXIS");
      CHECK_VALUE("NAXIS", naxis);

      if (header->naxis) {
        header->state = STATE_NAXIS_N;
      } else {
        header->state = STATE_REST;
      }
      break;
    case STATE_NAXIS_N:
      ret = sscanf(keyword, "NAXIS%d", &dim_no);
      if (ret != 1 || (unsigned int)dim_no != header->naxis_index + 1) {
        log_message("expected NAXIS%d keyword, found %s = %s\n", header->naxis_index + 1, keyword, value);
        return FITS_INVALIDDATA;
      }

      if (sscanf(value, "%d", &header->naxisn[header->naxis_index]) != 1) {
        log_message("invalid value of NAXIS%d keyword, %s = %s\n", header->naxis_index + 1, keyword, value);
        return FITS_INVALIDDATA;
      }

      header->naxis_index++;
      if (header->naxis_index == (unsigned int)header->naxis) {
        header->state = STATE_REST;
      }
      break;
    case STATE_PCOUNT:
      CHECK_KEYWORD("PCOUNT");
      CHECK_VALUE("PCOUNT", pcount);
      header->state = STATE_GCOUNT;
      break;
    case STATE_GCOUNT:
      CHECK_KEYWORD("GCOUNT");
      CHECK_VALUE("GCOUNT", gcount);
      header->state = STATE_REST;
      break;
    case STATE_REST:
      if (!strcmp(keyword, "BLANK") && sscanf(value, "%" SCNd64 "", &t) == 1) {
        header->blank = t;
        header->blank_found = 1;
      } else if (!strcmp(keyword, "BSCALE") && sscanf(value, "%lf", &d) == 1) {
        header->bscale = d;
      } else if (!strcmp(keyword, "BZERO") && sscanf(value, "%lf", &d) == 1) {
        header->bzero = d;
      } else if (!strcmp(keyword, "CTYPE3") && !strncmp(value, "'RGB", 4)) {
        header->rgb = 1;
      } else if (!strcmp(keyword, "BAYERPAT")) {
        strncpy(header->bayerpat, value + 1, 4);
        header->bayerpat[4] = '\0';
      } else if (!strcmp(keyword, "XBAYROFF") && sscanf(value, "%lf", &d) == 1) {
        header->xbayeroff = d;
      } else if (!strcmp(keyword, "YBAYROFF") && sscanf(value, "%lf", &d) == 1) {
        header->ybayeroff = d;
      } else if (!strcmp(keyword, "DATAMIN") && sscanf(value, "%lf", &d) == 1) {
        header->data_min_found = 1;
        header->data_min = d;
      } else if (!strcmp(keyword, "END")) {
        return 1;
      } else if (!strcmp(keyword, "GROUPS") && sscanf(value, "%c", &c) == 1) {
        header->groups = (c == 'T');
      } else if (!strcmp(keyword, "GCOUNT") && sscanf(value, "%" SCNd64 "", &t) == 1) {
        header->gcount = t;
      } else if (!strcmp(keyword, "PCOUNT") && sscanf(value, "%" SCNd64 "", &t) == 1) {
        header->pcount = t;
      } else if (!strcmp(keyword, "SWMODIFY")) {
        strncpy(header->swmodify, value, 511);
      } else if (!strcmp(keyword, "NOTES")) {
        strncpy(header->notes, value, 127);
      }
      float f;
      int iv;
      if (!strcmp(keyword, "GAIN") && sscanf(value, "%f", &f) == 1) {
        header->gain = f;
      } else if (!strcmp(keyword, "EGAIN") && sscanf(value, "%f", &f) == 1) {
        header->egain = f;
      } else if (!strcmp(keyword, "OFFSET") && sscanf(value, "%f", &f) == 1) {
        header->offset = f;
      } else if (!strcmp(keyword, "GAMMA") && sscanf(value, "%f", &f) == 1) {
        header->gamma = f;
      } else if (!strcmp(keyword, "EXPTIME") && sscanf(value, "%f", &f) == 1) {
        header->exptime = f;
      } else if (!strcmp(keyword, "CCD-TEMP") && sscanf(value, "%f", &f) == 1) {
        header->ccd_temp = f;
      } else if (!strcmp(keyword, "XPIXSZ") && sscanf(value, "%f", &f) == 1) {
        header->xpixsz = f;
      } else if (!strcmp(keyword, "YPIXSZ") && sscanf(value, "%f", &f) == 1) {
        header->ypixsz = f;
      } else if (!strcmp(keyword, "XBINNING") && sscanf(value, "%d", &iv) == 1) {
        header->xbinning = iv;
      } else if (!strcmp(keyword, "YBINNING") && sscanf(value, "%d", &iv) == 1) {
        header->ybinning = iv;
      } else if (!strcmp(keyword, "APTDIA") && sscanf(value, "%f", &f) == 1) {
        header->aptdia = f;
      } else if (!strcmp(keyword, "FOCALLEN") && sscanf(value, "%f", &f) == 1) {
        header->focallen = f;
      } else if (!strcmp(keyword, "JD") && sscanf(value, "%lf", &d) == 1) {
        header->jd = d;
      } else if (!strcmp(keyword, "DATE-OBS")) {
        char *src = value;
        if (*src == '\'') src++;
        strncpy(header->date_obs, src, sizeof(header->date_obs) - 1);
        header->date_obs[sizeof(header->date_obs) - 1] = '\0';
        char *end = strchr(header->date_obs, '\'');
        if (end) *end = '\0';
      } else if (!strcmp(keyword, "IMAGETYP")) {
        char *src = value;
        if (*src == '\'') src++;
        strncpy(header->imagetyp, src, sizeof(header->imagetyp) - 1);
        header->imagetyp[sizeof(header->imagetyp) - 1] = '\0';
        char *end = strchr(header->imagetyp, '\'');
        if (end) *end = '\0';
      } else if (!strcmp(keyword, "INSTRUME")) {
        char *src = value;
        if (*src == '\'') src++;
        strncpy(header->instrume, src, sizeof(header->instrume) - 1);
        header->instrume[sizeof(header->instrume) - 1] = '\0';
        char *end = strchr(header->instrume, '\'');
        if (end) *end = '\0';
      } else if (!strcmp(keyword, "OBJECT")) {
        char *src = value;
        if (*src == '\'') src++;
        strncpy(header->object, src, sizeof(header->object) - 1);
        header->object[sizeof(header->object) - 1] = '\0';
        char *end = strchr(header->object, '\'');
        if (end) *end = '\0';
      } else if (!strcmp(keyword, "ROWORDER")) {
        char *src = value;
        if (*src == '\'') src++;
        strncpy(header->roworder, src, sizeof(header->roworder) - 1);
        header->roworder[sizeof(header->roworder) - 1] = '\0';
        char *end = strchr(header->roworder, '\'');
        if (end) *end = '\0';
      }
      break;
  }
  return FITS_OK;
}

void get_fits_header(fits_header *header, char *buffer) {
  if (!buffer)
    return;

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    log_message("Failed to create JSON object");
    return;
  }

  for (int i = 0; i < header->nkeywords; i++) {
    cJSON_AddStringToObject(root, header->name[i], header->value[i]);
  }

  char *json_str = cJSON_PrintUnformatted(root);
  if (json_str) {
    strcpy(buffer, json_str);
    free(json_str);
  } else {
    log_message("Failed to print JSON object");
  }

  cJSON_Delete(root);
}

int fits_read_header(const uint8_t *fits_data, int fits_size, fits_header *header) {
  const uint8_t *ptr8 = fits_data;
  int lines_read, ret = 0;
  size_t size;

  if (fits_size < FITS_HEADER_BLOCK_SIZE)
    return FITS_INVALIDDATA;

  lines_read = 0;
  fits_header_init(header, STATE_SIMPLE);
  do {
    ret = fits_header_parse_line(header, ptr8);
    ptr8 += 80;
    lines_read++;
  } while (!ret);

  if (ret < 0)
    return ret;

  header->data_offset = (int)ceil(lines_read / 36.0) * FITS_HEADER_BLOCK_SIZE;
  log_message("lines_read = %d, header blocks = %d", lines_read, (int)ceil(lines_read / 36.0));

  if (header->rgb && (header->naxis != 3 || (header->naxisn[2] != 3 && header->naxisn[2] != 4))) {
    log_message("File contains RGB image but NAXIS = %d and NAXIS3 = %d\n", header->naxis, header->naxisn[2]);
    return FITS_INVALIDDATA;
  }

  if (header->blank_found && (header->bitpix == -32 || header->bitpix == -64)) {
    log_message("BLANK keyword found but BITPIX = %d\n. Ignoring BLANK", header->bitpix);
    header->blank_found = 0;
  }

  size = abs(header->bitpix) >> 3;
  for (int i = 0; i < header->naxis; i++) {
    if (size && (size_t)header->naxisn[i] > SIZE_MAX / size) {
      log_message("unsupported size of FITS image");
      return FITS_INVALIDDATA;
    }
    size *= header->naxisn[i];
  }
  return FITS_OK;
}

int fits_get_buffer_size(fits_header *header) {
  int size = abs(header->bitpix) / 8;
  for (int i = 0; i < header->naxis; i++) {
    size *= header->naxisn[i];
  }
  return size;
}

int fits_process_data(const uint8_t *fits_data, int fits_size, fits_header *header, char *native_data) {
  int little_endian = 1;
  int size = 1;
  for (int i = 0; i < header->naxis; i++) {
    size *= header->naxisn[i];
  }

  log_message("size = %d min_size = %d fits_size = %d\n", size, size * abs(header->bitpix) / 8 + header->data_offset, fits_size);
  if ((size * abs(header->bitpix) / 8 + header->data_offset) > fits_size) {
    return FITS_INVALIDDATA;
  }

  if (header->bitpix == -32 && header->naxis > 0) {
    float *raw = (float *)(fits_data + header->data_offset);
    float *native = (float *)native_data;
    if (little_endian) {
      for (int i = 0; i < size; i++) {
        char *rawc = (char *)raw;
        char *nativec = (char *)native;
        nativec[0] = rawc[3];
        nativec[1] = rawc[2];
        nativec[2] = rawc[1];
        nativec[3] = rawc[0];
        *native = (*native + header->bzero) * header->bscale;
        native++;
        raw++;
      }
    } else {
      for (int i = 0; i < size; i++) {
        *native++ = (*raw++ + header->bzero) * header->bscale;
      }
    }
    return FITS_OK;
  } else if (header->bitpix == 32 && header->naxis > 0) {
    int32_t *raw = (int32_t *)(fits_data + header->data_offset);
    int32_t *native = (int32_t *)native_data;
    if (little_endian) {
      for (int i = 0; i < size; i++) {
        char *rawc = (char *)raw;
        char *nativec = (char *)native;
        nativec[0] = rawc[3];
        nativec[1] = rawc[2];
        nativec[2] = rawc[1];
        nativec[3] = rawc[0];
        *native = (uint32_t)(*native + header->bzero) * header->bscale;
        native++;
        raw++;
      }
    } else {
      for (int i = 0; i < size; i++) {
        *native++ = (*raw++ + header->bzero) * header->bscale;
      }
    }
    return FITS_OK;
  } else if (header->bitpix == 16 && header->naxis > 0) {
    short *raw = (short *)(fits_data + header->data_offset);
    short *native = (short *)native_data;
    if (little_endian) {
      for (int i = 0; i < size; i++) {
        *native = (*raw & 0xff) << 8 | (*raw & 0xff00) >> 8;
        *native = (*native + header->bzero) * header->bscale;
        native++;
        raw++;
      }
    } else {
      for (int i = 0; i < size; i++) {
        *native++ = (*raw++ + header->bzero) * header->bscale;
      }
    }
    return FITS_OK;
  } else if (header->bitpix == 8 && header->naxis > 0) {
    uint8_t *raw = (uint8_t *)(fits_data + header->data_offset);
    uint8_t *native = (uint8_t *)native_data;
    for (int i = 0; i < size; i++) {
      *native++ = (*raw++ + header->bzero) * header->bscale;
    }
    return FITS_OK;
  }
  return FITS_INVALIDDATA;
}