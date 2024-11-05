run: compileStandalones
	cargo run --release --bin test
compileStandalones:
	cd "../standalones" && make
cleanOut:
	rm *.txt
clean:
	cd "../standalones" && make clean
main: ffi.a
#	g++ rust.cpp target/release/libffi.a -o main
ffi.a:
	cargo build --release
	cbindgen --config cbindgen.toml --crate ffi --output my_header.h
