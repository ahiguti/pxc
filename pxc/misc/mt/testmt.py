#!/usr/bin/env python
import mt19937

m = mt19937.mt19937(123)
for i in range(0, 1024):
	print m.extract_number()
