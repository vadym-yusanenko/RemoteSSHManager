### VARIABLES SECTION ###

PYTHON_INTERPRETER=/Library/Frameworks/Python.framework/Versions/2.7/bin/python
PYTHON_SITE_PACKAGES_DIR=/Library/Frameworks/Python.framework/Versions/2.7/site-packages
PYTHON_LIBRARY_DIR=/Library/Frameworks/Python.framework/Versions/2.7/lib
PYTHON_INCLUDE_DIR=/Library/Frameworks/Python.framework/Versions/2.7/include
OPENSSL_VERSION=1.0.2g
OPENSSL_BUILD_DIR=usr/local/openssl_build
LIBSSH2_VERSION=1.7.0
CC=gcc
DEPS=functions.h definitions.h
CFLAGS=-DDEBUG -I/c/python27/include -I$(PYTHON_INCLUDE_DIR) -Ilibraries/libssh2-$(LIBSSH2_VERSION)/include
ifdef SystemRoot
	LIBS+=-lpthreadgc2 -lpython27 -lws2_32
	LDFLAGS=-L/usr/local/lib -L/c/python27/libs $(LIBS)
else
	ifeq ($(shell uname -s), Darwin)
		LIBS=-lssh2 -lssl -lcrypto -lz -lpython2.7 -lpthread
		LDFLAGS=-L./libraries/libssh2-$(LIBSSH2_VERSION)/src/.libs/ -L$(PYTHON_LIBRARY_DIR) -L./libraries/openssl-$(OPENSSL_VERSION)/$(OPENSSL_BUILD_DIR)/lib/ $(LIBS)
	endif
endif


### TARGETS SECTION ###

# NOTE: Because two builds differ only in defines, make will be confused and will claim the target has already been built - so clean-up is advised

# NOTE: $@ - target name, $^ - all stated dependencies
test: functions.o main.o
	$(CC) -c main.c $(CFLAGS)
	$(CC) -c functions.c $(CFLAGS)
	$(CC) -o $@ functions.o main.o $(LDFLAGS)
test: openssl zlib libssh2

# NOTE: test_non_threaded differs from test only with one make variable - to alter it before actual build - we introduce "test" target and append variable before it
test_non_threaded: CFLAGS+=-DDEBUG_NON_THREADED
test_non_threaded: test

# NOTE: %. - target's abstract name by pattern, $< - first item defined as dependency - we have "%.c" and list of remaining "$(DEPS)"
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

extension: openssl zlib libssh2
extension:
	rm -rf build;
ifdef SystemRoot
	python setup.py build --compiler=mingw32
else
ifeq ($(shell uname -s), Darwin)
	$(PYTHON_INTERPRETER) setup.py build
endif
endif

libssh2:
ifdef SystemRoot
	# TODO: we are on Windows
else
	cd libraries/libssh2-$(LIBSSH2_VERSION) && bash configure --enable-shared=no --disable-examples-build --with-libssl-prefix=$(shell pwd)/libraries/openssl-$(OPENSSL_VERSION)/$(OPENSSL_BUILD_DIR) && make;
endif

openssl:
ifdef SystemRoot
	# TODO: we are on Windows
else
ifeq ($(shell uname -s), Darwin)
	cd libraries/openssl-$(OPENSSL_VERSION) && perl Configure darwin64-x86_64-cc no-unit-test --openssldir=$(shell pwd)/libraries/openssl-$(OPENSSL_VERSION)/$(OPENSSL_BUILD_DIR) no-shared && chmod -R 777 util && make depend && make && make install
endif
endif

zlib:
ifdef SystemRoot
	# TODO: we are on Windows
endif

clean:
ifdef SystemRoot
	# TODO: we are on Windows
else
	rm -rf *.o
	rm -rf libraries/openssl-$(OPENSSL_VERSION)/$(OPENSSL_BUILD_DIR)
	cd libraries/libssh2-$(LIBSSH2_VERSION) && make clean
	cd libraries/openssl-$(OPENSSL_VERSION) && make clean
endif
	rm -rf *.o *.exe build test
