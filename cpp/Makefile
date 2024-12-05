DEBUGFLAGS = -fsanitize=address -static-libasan -g -D_GLIBCXX_DEBUG
OPTIMIZATIONS = -Ofast
CPPVERSION = -std=c++17
WARNINGS = -Wno-unused-function -Wall -Wextra -pedantic -Wfatal-errors
FLAGS = $(OPTIMIZATIONS) $(CPPVERSION) $(WARNINGS)
main: libdc_cpp.a
libdc.a:
	cd ../ && make
dc.o:
	g++ -c dc.cpp $(FLAGS) -o dc.o
libdc_cpp.a: dc.o libdc.a
	cp ../target/release/libdc.a libdc_cpp.a
	ar r libdc_cpp.a dc.o
clean:
	rm dc.o
	rm libdc_cpp.a
	(cd ../ && make clean)
.PHONY: libdc.a libdc_cpp.a main
