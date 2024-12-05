main: libdc.a
libdc.a:
	cargo build --release
clean:
	cargo clean
.PHONY: main libdc.a clean
#	cbindgen --config cbindgen.toml --crate dc --output my_header.h
