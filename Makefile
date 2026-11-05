# Makefile - skip-list-cpp
# Uso: make | make run | make test | make test SANITIZE=1 | make clean

CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
CPPFLAGS := -Iinclude
LDFLAGS  :=

# Com SANITIZE=1 os binarios sao instrumentados com AddressSanitizer e
# UndefinedBehaviorSanitizer (vazamentos, acessos invalidos e UB abortam).
ifeq ($(SANITIZE),1)
    CXXFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g
    LDFLAGS  += -fsanitize=address,undefined
endif

# No Windows (MinGW) o g++ gera .exe; o sufixo evita recompilar a toa.
ifeq ($(OS),Windows_NT)
    EXE := .exe
endif

BUILD   := build
HEADERS := $(wildcard include/*.hpp)
DEMO    := $(BUILD)/demo$(EXE)
TESTES  := $(BUILD)/testes$(EXE)

.PHONY: all run test clean

all: $(DEMO) $(TESTES)

$(DEMO): src/main.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@ $(LDFLAGS)

$(TESTES): tests/test_skip_list.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@ $(LDFLAGS)

$(BUILD):
	mkdir -p $(BUILD)

run: $(DEMO)
	./$(DEMO)

test: $(TESTES)
	./$(TESTES)

clean:
	rm -rf $(BUILD)
