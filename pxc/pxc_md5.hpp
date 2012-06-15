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

#ifndef PXC_MD5_HPP
#define PXC_MD5_HPP

#include <string>

namespace pxc {

class md5_context {

 public:

  md5_context();
  void update(const std::string& str);
  void update_raw(const unsigned char *ptr, size_t plen);
  std::string final();

 private:

  unsigned long ma, mb, mc, md;
  unsigned long mlength, mlength_high;
  unsigned char buffer[64];
  void initialize();
  void update_block(const unsigned long x[16]);
  void update_block_ch(const unsigned char x[64]);
  void write_result(unsigned char ptr[16]);
};

};

#endif

