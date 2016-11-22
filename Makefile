### VARIABLES SECTION ###

OPENSSL_VERSION=1.1.0c
OPENSSL_BUILD_DIR=usr/local/openssl_build
OPENSSL_INSTALL_DIR=usr/local/openssl_install
LIBSSH2_VERSION=1.8.0


### TARGETS SECTION ###

# NOTE: Because two builds differ only in defines, make will be confused and will claim the target has already been built - so clean-up is advised

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
	cd libraries/libssh2-$(LIBSSH2_VERSION) && bash configure --enable-shared=no --disable-examples-build --with-libssl-prefix=$(shell pwd)/libraries/openssl-$(OPENSSL_VERSION)/$(OPENSSL_INSTALL_DIR) && make;
endif

openssl:
ifdef SystemRoot
	# TODO: we are on Windows
else
ifeq ($(shell uname -s), Darwin)
	cd libraries/openssl-$(OPENSSL_VERSION) && perl Configure darwin64-x86_64-cc no-unit-test --openssldir=$(shell pwd)/libraries/openssl-$(OPENSSL_VERSION)/$(OPENSSL_BUILD_DIR) --prefix=$(shell pwd)/libraries/openssl-$(OPENSSL_VERSION)/$(OPENSSL_INSTALL_DIR) no-shared && chmod -R 777 usr/ && make depend && make && make install
endif
endif

zlib:
ifdef SystemRoot
	# TODO: we are on Windows
endif

clean:
	rm -rf *.o build/
	rm -rf libraries/openssl-$(OPENSSL_VERSION)/$(OPENSSL_BUILD_DIR)
	rm -rf libraries/openssl-$(OPENSSL_VERSION)/$(OPENSSL_INSTALL_DIR)
	cd libraries/libssh2-$(LIBSSH2_VERSION) && make clean
	cd libraries/openssl-$(OPENSSL_VERSION) && make clean
