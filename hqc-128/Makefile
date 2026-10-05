ifeq ($(ROOT),)
ROOT:=.
endif

CC:=gcc

CFLAGS:=-O3 -std=c99 -funroll-all-loops -flto -mavx -mavx2 -mbmi -mpclmul -pedantic -Wall -Wextra

SHA3_SRC:=$(ROOT)/lib/fips202/fips202.c
SHA3_INCLUDE:=-I $(ROOT)/lib/fips202

SRC:=$(ROOT)/src/
INCLUDE:=-I $(ROOT)/src
LIB:=$(SHA3_INCLUDE)

MAIN_HQC:=$(ROOT)/src/main_hqc.c
MAIN_KAT:=$(ROOT)/src/main_kat.c

HQC_OBJS:=vector.o reed_muller.o reed_solomon.o fft.o gf.o gf2x.o code.o parsing.o hqc.o kem.o shake_ds.o shake_prng.o
HQC_OBJS_VERBOSE:=vector-verbose.o reed_muller.o reed_solomon-verbose.o fft.o gf.o gf2x.o code-verbose.o parsing.o hqc-verbose.o kem-verbose.o shake_ds.o shake_prng.o
LIB_OBJS:= fips202.o

DEST_LIB:=libhqc-128.so
LDFLAGS:=-shared

BIN:=bin
BUILD:=bin/build

folders:
	@/bin/echo -e "\n### Creating folders"
	mkdir -p $(BUILD)

fips202.o: | folders
	@/bin/echo -e "\n### Compiling fips202"
	$(CC) $(CFLAGS) -c $(SHA3_SRC) $(SHA3_INCLUDE) -o $(BUILD)/$@

%.o: $(SRC)/%.c | folders
	@/bin/echo -e "\n### Compiling $@"
	$(CC) $(CFLAGS) -c $< $(INCLUDE) $(LIB) -o $(BUILD)/$@

%-verbose.o: $(SRC)/%.c | folders
	@/bin/echo -e "\n### Compiling $@ (verbose mode)"
	$(CC) $(CFLAGS) -c $< $(INCLUDE) $(LIB) -D VERBOSE -o $(BUILD)/$@

hqc-128: $(HQC_OBJS) $(LIB_OBJS) | folders
	@/bin/echo -e "\n### Compiling hqc-128"
	$(CC) $(CFLAGS) $(MAIN_HQC) $(addprefix $(BUILD)/, $^) $(INCLUDE) $(LIB) -o $(BIN)/$@

hqc-128-kat: $(HQC_OBJS) $(LIB_OBJS) | folders
	@/bin/echo -e "\n### Compiling hqc-128 KAT"
	$(CC) $(CFLAGS) $(MAIN_KAT) $(addprefix $(BUILD)/, $^) $(INCLUDE) $(LIB) -o $(BIN)/$@

hqc-128-verbose: $(HQC_OBJS_VERBOSE) $(LIB_OBJS) | folders
	@/bin/echo -e "\n### Compiling hqc-128 (verbose mode)"
	$(CC) $(CFLAGS) $(MAIN_HQC) $(addprefix $(BUILD)/, $^) $(INCLUDE) $(LIB) -D VERBOSE -o $(BIN)/$@

create-lib: $(DEST_LIB)

$(DEST_LIB): $(HQC_OBJS) $(LIB_OBJS) | folders
	@/bin/echo -e "\n### Creating library"
	$(CC) $(LDFLAGS) $(addprefix $(BUILD)/, $^) $(INCLUDE) $(LIB) -o $(BUILD)/$@

clean:
	rm -f PQCkemKAT_*
	rm -f vgcore.*
	rm -rf ./bin



# TEST 
# Add new test:
# 1. Add the object file name after TEST_OBJS.
# 2. Add rules for the test. (1) Change the target. (2) Change the last object file name in prerequisites. (3) Change echo information in the recipe.

TEST:=test
TEST_OBJS:=test_utils.o fix_weight_gen_test.o vect_set_random_test.o vect_mul_test.o keygen_test.o code_encode_test.o reed_solomon_encode_test.o  \
			gf_mul_vec_test.o reed_muller_encode_test.o hqc_pke_encrypt_test.o hqc_pke_decrypt_test.o crypto_kem_enc_test.o crypto_kem_dec_test.o 

test_folders:
	@/bin/echo -e "\n### Creating test folders"
	mkdir -p $(BIN)/test

%.o: $(TEST)/%.c | folders test_folders
	@/bin/echo -e "\n### Compiling $@"
	$(CC) $(CFLAGS) -c $< $(INCLUDE) $(LIB) -o $(BIN)/test/$@

fix_weight_gen_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o fix_weight_gen_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling fix weight gen test test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

vect_set_random_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o vect_set_random_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling vec set random test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

vect_mul_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o vect_mul_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling vec mul test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

keygen_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o keygen_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling keygen test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

code_encode_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o code_encode_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling code encode test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

reed_solomon_encode_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o reed_solomon_encode_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling reed solomon encode test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

gf_mul_vec_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o gf_mul_vec_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling gf_mul_vec test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

reed_muller_encode_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o reed_muller_encode_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling reed muller encode test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

hqc_pke_encrypt_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o hqc_pke_encrypt_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling HQC PKE encrypt test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

hqc_pke_decrypt_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o hqc_pke_decrypt_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling HQC PKE decrypt test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

crypto_kem_enc_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o crypto_kem_enc_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling crypto KEM enc test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@

crypto_kem_dec_test: $(HQC_OBJS) $(LIB_OBJS) test_utils.o crypto_kem_dec_test.o | folders test_folders
	@/bin/echo -e "\n### Compiling crypto KEM dec test"
	$(CC) $(CFLAGS) $(addprefix $(BUILD)/, $(HQC_OBJS)) $(addprefix $(BUILD)/, $(LIB_OBJS)) $(BIN)/test/$@.o $(BIN)/test/test_utils.o -o $(BIN)/test/$@
