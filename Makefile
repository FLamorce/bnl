PRG = bnl

SRC = $(PRG).c
HDR = 
OBJ = $(SRC:.c=.o)

CC = gcc
CCFLAGS = -O2 -D_FILE_OFFSET_BITS=64
LDFLAGS = -lz -flto

all : $(PRG)

%.o : %.c $(HDR)
	$(CC) $(CCFLAGS) -c $< -o $@

$(PRG) : $(OBJ)
	$(CC) $(CCFLAGS) $(OBJ) -o $@ $(LDFLAGS)
	strip -p -s $@

clean:
	rm -f $(OBJ)
	rm -f $(PRG)
