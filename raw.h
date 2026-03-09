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

#ifndef RAW_H
#define RAW_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  RAW_MONO8 = 0x31574152,
  RAW_MONO16 = 0x32574152,
  RAW_RGB24 = 0x33574152,
  RAW_RGBA32 = 0x36424752,
  RAW_ABGR32 = 0x36524742,
  RAW_RGB48 = 0x36574152
} raw_type;

typedef struct raw_str {
  raw_type type;
  const char *str;
} raw_str;

typedef struct
{
  uint32_t signature; // 8bit mono = RAW1 = 0x31574152, 16bit mono = RAW2 = 0x32574152, 24bit RGB = RAW3 = 0x33574152
  uint32_t width;
  uint32_t height;
} raw_header;

static inline void *safe_malloc(size_t size) {
  void *pointer = malloc(size);
  assert(pointer != NULL);
  memset(pointer, 0, size);
  return pointer;
}

static inline void safe_free(void *pointer) {
  if (pointer) {
    free(pointer);
  }
}

void get_raw_header(raw_header *header, const char *appendix, int appendix_len, char *buffer);

#ifdef __cplusplus
}
#endif

#endif // RAW_H
