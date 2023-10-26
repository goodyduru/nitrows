# The sources we're building
HEADERS = $(wildcard *.h)
PRODUCT_SOURCES = $(wildcard *.c)

# What we're building
PRODUCT_OBJECTS = $(PRODUCT_SOURCES:.c=.o)
PRODUCT = nitrows

# What we're building with
CXX = clang
CFLAGS := -Wall -std=c99
CFLAGS_DEBUG := -g -DDEBUG -O0
CFLAGS_ASAN := -O1 -g -fsanitize=address
CFLAGS_RELEASE := -O3
LDFLAGS = -lz

ifeq ($(DEBUG),1)
  CFLAGS := $(CFLAGS_DEBUG) $(CFLAGS)
else ifeq ($(ASAN),1)
  CFLAGS := $(CFLAGS_ASAN) $(CFLAGS)
  LDFLAGS := $(LDFLAGS) -fsanitize=address
else
  CFLAGS := $(CFLAGS_RELEASE) $(CFLAGS)
endif

# By default, make the product.
all:		$(PRODUCT)

# How to clean up
clean:
	$(RM) $(PRODUCT) $(PROFILE_PRODUCT) *.o *.out

# How to compile a C file
%.o:		%.c $(HEADERS)
	$(CXX) $(CFLAGS) -o 

# How to compile a C file
%.o:		%.c $(HEADERS)
	$(CXX) $(CFLAGS) $(EXTRA_CXXFLAGS) -o $@ -c $<

# How to link the product
$(PRODUCT):	$(PRODUCT_OBJECTS)
	$(CXX) $(LDFLAGS) $(EXTRA_LDFLAGS) -o $@ $(PRODUCT_OBJECTS)