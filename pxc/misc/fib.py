#!/usr/bin/env python

# import gc
# gc.disable()

def fib(x):
  if x < 2:
    return x
  else:
    return fib(x - 2) + fib(x - 1)

print(fib(35))

