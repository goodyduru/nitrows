# The sources we're building
HEADERS = $(wildcard *.h)
PRODUCT_SOURCES = $(wildcard *.c)

# What we're building
PRODUCT_OBJECTS = $(PRODUCT_SOURCES:.c=.o)
PRODUCT = nitrows

# What we're building with
CXX = clang
CXXFLAGS = -std=gnu99 -Wall -g -O0 -gdwarf-3
LDFLAGS = -lz
# By default, make the product.
all:		$(PRODUCT)

# How to clean up
clean:
	$(RM) $(PRODUCT) $(PROFILE_PRODUCT) *.o *.out

# How to compile a C file
%.o:		%.c $(HEADERS)
	$(CXX) $(CXXFLAGS) -o 

# How to compile a C file
%.o:		%.c $(HEADERS)
	$(CXX) $(CXXFLAGS) $(EXTRA_CXXFLAGS) -o $@ -c $<

# How to link the product
$(PRODUCT):	$(PRODUCT_OBJECTS)
	$(CXX) $(LDFLAGS) $(EXTRA_LDFLAGS) -o $@ $(PRODUCT_OBJECTS)