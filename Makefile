CMAKE ?= cmake
CTEST ?= ctest
DOCKER ?= docker

BUILD_DIR ?= build
BUILD_TYPE ?= Release
PARALLEL ?=
IMAGE ?= cmake-xray
COVERAGE_THRESHOLD ?= 100
CLANG_TIDY_MAX_FINDINGS ?= 0
LIZARD_MAX_CCN ?= 10
LIZARD_MAX_LENGTH ?= 50
LIZARD_MAX_PARAMETERS ?= 5

PARALLEL_FLAG := $(if $(PARALLEL),--parallel $(PARALLEL),--parallel)
BINARY := $(BUILD_DIR)/cmake-xray

.DEFAULT_GOAL := help

.PHONY: help dev configure build test smoke lint docker-test docker-gates coverage-gate quality-gate coverage-report quality-report runtime docs-portability release-dry-run clean

help:
	@printf '%s\n' \
		'Targets:' \
		'  make dev                Configure and build the local Release binary' \
		'  make test               Build locally and run ctest' \
		'  make smoke              Run local --help and --version smoke checks' \
		'  make docker-gates       Run Docker test, coverage and quality gates' \
		'  make runtime            Build and smoke-test the runtime image' \
		'  make coverage-report    Build and print the Docker coverage report' \
		'  make quality-report     Build and print the Docker quality report' \
		'' \
		'Variables:' \
		'  BUILD_DIR=build BUILD_TYPE=Release PARALLEL= IMAGE=cmake-xray COVERAGE_THRESHOLD=100' \
		'  CLANG_TIDY_MAX_FINDINGS=0 LIZARD_MAX_CCN=10 LIZARD_MAX_LENGTH=50 LIZARD_MAX_PARAMETERS=5'

dev: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(BUILD_TYPE) $(PARALLEL_FLAG)

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure -C $(BUILD_TYPE) $(PARALLEL_FLAG)

smoke: build
	./$(BINARY) --help
	./$(BINARY) --version

lint: quality-gate

docker-test:
	$(DOCKER) build --target test -t $(IMAGE):test .

docker-gates: docker-test coverage-gate quality-gate

coverage-gate:
	$(DOCKER) build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=$(COVERAGE_THRESHOLD) -t $(IMAGE):coverage-check .

quality-gate:
	$(DOCKER) build --target quality-check \
		--build-arg XRAY_CLANG_TIDY_MAX_FINDINGS=$(CLANG_TIDY_MAX_FINDINGS) \
		--build-arg XRAY_LIZARD_MAX_CCN=$(LIZARD_MAX_CCN) \
		--build-arg XRAY_LIZARD_MAX_LENGTH=$(LIZARD_MAX_LENGTH) \
		--build-arg XRAY_LIZARD_MAX_PARAMETERS=$(LIZARD_MAX_PARAMETERS) \
		-t $(IMAGE):quality-check .

coverage-report:
	$(DOCKER) build --target coverage -t $(IMAGE):coverage .
	$(DOCKER) run --rm $(IMAGE):coverage

quality-report:
	$(DOCKER) build --target quality -t $(IMAGE):quality .
	$(DOCKER) run --rm $(IMAGE):quality

runtime:
	$(DOCKER) build --target runtime -t $(IMAGE) .
	$(DOCKER) run --rm $(IMAGE) --help

docs-portability:
	bash scripts/verify-doc-examples-portability.sh

release-dry-run:
	bash tests/release/test_release_dry_run.sh

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean
