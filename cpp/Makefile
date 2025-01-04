PROFILEFLAGS = -pg -fno-omit-frame-pointer -fno-common
DEBUGFLAGS = -fsanitize=address -static-libasan -g -D_GLIBCXX_DEBUG -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fno-common
OPTIMIZATIONS = -Ofast
CPPVERSION = -std=c++17
# -Wunused-function disabled for serialization lib, and -Wterminate to prevent errors dealing with termination from exceptions
WARNINGS = -Wno-unused-function -Wno-terminate -Wall -Wextra -pedantic -Wfatal-errors
FLAGS = $(OPTIMIZATIONS) $(CPPVERSION) $(WARNINGS)
main: libdc_cpp.a
libdc.a:
	cd ../ && $(MAKE)
Client.o:
	g++ -c src/Client.cpp $(FLAGS) -o bin/Client.o
Server.o:
	g++ -c src/Server.cpp $(FLAGS) -o bin/Server.o
libdc_cpp.a: libdc.a Server.o Client.o
	cp ../target/release/libdc.a libdc_cpp.a
	ar r libdc_cpp.a bin/Server.o bin/Client.o
clean:
	rm bin/*
	rm libdc_cpp.a
	(cd ../ && $(MAKE) clean)
.PHONY: libdc.a libdc_cpp.a main
