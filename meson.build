project('py2exestub', 'c', version: '0.1.0')
add_project_arguments(['-D_CRT_SECURE_NO_WARNINGS', '-D_UNICODE', '-DUNICODE'], language: 'c')

py = import('python').find_installation(pure: false)
windows = import('windows')
fs = import('fs')

# Optional: enable Py_LIMITED_API for the embed exe
limited_api_args = ['-DPy_LIMITED_API=0x03090000']

# ----------------------------------------------------
# 1. Build _resourceediting
# ----------------------------------------------------

# for some reason the wheel file also contains the *.lib file that
# is produced from building this C extension.
_resourceediting = py.extension_module(
    '_resourceediting',
    'src/py2exestub/_resourceediting.c',
    install: true,
    include_directories: [include_directories('src/py2exestub')],
    # c_args: limited_api_args,
    subdir: 'py2exestub',
    limited_api: '3.9',
)

# ----------------------------------------------------
# 2. Build embed (uses Python)
# ----------------------------------------------------

embed_res = windows.compile_resources(
    'src/py2exestub/embed.rc',
    include_directories: [include_directories('src/py2exestub')],
)

embed = executable(
    'embed',
    [
        'src/py2exestub/embed.c',
        embed_res
    ],
    install: true,
    include_directories: [include_directories(fs.parent(py.full_path()) / 'Include'), include_directories('src/py2exestub')],
    c_args: limited_api_args,
    link_args: ['/LIBPATH:' + fs.parent(py.full_path()) / 'libs'],
    install_dir: py.get_install_dir() / 'py2exestub',
)

# ----------------------------------------------------
# 3. Build stub (no Python)
# ----------------------------------------------------

stub_res = windows.compile_resources(
    'src/py2exestub/stub.rc',
    include_directories: [include_directories('src/py2exestub')],
)

stub = executable(
    'stub',
    [
        'src/py2exestub/stub.c',
        'externals/miniz-3.1.0/miniz.c',
        stub_res
    ],
    install: true,
    include_directories: [include_directories('externals/miniz-3.1.0'), include_directories('src/py2exestub')],
    install_dir: py.get_install_dir() / 'py2exestub',
    win_subsystem: 'windows',
)

# the main package's python code.
py.install_sources(
    files(
        'src/py2exestub/__init__.py',
        'src/py2exestub/__main__.py',
        'src/py2exestub/DistBuilder.py',

        # code that gets copied into the target folder (is data).
        'src/py2exestub/memimport.py',
        'src/py2exestub/zipextimporter.py'
    ),
    subdir: 'py2exestub'
)
