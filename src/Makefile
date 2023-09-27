# The sources we're building
HEADERS = $(wildcard *.h)
PRODUCT_SOURCES = $(wildcard *.c)

OPENSSL_IDIR = /usr/local/opt/openssl@3/include
OPENSSL_IFLAG = -I$(OPENSSL_IDIR)

# What we're building with
CXX = clang
CXXFLAGS = -std=gnu99 -Wall -g -O0 -gdwarf-3
LIBS=-L/usr/local/Cellar/openssl@3/3.0.1/lib -lssl -lcrypto

# By default, make the product.
all:		$(PRODUCT)

# How to compile a C file
%.o:		%.c $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(LIBS) $(OPENSSL_IFLAG)