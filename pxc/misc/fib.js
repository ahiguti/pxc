#!/usr/local/bin/node

function fib(x) {
  return x <= 1 ? x : fib(x - 2) + fib(x - 1);
}

var x = process.argv[2] || 10;
var n = process.argv[3] || 10;
for (var i = 0; i < n; ++i) {
  console.log(fib(x));
}

