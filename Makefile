######## PROJECT FILES ########
SRC_DIR ::= ./src
OBJ_DIR ::= ./obj
SRC ::= crypt/sha1.c main.c

VPATH ::= $(SRC_DIR)
OBJ ::=  $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(SRC)))
TARGET ::= a.out
######## BUILD OPTIONS ########
CC ?= gcc
CFLAGS ::= -Wall -Wextra -Wpedantic -std=c99\
	-Wno-unused-variable -Wno-unused-parameter\
	-I$(SRC_DIR)
########     RULES     ########
.PHONY: all debug test clean

### ALL ###
all: CFLAGS += -O2 -march=native -Werror
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

define OBJ_TEMPLATE =
$$(OBJ_DIR)/$(patsubst %.c,%.o, $(notdir $(1))): $(1) Makefile | $$(OBJ_DIR)
	$$(CC) $$(CFLAGS) -c -o $$@ $$<
endef

$(foreach src, $(SRC), $(eval $(call OBJ_TEMPLATE, $(src))))

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

### DEBUG ###
debug: CFLAGS += -Og -g -DT_DEBUG
debug: clean $(TARGET)

### TEST ###
test: CFLAGS += -O3 -march=native
test: sha1-test
sha1-test:
	$(CC) $(CFLAGS) ./src/crypt/sha1.c \
	./src/crypt/sha1_test.c   -o ./src/crypt/test
	cd ./src/crypt && ./sha1_test.sh

clean: 
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET)
