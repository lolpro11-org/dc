main: ffi.a
ffi.a:
	cargo build --release
#	cbindgen --config cbindgen.toml --crate dc --output my_header.h
