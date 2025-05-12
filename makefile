CC = emcc
CFLAGS = -Os -s WASM=1 \
         -s EXPORTED_FUNCTIONS="['_sv32_translate','_sv39_translate','_demo_setup','_demo_setup_sv39','_init_physical_memory','_init_memory_system', '_get_log_buffer','_clear_log_buffer']" \
         -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap','UTF8ToString','AsciiToString','lengthBytesUTF8']" \
         -s ASSERTIONS=1 \
         -s SAFE_HEAP=1 \
         -s INITIAL_MEMORY=64MB

all: build serve

build:
	mkdir -p public
	$(CC) $(CFLAGS) src/mem_sim.c -o public/sim.js

serve:
	cd public && python3 -m http.server 8000

clean:
	rm -rf public/*
