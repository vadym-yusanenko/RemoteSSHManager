from distutils.core import setup, Extension
from glob import glob
from platform import platform

PYTHON_LIBRARY_PATH = (
    #'/Library/Frameworks/Python.framework/Versions/2.7/lib'
    '/usr/lib/'
    if platform().startswith('Darwin')
    else ''  # TODO: Linux, or ld: warning: directory not found for option '-L-lpython2.7' !!!
)


extension_mod = Extension(
    "remote_ssh_manager",
    sources = [
        "wrapper.c"
    ],
    include_dirs = [
        # Windows
        'c:\\msys32\\usr\\local\\include\\',
        # UNIX
        'libraries/libssh2-1.8.0/include'
    ],
    library_dirs = (
        # Windows
        [
            'c:\\Python27\\libs',
            'c:\\msys32\\usr\\local\\lib',
            'c:\\msys32\\mingw32\\i686-w64-mingw32\\lib'
        ]
        if platform().startswith('Windows')
        else
        # UNIX
        [
            'libraries/libssh2-1.8.0/src/.libs/',
            'libraries/openssl-1.1.0c/usr/local/openssl_install/lib',
            PYTHON_LIBRARY_PATH
        ]
    ),
    libraries = (
        # Windows
        ['python27', 'ssh2', 'ssl', 'crypto', 'z', 'gdi32', 'ws2_32']
        if platform().startswith('Windows')
        else
        # UNIX
        ['python2.7', 'ssh2', 'ssl', 'crypto', 'z']
    ),
    extra_compile_args = ['-O3', '-flto'] + (['-mwin32'] if platform().startswith('Windows') else []),
    extra_link_args = ['-O3', '-flto']
)


setup(
    name = "Remote SSH manager",
    version = '1.0',
    description = 'Simple and fast SSH manager written in C',
    author = 'Vadym Yusanenko',
    author_email = 'usanenko.vadim@gmail.com',
    url = 'https://github.com/yusanenko-vadim/RemoteSSHManager',
    ext_modules=[extension_mod]
)
