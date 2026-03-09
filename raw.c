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

#include "raw.h"

static raw_str raw_buffers[] = {
    {RAW_MONO8, "RAW_MONO8"},
    {RAW_MONO16, "RAW_MONO16"},
    {RAW_RGB24, "RAW_RGB24"},
    {RAW_RGBA32, "RAW_RGBA32"},
    {RAW_ABGR32, "RAW_ABGR32"},
    {RAW_RGB48, "RAW_RGB48"}};

const char *raw_str_get(raw_type type) {
  for (size_t i = 0; i < sizeof(raw_buffers) / sizeof(raw_str); i++) {
    if (raw_buffers[i].type == type) {
      return raw_buffers[i].str;
    }
  }
  return "UNKNOWN";
}

static void raw_parse_appendix(const char *appendix, int appendix_len, cJSON *root) {
  if (!appendix || appendix_len <= 9)
    return;

  char *buf = (char *)malloc(appendix_len + 1);
  if (!buf)
    return;
  memcpy(buf, appendix, appendix_len);
  buf[appendix_len] = '\0';

  char *ptr = buf;
  if (strncmp(ptr, "SIMPLE=T;", 9) != 0) {
    free(buf);
    return;
  }
  ptr += 9;

  while (*ptr) {
    char *eq = strchr(ptr, '=');
    if (!eq)
      break;
    char *semi = strchr(eq, ';');
    if (!semi)
      break;

    *eq = '\0';
    *semi = '\0';
    char *key = ptr;
    char *val = eq + 1;

    size_t vlen = strlen(val);
    if (vlen >= 2 && val[0] == '\'' && val[vlen - 1] == '\'') {
      val[vlen - 1] = '\0';
      val++;
      cJSON_AddStringToObject(root, key, val);
    } else {
      char *endp;
      long lv = strtol(val, &endp, 10);
      if (*endp == '\0')
        cJSON_AddNumberToObject(root, key, lv);
      else
        cJSON_AddStringToObject(root, key, val);
    }

    ptr = semi + 1;
  }
  free(buf);
}

void get_raw_header(raw_header *header, const char *appendix, int appendix_len, char *buffer) {
  if (!buffer)
    return;
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "SIGNATURE", raw_str_get(header->signature));
  cJSON_AddNumberToObject(root, "WIDTH", header->width);
  cJSON_AddNumberToObject(root, "HEIGHT", header->height);

  raw_parse_appendix(appendix, appendix_len, root);

  char *json_str = cJSON_PrintUnformatted(root);
  strcpy(buffer, json_str);

  cJSON_Delete(root);
  free(json_str);
}