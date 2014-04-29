
all: decimal

clean:
	cd misctest && $(MAKE) clean
	cd py && $(MAKE) clean
	cd hmtest && $(MAKE) clean
	cd tmtiming && $(MAKE) clean
	rm -f decimal

decimal:
	g++ -O9 decimal.cpp -o decimal
