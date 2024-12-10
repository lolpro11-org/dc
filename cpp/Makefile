PROFILEFLAGS = -pg -fno-omit-frame-pointer -fno-common
DEBUGFLAGS = -fsanitize=address -static-libasan -g -D_GLIBCXX_DEBUG -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fno-common
OPTIMIZATIONS = -Ofast
CPPVERSION = -std=c++17
WARNINGS = -Wno-unused-function -Wall -Wextra -pedantic -Wfatal-errors
FLAGS = $(OPTIMIZATIONS) $(CPPVERSION) $(WARNINGS)
main: libdc_cpp.a
libdc.a:
	cd ../ && $(MAKE)
Client.o:
	g++ -c src/Client.cpp $(FLAGS) -o bin/Client.o
Server.o:
	g++ -c src/Server.cpp $(FLAGS) -o bin/Server.o
RustString.o:
	g++ -c src/RustString.cpp $(FLAGS) -o bin/RustString.o
libdc_cpp.a: libdc.a RustString.o Server.o Client.o
	cp ../target/release/libdc.a libdc_cpp.a
	ar r libdc_cpp.a bin/RustString.o bin/Server.o bin/Client.o
clean:
	rm bin/*
	rm libdc_cpp.a
	(cd ../ && $(MAKE) clean)
.PHONY: libdc.a libdc_cpp.a main
