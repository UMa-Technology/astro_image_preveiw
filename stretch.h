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

#pragma once

#include "common.h"
#include "rgb.h"

#define DEFAULT_B (0.25)
#define DEFAULT_C (-2.8)

struct StretchParams1Channel {
  // Stretch algorithm parameters
  float shadows;
  float highlights;
  float midtones;
  // The extension parameters are not used.
  float shadows_expansion;
  float highlights_expansion;

  // The default parameters result in no stretch at all.
  StretchParams1Channel() {
    shadows = 0.0;
    highlights = 1.0;
    midtones = 0.5;
    shadows_expansion = 0.0;
    highlights_expansion = 1.0;
  }
};

struct StretchParams {
  StretchParams1Channel *refChannel;
  StretchParams1Channel grey_red;
  StretchParams1Channel green;
  StretchParams1Channel blue;

  StretchParams() {
    refChannel = nullptr;
  }
};

class Stretcher {
public:
  explicit Stretcher(int width, int height, int pix_fmt);
  ~Stretcher() {}

  void setParams(StretchParams input_params) { m_params = input_params; }
  StretchParams getParams() { return m_params; }
  StretchParams computeParams(const uint8_t *input, const float B = DEFAULT_B, const float C = DEFAULT_C);
  void stretch(uint8_t const *input, void *_outputImage, int sampling = 1);

private:
  int m_image_width;
  int m_image_height;
  double m_input_range;
  uint32_t m_pix_fmt;

  StretchParams m_params;
};
