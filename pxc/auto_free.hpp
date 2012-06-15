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

#ifndef PXC_AUTO_FREE_HPP
#define PXC_AUTO_FREE_HPP

#include <stdio.h>
#include <stdlib.h>

struct auto_free {
  explicit auto_free(char *p) : ptr(p) {
    if (ptr == 0) {
      abort();
    }
  }
  ~auto_free() { free(ptr); }
  char *get() const { return ptr; }
private:
  char *ptr;
  auto_free(const auto_free&);
  auto_free& operator =(const auto_free&);
};

#endif

