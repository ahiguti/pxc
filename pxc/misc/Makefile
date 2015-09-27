
all: decimal

clean:
	cd misctest && $(MAKE) clean
	cd py && $(MAKE) clean
	cd hmtest && $(MAKE) clean
	cd tmtiming && $(MAKE) clean
	rm -f decimal
	-find . -name "pxc_clean.sh" -exec {} ";"

decimal:
	g++ -O9 decimal.cpp -o decimal
