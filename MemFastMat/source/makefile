TOP_DIR		 =	.

RM			 =	rm -f
ECHO		 =	echo


CC			 =	gcc
INC			 =
INC			+=	-I$(TOP_DIR)

OBJ_DIR		 = objs

INC_LIST	 =
INC_LIST	+= common.h
INC_LIST	+= osadap.h
INC_LIST	+= test.h

CFLAGS		 =
CFLAGS		+=	-D_CYG_WIN_
CFLAGS		+=	-DLITTLE_ENDIAN=0
CFLAGS		+=	-DBIG_ENDIAN=1
CFLAGS		+=	-DENDIAN_TYPE=LITTLE_ENDIAN
CFLAGS		+=	-O2

TGT			 =	memTest

SRCS		 =
SRCS		+= main.c
SRCS		+= osamem.c
SRCS		+= simulLib.c
SRCS		+= dbgprint.c
SRCS		+= hexdump.c

OBJS		 =	$(addprefix $(OBJ_DIR)/,$(SRCS:.c=.o))

all : $(TGT)

$(TGT) : $(OBJS)
	$(CC) -o $@ $(OBJS)

$(OBJ_DIR)/main.o     : main.c     $(INC_LIST)
$(OBJ_DIR)/osamem.o   : osamem.c   $(INC_LIST)
$(OBJ_DIR)/simulLib.o : simulLib.c $(INC_LIST)
$(OBJ_DIR)/dbgprint.o : dbgprint.c $(INC_LIST)
$(OBJ_DIR)/hexdump.o  : hexdump.c  $(INC_LIST)

$(OBJ_DIR)/%.o : %.c
	-@test -d $(OBJ_DIR) || mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) $(INC) -o $@ $<

clean:
	@$(RM) $(OBJ_DIR)/*.o
	@$(ECHO) "Image Clean Done!"

clobber: clean
	@$(RM) -r $(OBJ_DIR) *.exe *.exe.stackdump
	@$(ECHO) "Image Clobber Done!"
