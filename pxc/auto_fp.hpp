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

#ifndef PXC_AUTO_FP_HPP
#define PXC_AUTO_FP_HPP

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

namespace pxc {

struct auto_fp {
  auto_fp() : fp(0) { }
  explicit auto_fp(FILE *fp) : fp(fp) { }
  ~auto_fp() { if (fp) { fclose(fp); } }
  FILE *get () const { return fp; }
  int close() {
    int r = -1;
    if (fp) {
      r = fclose(fp);
      fp = 0;
    }
    return r;
  }
  auto_fp& operator =(auto_fp& x) {
    close();
    fp = x.fp;
    x.fp = 0;
    return *this;
  }
private:
  FILE *fp;
  auto_fp(const auto_fp&);
};

struct auto_pipe_fp {
  explicit auto_pipe_fp(FILE *fp) : fp(fp) { }
  ~auto_pipe_fp() { close(); }
  FILE *get() const { return fp; }
  int close() {
    int r = -1;
    if (fp) {
      r = pclose(fp);
      fp = 0;
    }
    return r;
  }
private:
  FILE *fp;
  auto_pipe_fp(const auto_pipe_fp&);
  auto_pipe_fp& operator =(const auto_pipe_fp&);
};

struct auto_dir {
  explicit auto_dir(DIR *dir) : dir(dir) { }
  ~auto_dir() { close(); }
  DIR *get() const { return dir; }
  int close() {
    int r = -1;
    if (dir) {
      r = closedir(dir);
      dir = 0;
    }
    return r;
  }
private:
  DIR *dir;
  auto_dir(const auto_dir&);
  auto_dir& operator =(const auto_dir&);
};

};

#endif

