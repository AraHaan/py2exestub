project('py2exestub', 'c', meson_version: '>=1.10.0', version: '0.1.0')

py = import('python').find_installation(pure: true)

# ----------------------------------------------------
# 1. build the wheel
# ----------------------------------------------------

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
