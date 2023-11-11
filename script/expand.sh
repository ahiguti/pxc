#!/bin/bash

if [ "$1" = "" ]; then
  exit 1
fi

echo "executing expand $1"
expand "$1" | sed 's/[ \t]*$//' > "$1.tmp" && mv "$1.tmp" "$1"

