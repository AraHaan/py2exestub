project('py2exestub', 'c')
add_project_arguments(['-D_CRT_SECURE_NO_WARNINGS', '-D_UNICODE', '-DUNICODE'], language: 'c')

py = import('python').find_installation(pure: false)
windows = import('windows')

# Optional: enable Py_LIMITED_API for the embed exe
limited_api_args = ['-DPy_LIMITED_API=0x030C0000']

# ----------------------------------------------------
# 1. Build _memimport from externals/py2exe sources
# ----------------------------------------------------

memimport_sources = files(
    '../py2exe/source/_memimporter.c',
    '../py2exe/source/actctx.c',
    '../py2exe/source/import_mini.c',
    '../py2exe/source/MemoryModule.c',
    '../py2exe/source/MyLoadLibrary.c'
)

_memimport = py.extension_module(
    '_memimport',
    memimport_sources,
    install: true,
    include_directories: [include_directories('../py2exe/source')],
    c_args: ['-DSTANDALONE'],   # ← required define
    subdir: 'py2exestub/data',
    limited_api: '3.12',
)

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
        embed_res
    ],
    install: true,
    include_directories: [include_directories('src/py2exestub')],
    c_args: limited_api_args,
    dependencies: py.dependency(embed: true),
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
)

# Install the two executables, the C extension, and the contents of the data folder in src into py2exestub/data/
py.install_sources(
    files('src/py2exestub/data/memimport.py', 'src/py2exestub/data/zipextimporter.py'),
    subdir: 'py2exestub/data'
)
