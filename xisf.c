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

#include "xisf.h"

static int xisf_metadata_init(xisf_metadata *metadata) {
  metadata->bitpix = 0;
  metadata->width = 0;
  metadata->height = 0;
  metadata->channels = 0;
  metadata->big_endian = false;           // default is little endian
  metadata->normal_pixel_storage = false; // planar is default
  metadata->data_offset = 0;
  metadata->data_size = 0;
  metadata->uncompressed_data_size = 0;
  metadata->shuffle_size = 0;
  metadata->compression[0] = '\0';
  metadata->color_space[0] = '\0';
  metadata->bayer_pattern[0] = '\0';
  metadata->camera_name[0] = '\0';
  metadata->image_type[0] = '\0';
  metadata->observation_time[0] = '\0';
  metadata->exposure_time = -1;
  metadata->sensor_temperature = -1;

  metadata->gain = -1;
  metadata->egain = -1;
  metadata->offset = -1;
  metadata->gamma = -1;
  metadata->xpixsz = -1;
  metadata->ypixsz = -1;
  metadata->xbinning = -1;
  metadata->ybinning = -1;
  metadata->aptdia = -1;
  metadata->focallen = -1;
  metadata->jd = -1;
  metadata->date_obs[0] = '\0';
  metadata->instrume[0] = '\0';
  metadata->object[0] = '\0';

  metadata->nkeywords = 0;
  return 0;
}

static void un_shuffle(uint8_t *output, const uint8_t *input, size_t size, size_t item_size) {
  if (size > 0) {
    if (item_size > 0) {
      if (input != NULL) {
        if (output != NULL) {
          size_t items = size / item_size;
          const uint8_t *s = input;
          for (size_t j = 0; j < item_size; ++j) {
            uint8_t *u = output + j;
            for (size_t i = 0; i < items; ++i, ++s, u += item_size) {
              *u = *s;
            }
          }
          memcpy(output + items * item_size, s, size % item_size);
        }
      }
    }
  }
}

int xisf_read_metadata(uint8_t *xisf_data, int xisf_size, xisf_metadata *metadata) {
  if (!xisf_data || !xisf_size || !metadata) {
    return XISF_INVALIDPARAM;
  }

  xisf_metadata_init(metadata);

  xisf_header *header = (xisf_header *)xisf_data;

  if (strncmp(header->signature, "XISF0100", 8)) {
    return XISF_NOT_XISF;
  }

  uint32_t xml_offset = 0;
  while (strncmp((char *)xisf_data + xml_offset, "<xisf", 5)) {
    xml_offset++;
    if (xml_offset > header->xml_length) {
      return XISF_NOT_XISF;
    }
  }

  struct xml_document *document = xml_parse_document(xisf_data + xml_offset, header->xml_length);

  if (!document) {
    return XISF_INVALIDDATA;
  }

  struct xml_node *xisf_root = xml_document_root(document);
  struct xml_node *node_image = NULL;

  int nodes = xml_node_children(xisf_root);
  for (int i = 0; i < nodes; i++) {
    struct xml_node *child = xml_node_child(xisf_root, i);
    struct xml_string *node_name_s = xml_node_name(child);
    char *node_name = calloc(xml_string_length(node_name_s) + 1, sizeof(uint8_t));
    xml_string_copy(node_name_s, (uint8_t *)node_name, xml_string_length(node_name_s));
    if (!strcmp(node_name, "Image")) {
      node_image = child;
      free(node_name);
      break;
    }
    free(node_name);
  }
  if (node_image == NULL) {
    xml_document_free(document, false);
    return XISF_INVALIDDATA;
  }

  int attr = xml_node_attributes(node_image);
  for (int i = 0; i < attr; i++) {
    struct xml_string *attr_name_s = xml_node_attribute_name(node_image, i);
    char *name = calloc(xml_string_length(attr_name_s) + 1, sizeof(uint8_t));
    xml_string_copy(attr_name_s, (uint8_t *)name, xml_string_length(attr_name_s));

    struct xml_string *attr_content_s = xml_node_attribute_content(node_image, i);
    char *content = calloc(xml_string_length(attr_content_s) + 1, sizeof(uint8_t));
    xml_string_copy(attr_content_s, (uint8_t *)content, xml_string_length(attr_content_s));

    if (!strncmp(name, "geometry", strlen(name))) {
      int width = 0, height = 0, channels = 0;
      int scanned = sscanf(content, "%d:%d:%d", &width, &height, &channels);
      if (scanned != 3) {
        xml_document_free(document, false);
        return XISF_INVALIDDATA;
      }
      if (channels != 1 && channels != 3) {
        xml_document_free(document, false);
        return XISF_UNSUPPORTED;
      }
      metadata->width = width;
      metadata->height = height;
      metadata->channels = channels;
    } else if (!strncmp(name, "sampleFormat", strlen(name))) {
      if (!strncmp(content, "UInt8", strlen(content))) {
        metadata->bitpix = 8;
      } else if (!strncmp(content, "UInt16", strlen(content))) {
        metadata->bitpix = 16;
      } else if (!strncmp(content, "UInt32", strlen(content))) {
        metadata->bitpix = 32;
      } else if (!strncmp(content, "Float32", strlen(content))) {
        metadata->bitpix = -32;
      } else if (!strncmp(content, "Float64", strlen(content))) {
        metadata->bitpix = -64;
      }
    } else if (!strncmp(name, "pixelStorage", strlen(name))) {
      if (!strncmp(content, "Normal", strlen(content))) {
        metadata->normal_pixel_storage = true;
      } else if (!strncmp(content, "Planar", strlen(content))) {
        metadata->normal_pixel_storage = false;
      }
    } else if (!strncmp(name, "byteOrder", strlen(name))) {
      if (!strncmp(content, "big", strlen(content))) {
        metadata->big_endian = true;
      } else if (!strncmp(content, "little", strlen(content))) {
        metadata->big_endian = false;
      }
    } else if (!strncmp(name, "colorSpace", strlen(name))) {
      strncpy(metadata->color_space, content, sizeof(metadata->color_space) - 1);
      metadata->color_space[sizeof(metadata->color_space) - 1] = '\0';
    } else if (!strncmp(name, "imageType", strlen(name))) {
      strncpy(metadata->image_type, content, sizeof(metadata->image_type) - 1);
      metadata->image_type[sizeof(metadata->image_type) - 1] = '\0';
    } else if (!strncmp(name, "location", strlen(name))) {
      char location[100] = {0};
      int data_offset = 0;
      int data_size = 0;
      int scanned = sscanf(content, "%30[^:]:%d:%d", location, &data_offset, &data_size);
      if (scanned != 3) {
        xml_document_free(document, false);
        return XISF_INVALIDDATA;
      }
      if (strncmp(location, "attachment", strlen(location))) {
        xml_document_free(document, false);
        return XISF_UNSUPPORTED;
      }
      metadata->data_offset = data_offset;
      metadata->data_size = data_size;
    } else if (!strncmp(name, "compression", strlen(name))) {
      char compression[30] = {0};
      int data_size = 0;
      int shuffle_size = 0;
      int scanned = sscanf(content, "%29[^:]:%d:%d", compression, &data_size, &shuffle_size);
      if (scanned != 2 && scanned != 3) {
        xml_document_free(document, false);
        return XISF_INVALIDDATA;
      }
      strncpy(metadata->compression, compression, sizeof(metadata->compression));
      metadata->uncompressed_data_size = data_size;
      metadata->shuffle_size = shuffle_size;
    }
    free(name);
    free(content);

    nodes = xml_node_children(node_image);
    for (int j = 0; j < nodes; j++) {
      struct xml_node *child = xml_node_child(node_image, j);
      struct xml_string *node_name_s = xml_node_name(child);
      char *node_name = calloc(xml_string_length(node_name_s) + 1, sizeof(uint8_t));
      xml_string_copy(node_name_s, (uint8_t *)node_name, xml_string_length(node_name_s));

      if (!strcmp(node_name, "ColorFilterArray")) {
        int attr = xml_node_attributes(child);
        for (int i = 0; i < attr; i++) {
          struct xml_string *attr_name_s = xml_node_attribute_name(child, i);
          char *attr_name = calloc(xml_string_length(attr_name_s) + 1, sizeof(uint8_t));
          xml_string_copy(attr_name_s, (uint8_t *)attr_name, xml_string_length(attr_name_s));

          struct xml_string *attr_content_s = xml_node_attribute_content(child, i);
          char *attr_content = calloc(xml_string_length(attr_content_s) + 1, sizeof(uint8_t));
          xml_string_copy(attr_content_s, (uint8_t *)attr_content, xml_string_length(attr_content_s));

          if (!strcmp(attr_name, "pattern")) {
            strncpy(metadata->bayer_pattern, attr_content, sizeof(metadata->bayer_pattern) - 1);
            metadata->bayer_pattern[sizeof(metadata->bayer_pattern) - 1] = '\0';
            if (metadata->nkeywords < XISF_MAX_KEYWORDS) {
              strcpy(metadata->fits_names[metadata->nkeywords], "BAYERPAT");
              strncpy(metadata->fits_values[metadata->nkeywords], attr_content, 79);
              metadata->fits_values[metadata->nkeywords][79] = '\0';
              metadata->nkeywords++;
            }
          }

          free(attr_name);
          free(attr_content);
        }
      } else if (!strcmp(node_name, "Property")) {
        int attr = xml_node_attributes(child);
        char id[255] = {0};
        char value[255] = {0};
        for (int i = 0; i < attr; i++) {
          struct xml_string *attr_name_s = xml_node_attribute_name(child, i);
          char *attr_name = calloc(xml_string_length(attr_name_s) + 1, sizeof(uint8_t));
          xml_string_copy(attr_name_s, (uint8_t *)attr_name, xml_string_length(attr_name_s));

          struct xml_string *attr_content_s = xml_node_attribute_content(child, i);
          char *attr_content = calloc(xml_string_length(attr_content_s) + 1, sizeof(uint8_t));
          xml_string_copy(attr_content_s, (uint8_t *)attr_content, xml_string_length(attr_content_s));

          if (!strcmp(attr_name, "id")) {
            strncpy(id, attr_content, sizeof(id) - 1);
            id[sizeof(id) - 1] = '\0';
          }
          if (!strcmp(attr_name, "value")) {
            strncpy(value, attr_content, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';
          }

          free(attr_name);
          free(attr_content);
        }

        struct xml_string *node_content_s = xml_node_content(child);
        char *node_content = calloc(xml_string_length(node_content_s) + 1, sizeof(uint8_t));
        xml_string_copy(node_content_s, (uint8_t *)node_content, xml_string_length(node_content_s));

        if (!strcmp(id, "Instrument:Camera:Name")) {
          strncpy(metadata->camera_name, node_content, sizeof(metadata->camera_name) - 1);
          metadata->camera_name[sizeof(metadata->camera_name) - 1] = '\0';
        } else if (!strcmp(id, "Instrument:ExposureTime")) {
          metadata->exposure_time = atof(value);
        } else if (!strcmp(id, "Instrument:Sensor:Temperature")) {
          metadata->sensor_temperature = atof(value);
        } else if (!strcmp(id, "Observation:Time:Start")) {
          strncpy(metadata->observation_time, value, sizeof(metadata->observation_time) - 1);
          metadata->observation_time[sizeof(metadata->observation_time) - 1] = '\0';
        } else if (!strcmp(id, "PCL:CFASourcePattern") && (metadata->bayer_pattern[0] == '\0') && !strcmp(metadata->color_space, "Gray")) {
          // Pixinsight does not follow its own specs. It writes PCL:CFASourcePattern. It writes it even with debayered images!!!
          strncpy(metadata->bayer_pattern, node_content, sizeof(metadata->bayer_pattern) - 1);
          metadata->bayer_pattern[sizeof(metadata->bayer_pattern) - 1] = '\0';
        }
        // Parse camera settings from Property elements
        else if (!strcmp(id, "Instrument:Camera:Gain")) {
          metadata->gain = atof(value);
        } else if (!strcmp(id, "Instrument:Camera:Offset")) {
          metadata->offset = atof(value);
        } else if (!strcmp(id, "Instrument:Camera:Gamma")) {
          metadata->gamma = atof(value);
        } else if (!strcmp(id, "Instrument:Sensor:XPixelSize")) {
          metadata->xpixsz = atof(value);
        } else if (!strcmp(id, "Instrument:Sensor:YPixelSize")) {
          metadata->ypixsz = atof(value);
        } else if (!strcmp(id, "Instrument:Camera:XBinning")) {
          metadata->xbinning = atoi(value);
        } else if (!strcmp(id, "Instrument:Camera:YBinning")) {
          metadata->ybinning = atoi(value);
        } else if (!strcmp(id, "Instrument:Camera:Aperture")) {
          metadata->aptdia = atof(value) * 1000; // Convert m to mm
        } else if (!strcmp(id, "Instrument:Camera:FocalLength")) {
          metadata->focallen = atof(value) * 1000; // Convert m to mm
        }

        free(node_content);
      } else if (!strcmp(node_name, "FITSKeyword")) {
        int attr = xml_node_attributes(child);
        char name[255] = {0};
        char value[255] = {0};
        for (int i = 0; i < attr; i++) {
          struct xml_string *attr_name_s = xml_node_attribute_name(child, i);
          char *attr_name = calloc(xml_string_length(attr_name_s) + 1, sizeof(uint8_t));
          xml_string_copy(attr_name_s, (uint8_t *)attr_name, xml_string_length(attr_name_s));

          struct xml_string *attr_content_s = xml_node_attribute_content(child, i);
          char *attr_content = calloc(xml_string_length(attr_content_s) + 1, sizeof(uint8_t));
          xml_string_copy(attr_content_s, (uint8_t *)attr_content, xml_string_length(attr_content_s));

          if (!strcmp(attr_name, "name")) {
            strncpy(name, attr_content, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
          }
          if (!strcmp(attr_name, "value")) {
            strncpy(value, attr_content, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';
          }

          free(attr_name);
          free(attr_content);
        }

        // Store FITSKeyword to generic array (skip if already exists)
        if (name[0] != '\0' && metadata->nkeywords < XISF_MAX_KEYWORDS) {
          // Check if keyword already exists
          int exists = 0;
          for (int k = 0; k < metadata->nkeywords; k++) {
            if (!strcmp(metadata->fits_names[k], name)) {
              exists = 1;
              break;
            }
          }
          if (!exists) {
            // Copy name (max 15 chars + null)
            size_t name_len = strlen(name);
            if (name_len > 15) name_len = 15;
            memcpy(metadata->fits_names[metadata->nkeywords], name, name_len);
            metadata->fits_names[metadata->nkeywords][name_len] = '\0';
            // Clean up value: remove surrounding quotes and trim leading/trailing spaces
            char *val_start = value;
            if (*val_start == '\'') val_start++;
            while (*val_start == ' ') val_start++;  // Skip leading spaces
            size_t val_len = strlen(val_start);
            if (val_len > 79) val_len = 79;
            memcpy(metadata->fits_values[metadata->nkeywords], val_start, val_len);
            metadata->fits_values[metadata->nkeywords][val_len] = '\0';
            // Remove trailing quote and spaces
            char *val_end = strchr(metadata->fits_values[metadata->nkeywords], '\'');
            if (val_end) *val_end = '\0';
            val_len = strlen(metadata->fits_values[metadata->nkeywords]);
            while (val_len > 0 && metadata->fits_values[metadata->nkeywords][val_len - 1] == ' ') {
              metadata->fits_values[metadata->nkeywords][--val_len] = '\0';
            }
            metadata->nkeywords++;
          }
        }

        // Parse specific keywords for typed fields
        if (!strcmp(name, "BAYERPAT") && (metadata->bayer_pattern[0] == '\0') && !strcmp(metadata->color_space, "Gray")) {
          // Pixinsight does not copy this from FITS so if not found, copy it from fits header.
          // Value is written like "'RGGB    '" so remove single quotes and spaces first
          char *start = value;
          while (*start == '\'' || *start == ' ' || *start == '\0')
            start++;
          char *end = start;
          while (*end != '\'' && *end != ' ' && *end != '\0')
            end++;
          *end = '\0';
          strncpy(metadata->bayer_pattern, start, sizeof(metadata->bayer_pattern) - 1);
          metadata->bayer_pattern[sizeof(metadata->bayer_pattern) - 1] = '\0';
        }
        // Parse camera settings from FITSKeyword elements (fallback if Property not present)
        else if (!strcmp(name, "GAIN") && metadata->gain < 0) {
          metadata->gain = atof(value);
        } else if (!strcmp(name, "EGAIN") && metadata->egain < 0) {
          metadata->egain = atof(value);
        } else if (!strcmp(name, "OFFSET") && metadata->offset < 0) {
          metadata->offset = atof(value);
        } else if (!strcmp(name, "GAMMA") && metadata->gamma < 0) {
          metadata->gamma = atof(value);
        } else if (!strcmp(name, "XPIXSZ") && metadata->xpixsz < 0) {
          metadata->xpixsz = atof(value);
        } else if (!strcmp(name, "YPIXSZ") && metadata->ypixsz < 0) {
          metadata->ypixsz = atof(value);
        } else if (!strcmp(name, "XBINNING") && metadata->xbinning < 0) {
          metadata->xbinning = atoi(value);
        } else if (!strcmp(name, "YBINNING") && metadata->ybinning < 0) {
          metadata->ybinning = atoi(value);
        } else if (!strcmp(name, "APTDIA") && metadata->aptdia < 0) {
          metadata->aptdia = atof(value);
        } else if (!strcmp(name, "FOCALLEN") && metadata->focallen < 0) {
          metadata->focallen = atof(value);
        } else if (!strcmp(name, "JD") && metadata->jd < 0) {
          metadata->jd = atof(value);
        } else if (!strcmp(name, "DATE-OBS") && metadata->date_obs[0] == '\0') {
          char *start = value;
          if (*start == '\'') start++;
          strncpy(metadata->date_obs, start, sizeof(metadata->date_obs) - 1);
          metadata->date_obs[sizeof(metadata->date_obs) - 1] = '\0';
          char *end = strchr(metadata->date_obs, '\'');
          if (end) *end = '\0';
        } else if (!strcmp(name, "INSTRUME") && metadata->instrume[0] == '\0') {
          char *start = value;
          if (*start == '\'') start++;
          strncpy(metadata->instrume, start, sizeof(metadata->instrume) - 1);
          metadata->instrume[sizeof(metadata->instrume) - 1] = '\0';
          char *end = strchr(metadata->instrume, '\'');
          if (end) *end = '\0';
        } else if (!strcmp(name, "OBJECT") && metadata->object[0] == '\0') {
          char *start = value;
          if (*start == '\'') start++;
          strncpy(metadata->object, start, sizeof(metadata->object) - 1);
          metadata->object[sizeof(metadata->object) - 1] = '\0';
          char *end = strchr(metadata->object, '\'');
          if (end) *end = '\0';
        }
      }
      free(node_name);
    }
  }
  xml_document_free(document, false);
  return XISF_OK;
}

void xisf_get_header(xisf_metadata *metadata, char *buffer) {
  if (!buffer)
    return;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "SIGNATURE", "XISF0100");
  cJSON_AddNumberToObject(root, "WIDTH", metadata->width);
  cJSON_AddNumberToObject(root, "HEIGHT", metadata->height);
  cJSON_AddNumberToObject(root, "CHANNELS", metadata->channels);
  cJSON_AddNumberToObject(root, "SAMPLEFORMAT", metadata->bitpix);

  if (metadata->normal_pixel_storage) {
    cJSON_AddStringToObject(root, "PIXELSTORAGE", "Normal");
  } else {
    cJSON_AddStringToObject(root, "PIXELSTORAGE", "Planar");
  }
  if (metadata->big_endian) {
    cJSON_AddStringToObject(root, "BYTEORDER", "big");
  } else {
    cJSON_AddStringToObject(root, "BYTEORDER", "little");
  }
  // XISF-specific metadata (internal use only, not FITS keywords)
  // cJSON_AddStringToObject(root, "COLORSPACE", metadata->color_space);
  cJSON_AddNumberToObject(root, "DATA_OFFSET", metadata->data_offset);
  cJSON_AddNumberToObject(root, "SIZE", metadata->data_size);
  if (metadata->compression[0] != '\0') cJSON_AddStringToObject(root, "COMPRESSION", metadata->compression);
  if (metadata->uncompressed_data_size > 0) cJSON_AddNumberToObject(root, "UNCOMPRESSED_SIZE", metadata->uncompressed_data_size);
  if (metadata->shuffle_size > 0) cJSON_AddNumberToObject(root, "SHUFFLE_SIZE", metadata->shuffle_size);

  for (int i = 0; i < metadata->nkeywords; i++) {
    cJSON_AddStringToObject(root, metadata->fits_names[i], metadata->fits_values[i]);
  }

  if (metadata->nkeywords == 0) {
    if (metadata->image_type[0] != '\0') cJSON_AddStringToObject(root, "IMAGETYP", metadata->image_type);
    if (metadata->bayer_pattern[0] != '\0') cJSON_AddStringToObject(root, "BAYERPAT", metadata->bayer_pattern);
    if (metadata->camera_name[0] != '\0') cJSON_AddStringToObject(root, "INSTRUME", metadata->camera_name);
    if (metadata->observation_time[0] != '\0') cJSON_AddStringToObject(root, "DATE-OBS", metadata->observation_time);
  }

  char *json_str = cJSON_PrintUnformatted(root);
  strcpy(buffer, json_str);

  cJSON_Delete(root);
  free(json_str);
}

int xisf_decompress(uint8_t *xisf_data, xisf_metadata *metadata, uint8_t *decompressed_data) {
  size_t uncompressed_data_size = metadata->uncompressed_data_size;
  if (!strcmp(metadata->compression, "zlib")) {
    int err = uncompress(decompressed_data, &uncompressed_data_size, xisf_data + metadata->data_offset, metadata->data_size);
    if (err == Z_OK) {
      return XISF_OK;
    } else {
      return XISF_INVALIDDATA;
    }
  } else if (!strcmp(metadata->compression, "zlib+sh")) {
    uint8_t *shuffled_data = (uint8_t *)malloc(uncompressed_data_size);
    int err = uncompress(shuffled_data, &uncompressed_data_size, xisf_data + metadata->data_offset, metadata->data_size);
    if (err != Z_OK) {
      free(shuffled_data);
      return XISF_INVALIDDATA;
    }
    un_shuffle(decompressed_data, shuffled_data, uncompressed_data_size, metadata->shuffle_size);
    free(shuffled_data);
    return XISF_OK;
  } else if (!strcmp(metadata->compression, "lz4") || !strcmp(metadata->compression, "lz4hc")) {
    int result = LZ4_decompress_safe((const char *)(xisf_data + metadata->data_offset), (char *)decompressed_data, metadata->data_size, uncompressed_data_size);
    if (result > 0) {
      return XISF_OK;
    } else {
      return XISF_INVALIDDATA;
    }
  } else if (!strcmp(metadata->compression, "lz4+sh") || !strcmp(metadata->compression, "lz4hc+sh")) {
    uint8_t *shuffled_data = (uint8_t *)malloc(uncompressed_data_size);
    int result = LZ4_decompress_safe((const char *)(xisf_data + metadata->data_offset), (char *)shuffled_data, metadata->data_size, uncompressed_data_size);
    if (result <= 0) {
      free(shuffled_data);
      return XISF_INVALIDDATA;
    }
    un_shuffle(decompressed_data, shuffled_data, uncompressed_data_size, metadata->shuffle_size);
    free(shuffled_data);
    return XISF_OK;
  }
  return XISF_UNSUPPORTED;
}
