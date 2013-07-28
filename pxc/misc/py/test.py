import pyhello
h = pyhello.hoge()
sum = 0
for i in range(0, 100):
  for j in range(0, 1000):
    sum += h.m1(5)
print sum

f = pyhello.fuga(3, "")
# f.v = 10
print f.m1(20)
print f.m2(20)
f.v = 2
print f.m3(55)
