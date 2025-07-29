OUT ?= ./build

.PHONY: build
build: $(OUT)/build.ninja
	ninja -C $(OUT)

$(OUT)/build.ninja:
	meson setup $(OUT)

.PHONY: check
check: mason/.git/HEAD build
	./mason/test-suite/run-tests.js $(OUT)/mason-to-json

mason/.git/HEAD:
	git clone https://github.com/mortie/mason.git
