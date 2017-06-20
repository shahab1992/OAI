CC      = gcc
CFLAGS  = -msse4.1 -O3 -Wall -g
LDFLAGS = 
LIBS    = 
INCLUDE = -I ../api/inc
SRC_DIR = ./src
OBJ_DIR = ./build
SOURCES = $(shell ls $(SRC_DIR)/*.c) 
OBJS    = $(subst $(SRC_DIR),$(OBJ_DIR), $(SOURCES:.c=.o))
TARGET  = sample
DEPENDS = $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
#	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@if [ ! -d $(OBJ_DIR) ]; \
		then echo "mkdir -p $(OBJ_DIR)"; mkdir -p $(OBJ_DIR); fi
		$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $< 

clean:
	$(RM) $(OBJS) $(TARGET) $(DEPENDS)

-include $(DEPENDS)

.PHONY: all clean


#gcc -I ../api/inc/ sample.c ../api/build/fjt_turbo_api.o
# sample #############################################################################
#
#CC      = g++
#CFLAGS  = -g -MMD -MP
#LDFLAGS = 
#LIBS    = 
#INCLUDE = -I ./include
#SRC_DIR = ./src
#OBJ_DIR = ./build
#SOURCES = $(shell ls $(SRC_DIR)/*.cpp) 
#OBJS    = $(subst $(SRC_DIR),$(OBJ_DIR), $(SOURCES:.cpp=.o))
#TARGET  = target_name
#DEPENDS = $(OBJS:.o=.d)
#
#all: $(TARGET)
#
#$(TARGET): $(OBJS) $(LIBS)
#  $(CC) -o $@ $(OBJS) $(LDFLAGS)
#
#$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp 
#   @if [ ! -d $(OBJ_DIR) ]; \
#      then echo "mkdir -p $(OBJ_DIR)"; mkdir -p $(OBJ_DIR); \
#      fi
#  $(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $< 
#
#clean:
#  $(RM) $(OBJS) $(TARGET) $(DEPENDS)
#
#-include $(DEPENDS)
#
#.PHONY: all clean
#
