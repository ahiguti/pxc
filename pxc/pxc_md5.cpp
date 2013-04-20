/*
 *  This file is part of PXC, the p extension compiler.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 *
 *  This file is part of ASE, the abstract script engines.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 */


// vim:sw=2:ts=8:ai

// imported from gws/aioport

#include "pxc_md5.hpp"

namespace pxc {

static const unsigned char md5_padding[64] = {
  128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void
md5_context::initialize()
{
  ma = 0x67452301UL;
  mb = 0xefcdab89UL;
  mc = 0x98badcfeUL;
  md = 0x10325476UL;

  mlength = mlength_high = 0;

  for (unsigned long i = 0; i < 64; i++) {
    buffer[i] = 0;
  }
}

md5_context::md5_context()
{
  initialize();
}

void
md5_context::update(const std::string& str)
{
  update_raw(reinterpret_cast<const unsigned char *>(str.data()),
    str.size());
}

std::string
md5_context::final()
{
  unsigned char buf[8]; /* STACK BUFFER */
  unsigned long buflen = mlength & 63;
  unsigned long padlen = (buflen < 56) ? (56 - buflen) : (120 - buflen);
  mlength &= 0xffffffffUL;
  buf[0] = static_cast<unsigned char>(mlength <<  3);
  buf[1] = static_cast<unsigned char>(mlength >>  5);
  buf[2] = static_cast<unsigned char>(mlength >> 13);
  buf[3] = static_cast<unsigned char>(mlength >> 21);
  buf[4] = static_cast<unsigned char>((mlength >> 29) | (mlength_high << 3));
  buf[5] = static_cast<unsigned char>(mlength_high >>  5);
  buf[6] = static_cast<unsigned char>(mlength_high >> 13);
  buf[7] = static_cast<unsigned char>(mlength_high >> 21);
  update_raw(md5_padding, padlen);
  update_raw(buf, 8);
  write_result(buffer);
  const char *const bp = reinterpret_cast<const char *>(&buffer[0]);
  const std::string s(bp, 16);
  initialize();
  return s;
}

static void
u32_to_uchar_le(unsigned long val, unsigned char ptr[4])
{
  ptr[0] = static_cast<unsigned char>(val);
  ptr[1] = static_cast<unsigned char>(val >> 8);
  ptr[2] = static_cast<unsigned char>(val >> 16);
  ptr[3] = static_cast<unsigned char>(val >> 24);
}

void
md5_context::write_result(unsigned char ptr[16])
{
  u32_to_uchar_le(ma, ptr +  0);
  u32_to_uchar_le(mb, ptr +  4);
  u32_to_uchar_le(mc, ptr +  8);
  u32_to_uchar_le(md, ptr + 12);
}

unsigned long
carry_add(unsigned long x, unsigned long y)
{
  unsigned long nx = (x + y) & 0xffffffffUL;
  return (nx < x);
}

void
md5_context::update_raw(const unsigned char *ptr, size_t plen)
{
  size_t buflen = mlength & 63;
  mlength_high += carry_add(mlength, static_cast<unsigned long>(plen));
  mlength += static_cast<unsigned long>(plen);
  if (buflen > 0) {
    while (buflen < 64 && plen > 0) {
      buffer[buflen++] = *ptr;
      ptr++;
      plen--;
    }
    if (buflen == 64) {
      update_block_ch(buffer);
    }
  }
  while (plen >= 64) {
    update_block_ch(ptr);
    ptr += 64;
    plen -= 64;
  }
  for (size_t i = 0; i < plen; i++) {
    buffer[i] = ptr[i];
  }
}

void
md5_context::update_block_ch(const unsigned char x[64])
{
  unsigned long y[16];
  for (unsigned long i = 0; i < 16; i++) {
    unsigned long j = i * 4;
    y[i] = (static_cast<unsigned long>(x[j + 0]) <<  0)
         | (static_cast<unsigned long>(x[j + 1]) <<  8)
         | (static_cast<unsigned long>(x[j + 2]) << 16)
         | (static_cast<unsigned long>(x[j + 3]) << 24);
  }
  update_block(y);
  for (unsigned long i = 0; i < 16; i++) {
    y[i] = 0;
  }
}

#define md5_mcr_f(x, y, z) ((x & y) | ((~x) & z))
#define md5_mcr_g(x, y, z) ((x & z) | (y & (~z)))
#define md5_mcr_h(x, y, z) (x ^ y ^ z)
#define md5_mcr_i(x, y, z) (y ^ (x | (~z)))

#define md5_mcr_rotate_left(v, n) ((v << n) | (v >> (32 - n)))

#define md5_mcr_x1(var, arg1, arg2, arg3) \
 (var = (var + arg1 + arg2 + arg3) & 0xffffffffUL)
#define md5_mcr_x2(var1, var2, var3) \
 (var1 = (var2 + md5_mcr_rotate_left(var1, var3)) & 0xffffffffUL)

#define md5_mcr_r1(a, b, c, d, o, s, v) \
  (md5_mcr_x1(a, md5_mcr_f(b, c, d), x[o], 0x##v##UL), md5_mcr_x2(a, b, s))
#define md5_mcr_r2(a, b, c, d, o, s, v) \
  (md5_mcr_x1(a, md5_mcr_g(b, c, d), x[o], 0x##v##UL), md5_mcr_x2(a, b, s))
#define md5_mcr_r3(a, b, c, d, o, s, v) \
  (md5_mcr_x1(a, md5_mcr_h(b, c, d), x[o], 0x##v##UL), md5_mcr_x2(a, b, s))
#define md5_mcr_r4(a, b, c, d, o, s, v) \
  (md5_mcr_x1(a, md5_mcr_i(b, c, d), x[o], 0x##v##UL), md5_mcr_x2(a, b, s))

void
md5_context::update_block(const unsigned long x[16])
{
  unsigned long a = ma, b = mb, c = mc, d = md;

  md5_mcr_r1(a,b,c,d, 0, 7,d76aa478);
  md5_mcr_r1(d,a,b,c, 1,12,e8c7b756);
  md5_mcr_r1(c,d,a,b, 2,17,242070db);
  md5_mcr_r1(b,c,d,a, 3,22,c1bdceee);
  md5_mcr_r1(a,b,c,d, 4, 7,f57c0faf);
  md5_mcr_r1(d,a,b,c, 5,12,4787c62a);
  md5_mcr_r1(c,d,a,b, 6,17,a8304613);
  md5_mcr_r1(b,c,d,a, 7,22,fd469501);
  md5_mcr_r1(a,b,c,d, 8, 7,698098d8);
  md5_mcr_r1(d,a,b,c, 9,12,8b44f7af);
  md5_mcr_r1(c,d,a,b,10,17,ffff5bb1);
  md5_mcr_r1(b,c,d,a,11,22,895cd7be);
  md5_mcr_r1(a,b,c,d,12, 7,6b901122);
  md5_mcr_r1(d,a,b,c,13,12,fd987193);
  md5_mcr_r1(c,d,a,b,14,17,a679438e);
  md5_mcr_r1(b,c,d,a,15,22,49b40821);
  md5_mcr_r2(a,b,c,d, 1, 5,f61e2562);
  md5_mcr_r2(d,a,b,c, 6, 9,c040b340);
  md5_mcr_r2(c,d,a,b,11,14,265e5a51);
  md5_mcr_r2(b,c,d,a, 0,20,e9b6c7aa);
  md5_mcr_r2(a,b,c,d, 5, 5,d62f105d);
  md5_mcr_r2(d,a,b,c,10, 9,02441453);
  md5_mcr_r2(c,d,a,b,15,14,d8a1e681);
  md5_mcr_r2(b,c,d,a, 4,20,e7d3fbc8);
  md5_mcr_r2(a,b,c,d, 9, 5,21e1cde6);
  md5_mcr_r2(d,a,b,c,14, 9,c33707d6);
  md5_mcr_r2(c,d,a,b, 3,14,f4d50d87);
  md5_mcr_r2(b,c,d,a, 8,20,455a14ed);
  md5_mcr_r2(a,b,c,d,13, 5,a9e3e905);
  md5_mcr_r2(d,a,b,c, 2, 9,fcefa3f8);
  md5_mcr_r2(c,d,a,b, 7,14,676f02d9);
  md5_mcr_r2(b,c,d,a,12,20,8d2a4c8a);
  md5_mcr_r3(a,b,c,d, 5, 4,fffa3942);
  md5_mcr_r3(d,a,b,c, 8,11,8771f681);
  md5_mcr_r3(c,d,a,b,11,16,6d9d6122);
  md5_mcr_r3(b,c,d,a,14,23,fde5380c);
  md5_mcr_r3(a,b,c,d, 1, 4,a4beea44);
  md5_mcr_r3(d,a,b,c, 4,11,4bdecfa9);
  md5_mcr_r3(c,d,a,b, 7,16,f6bb4b60);
  md5_mcr_r3(b,c,d,a,10,23,bebfbc70);
  md5_mcr_r3(a,b,c,d,13, 4,289b7ec6);
  md5_mcr_r3(d,a,b,c, 0,11,eaa127fa);
  md5_mcr_r3(c,d,a,b, 3,16,d4ef3085);
  md5_mcr_r3(b,c,d,a, 6,23,04881d05);
  md5_mcr_r3(a,b,c,d, 9, 4,d9d4d039);
  md5_mcr_r3(d,a,b,c,12,11,e6db99e5);
  md5_mcr_r3(c,d,a,b,15,16,1fa27cf8);
  md5_mcr_r3(b,c,d,a, 2,23,c4ac5665);
  md5_mcr_r4(a,b,c,d, 0, 6,f4292244);
  md5_mcr_r4(d,a,b,c, 7,10,432aff97);
  md5_mcr_r4(c,d,a,b,14,15,ab9423a7);
  md5_mcr_r4(b,c,d,a, 5,21,fc93a039);
  md5_mcr_r4(a,b,c,d,12, 6,655b59c3);
  md5_mcr_r4(d,a,b,c, 3,10,8f0ccc92);
  md5_mcr_r4(c,d,a,b,10,15,ffeff47d);
  md5_mcr_r4(b,c,d,a, 1,21,85845dd1);
  md5_mcr_r4(a,b,c,d, 8, 6,6fa87e4f);
  md5_mcr_r4(d,a,b,c,15,10,fe2ce6e0);
  md5_mcr_r4(c,d,a,b, 6,15,a3014314);
  md5_mcr_r4(b,c,d,a,13,21,4e0811a1);
  md5_mcr_r4(a,b,c,d, 4, 6,f7537e82);
  md5_mcr_r4(d,a,b,c,11,10,bd3af235);
  md5_mcr_r4(c,d,a,b, 2,15,2ad7d2bb);
  md5_mcr_r4(b,c,d,a, 9,21,eb86d391);

  ma += a, mb += b, mc += c, md += d;
  ma &= 0xffffffffUL;
  mb &= 0xffffffffUL;
  mc &= 0xffffffffUL;
  md &= 0xffffffffUL;
}

}; // namespace pxc

#if 0

#include <stdio.h>

#if 0

static void
test_md5(const char *str)
{
  aioport::md5_context c1;
  c1.update(str);
  fprintf(stderr, "%s: %s\n", str, aioport::hexstring(c1.final()).c_str());

}

  test_md5("");
  test_md5("a");
  test_md5("abc");
  test_md5("message digest");
  test_md5("abcdefghijklmnopqrstuvwxyz");
  test_md5("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
  test_md5(
    "123456789012345678901234567890123456789012345678901234567890123456"
    "78901234567890");

#endif

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s filename\n", argv[0]);
    return -1;
  }
  aioport::md5_context cxt;
  FILE *fp = fopen(argv[1], "r");
  if (fp == 0)
    return -1;
  size_t len = 65536 * 32;
  char buf[len];
  size_t s;
  while ((s = fread(buf, 1, len, fp)) != 0) {
    cxt.update_raw((unsigned char *)buf, s);
  }
  fprintf(stderr, "%s  %s\n", aioport::to_hexadecimal(cxt.final()).c_str(),
          argv[1]);
  fclose(fp);
  return 0;
}

#endif

