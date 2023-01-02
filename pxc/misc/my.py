#!/usr/bin/env python
import MySQLdb
dh = MySQLdb.connect('localhost', 'root', '', 'hoge')
cur = dh.cursor()
print(cur)
print(cur.execute('show tables'))

