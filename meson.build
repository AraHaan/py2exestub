project('py2exestub', 'c')
add_project_arguments(['-D_CRT_SECURE_NO_WARNINGS', '-D_UNICODE', '-DUNICODE'], language: 'c')

py = import('python').find_installation(pure: false)
windows = import('windows')

# Optional: enable Py_LIMITED_API for the embed exe
limited_api_args = ['-DPy_LIMITED_API=0x030C0000']

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
    limited_api: '3.12',
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
        '../py2exe/source/_memimporter.c',
        '../py2exe/source/actctx.c',
        '../py2exe/source/import_mini.c',
        '../py2exe/source/MemoryModule.c',
        '../py2exe/source/MyLoadLibrary.c',
        '../audioop/audioop.c',
        embed_res
    ],
    install: true,
    include_directories: [include_directories('src/py2exestub')],
    c_args: ['-DSTANDALONE', '-DPy_BUILD_CORE_MODULE', '-DEMBEDDED_PYTHON', '-DEMBED_MEMIMPORTER'],
    dependencies: py.dependency(embed: true),
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
        '../miniz-3.0.2/miniz.c',
        stub_res
    ],
    install: true,
    include_directories: [include_directories('../miniz-3.0.2'), include_directories('src/py2exestub')],
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
