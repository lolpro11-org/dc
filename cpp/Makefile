DEBUGFLAGS = -fsanitize=address -static-libasan -g -D_GLIBCXX_DEBUG -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fno-common
OPTIMIZATIONS = -Ofast
CPPVERSION = -std=c++17
WARNINGS = -Wno-unused-function -Wall -Wextra -pedantic -Wfatal-errors
FLAGS = $(OPTIMIZATIONS) $(CPPVERSION) $(WARNINGS)
main: libdc_cpp.a
libdc.a:
	cd ../ && $(MAKE)
dc.o:
	g++ -c dc.cpp $(FLAGS) -o dc.o
libdc_cpp.a: dc.o libdc.a
	cp ../target/release/libdc.a libdc_cpp.a
	ar r libdc_cpp.a dc.o
clean:
	rm dc.o
	rm libdc_cpp.a
	(cd ../ && $(MAKE) clean)
.PHONY: libdc.a libdc_cpp.a main
