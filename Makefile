# Makefile pentru proiectul de detectare a cuvintelor cu NEON

CC = gcc
CFLAGS = -Iinclude -O2 -Wall
NEON_FLAGS = -march=armv8-a+simd
LDLIBS_MATH = -lm

BUILD = build
SRC = src

# detectam arhitectura curenta, ca sa stim daca putem compila cod NEON
ARCH := $(shell uname -m)
IS_ARM := $(filter aarch64 arm64,$(ARCH))

# binare care merg pe orice arhitectura (fara cod NEON)
NONNEON_BINS = $(BUILD)/generate_test_wav \
               $(BUILD)/test_wav_reader \
               $(BUILD)/test_energy_detector \
               $(BUILD)/test_zcr \
               $(BUILD)/test_adaptive

# binare care au nevoie de compilator ARM (contin cod NEON)
NEON_BINS = $(BUILD)/test_compare $(BUILD)/benchmark

# pe ARM compilam tot, pe x86 (WSL) doar partea fara NEON
ifeq ($(IS_ARM),)
ALL_BINS = $(NONNEON_BINS)
else
ALL_BINS = $(NONNEON_BINS) $(NEON_BINS)
endif

.PHONY: all clean run

all: $(BUILD) $(ALL_BINS)

$(BUILD):
	mkdir -p $(BUILD)

# --- binare fara NEON ---

$(BUILD)/generate_test_wav: $(SRC)/generate_test_wav.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS_MATH)

$(BUILD)/test_wav_reader: $(SRC)/test_wav_reader.c $(SRC)/wav_reader.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_energy_detector: $(SRC)/test_energy_detector.c $(SRC)/wav_reader.c $(SRC)/energy_detector.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_zcr: $(SRC)/test_zcr.c $(SRC)/wav_reader.c $(SRC)/energy_detector.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_adaptive: $(SRC)/test_adaptive.c $(SRC)/wav_reader.c $(SRC)/energy_detector.c $(SRC)/adaptive_threshold.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

# --- binare cu NEON, compileaza doar pe ARM ---

$(BUILD)/test_compare: $(SRC)/test_compare_scalar_neon.c $(SRC)/wav_reader.c $(SRC)/energy_detector.c $(SRC)/energy_detector_neon.c | $(BUILD)
	$(CC) $(CFLAGS) $(NEON_FLAGS) -o $@ $^ $(LDLIBS_MATH)

$(BUILD)/benchmark: $(SRC)/benchmark.c $(SRC)/wav_reader.c $(SRC)/energy_detector.c $(SRC)/energy_detector_neon.c | $(BUILD)
	$(CC) $(CFLAGS) $(NEON_FLAGS) -o $@ $^

# ruleaza tot pe rand, cu afisaje intre pasi, ca sa vezi tot fluxul dintr-o data
run: all
	@echo "== Generam semnalul de test sintetic =="
	./$(BUILD)/generate_test_wav
	@echo ""
	@echo "== Citire WAV (verificare header) =="
	./$(BUILD)/test_wav_reader
	@echo ""
	@echo "== Detectie energie, prag fix =="
	./$(BUILD)/test_energy_detector
	@echo ""
	@echo "== Zero-crossing rate =="
	./$(BUILD)/test_zcr
	@echo ""
	@echo "== Prag adaptiv + hangover, pe voce reala =="
	./$(BUILD)/test_adaptive data/test_voice.wav
	@if [ -n "$(IS_ARM)" ]; then \
		echo ""; \
		echo "== Validare corectitudine: scalar vs NEON =="; \
		./$(BUILD)/test_compare; \
		echo ""; \
		echo "== Benchmark viteza: scalar vs NEON =="; \
		./$(BUILD)/benchmark data/test_voice.wav 100; \
	else \
		echo ""; \
		echo "(sarim testele NEON, arhitectura curenta e $(ARCH), nu ARM - ruleaza pe placa pentru asta)"; \
	fi

clean:
	rm -rf $(BUILD)