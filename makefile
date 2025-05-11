CC = emcc
CFLAGS = -Os -s WASM=1 \
         -s EXPORTED_FUNCTIONS="['_sv32_translate','_demo_setup']"\
         -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
         -s ASSERTIONS=1 \
         -s SAFE_HEAP=1

all: build serve

build:
	mkdir -p public
	$(CC) $(CFLAGS) src/mem_sim.c -o public/sim.js

serve:
	cd public && python3 -m http.server 8000

clean:
	rm -rf public/*
