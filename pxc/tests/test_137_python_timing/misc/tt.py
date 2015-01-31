#!/usr/bin/env python

def fib(x):
  if x < 2:
    return x
  else:
    return fib(x - 2) + fib(x - 1)

def test1(v, num):
  sum = 0
  for i in range(num):
    sum += fib(v)
  return sum


test1(30, 100)
