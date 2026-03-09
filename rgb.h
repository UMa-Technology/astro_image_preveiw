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

#ifndef RGB_H
#define RGB_H

typedef unsigned int rgb_triplet; // RGB triplet

#ifdef __cplusplus
extern "C" {
#endif

inline int red(rgb_triplet rgb) // get red part of RGB
{
  return ((rgb >> 16) & 0xff);
}

inline int green(rgb_triplet rgb) // get green part of RGB
{
  return ((rgb >> 8) & 0xff);
}

inline int blue(rgb_triplet rgb) // get blue part of RGB
{
  return (rgb & 0xff);
}

inline int alpha(rgb_triplet rgb) // get alpha part of RGBA
{
  return rgb >> 24;
}

inline rgb_triplet rgb(int r, int g, int b) // set RGB value
{
  return (0xffu << 24) | ((r & 0xffu) << 16) | ((g & 0xffu) << 8) | (b & 0xffu);
}

#ifdef __cplusplus
}
#endif

#endif // RGB_H
