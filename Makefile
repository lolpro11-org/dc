main: ffi.a
#	g++ rust.cpp target/release/libffi.a -o main
ffi.a:
	cargo build --release
	cbindgen --config cbindgen.toml --crate dc --output my_header.h
