.PHONY: all format build clean test

all: build

build:
	cmake --build build -j

format:
	cmake --build build --target format

clean:
	cmake --build build --target clean

test:
	ctest --test-dir build --output-on-failure
