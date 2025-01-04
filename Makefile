main:
	cargo build --release
clean:
	cargo clean
.PHONY: main clean
#	cbindgen --config cbindgen.toml --crate dc --output my_header.h
