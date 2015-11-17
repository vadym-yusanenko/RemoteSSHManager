from distutils.core import setup, Extension
from glob import glob
from platform import platform


extension_mod = Extension(
    "threaded_remote_manager",
    sources = [
        "wrapper.c",
        "functions.c"
    ],
    include_dirs = [
        # Windows
        'c:\\msys32\\usr\\local\\include\\',
        # UNIX
        'libraries/libssh2-1.6.0/include'
    ],
    library_dirs = (
        # Windows
        [
            'c:\\Python27\\libs',
            'c:\\msys32\\usr\\local\\lib',
            'c:\\msys32\\mingw32\\i686-w64-mingw32\\lib'
        ]
        if platform() == 'Windows'
        else
        # UNIX
        [
            'libraries/libssh2-1.6.0/src/.libs/'
        ]
    ),
    libraries = (
        # Windows
        ['python27', 'ssh2', 'ssl', 'crypto', 'z', 'pthread', 'gdi32', 'ws2_32']
        if platform() == 'Windows'
        else
        # UNIX
        ['python2.7', 'ssh2', 'ssl', 'crypto', 'z', 'pthread']
    ),
    extra_compile_args = ['-O3', '-flto'] + (['-mwin32'] if platform() == 'Windows' else []),
    extra_link_args = ['-O3', '-flto']
)


setup(
    name = "Threaded remote manager",
    version = '0.9',
    description = 'UIMT threaded remote manager implemented in ANSI C',
    author = 'Vadim Yusanenko',
    author_email = 'usanenko.vadim@gmail.com',
    url = 'http://code.google.com/p/unix-infrastructure-management/',
    ext_modules=[extension_mod]
)
