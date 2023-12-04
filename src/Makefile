# The sources we're building
HEADERS = $(wildcard *.h)
PRODUCT_SOURCES = $(wildcard *.c)

# What we're building
PRODUCT_OBJECTS = $(PRODUCT_SOURCES:.c=.o)
PRODUCT = nitrows

# What we're building with
CXX = clang
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	CFLAGS := -Wall -std=gnu99 -I/usr/local/opt/openssl/include
	LDFLAGS = -L/usr/local/opt/openssl/lib -lz -lcrypto
else
	CFLAGS := -Wall -std=gnu99
	LDFLAGS := -lz -lcrypto
endif
CFLAGS_DEBUG := -g -DDEBUG -O0
CFLAGS_ASAN := -O1 -g -fsanitize=address
CFLAGS_RELEASE :=  -O3 -g
TIDY := clang-tidy
FORMAT := clang-format
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

tidy:
	$(TIDY) $(HEADERS) $(PRODUCT_SOURCES) -- $(CFLAGS)

format: 
	$(FORMAT) -i -style=file $(HEADERS) $(PRODUCT_SOURCES)

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