// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
// Gotten via https://stackoverflow.com/a/22135005

#ifndef INCLUDED_UTF8_DOT_H
#define INCLUDED_UTF8_DOT_H

#include <stddef.h>
#include <stdint.h>

#include "clients.h"

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uint8_t utf8d[] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 00..1f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 20..3f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 40..5f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 60..7f
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,  // 80..9f
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,  // a0..bf
    8,   8,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,    // c0..df
    0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x4, 0x3, 0x3,  // e0..ef
    0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,  // f0..ff
    0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4, 0x6, 0x1, 0x1, 0x1, 0x1,  // s0..s0
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   0,   1,   1,   1,   1,   1,   0,   1,   0,   1,   1,   1,   1,   1,   1,  // s1..s2
    1,   2,   1,   1,   1,   1,   1,   2,   1,   2,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   2,   1,   1,   1,   1,   1,   1,   1,   1,  // s3..s4
    1,   2,   1,   1,   1,   1,   1,   1,   1,   2,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   3,   1,   3,   1,   1,   1,   1,   1,   1,  // s5..s6
    1,   3,   1,   1,   1,   1,   1,   3,   1,   3,   1,   1,   1,   1,   1,   1,
    1,   3,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,  // s7..s8
};

bool validate_utf8(const char *str, size_t len) {
  size_t i;
  uint32_t type;
  uint8_t state = UTF8_ACCEPT;

  for (i = 0; i < len; i++) {
    // We don't care about the codepoint, so this is
    // a simplified version of the decode function.
    type = utf8d[(uint8_t)str[i]];
    state = utf8d[256 + state * 16 + type];

    if (state == UTF8_REJECT) {
      break;
    }
  }

  return state == UTF8_ACCEPT;
}
#endif