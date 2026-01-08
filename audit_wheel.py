import os
import zipfile
import hashlib
import base64


def _get_files(arch: str) -> list[tuple[str, str]]:
    files: list[tuple[str, str]] = [
        (os.path.join(os.path.join(os.getcwd(), 'dist'), 'embed-win32.exe'),
         'py2exestub/embed-win32.exe'),
        (os.path.join(os.path.join(os.getcwd(), 'dist'), 'embed-amd64.exe'),
         'py2exestub/embed-amd64.exe'),
        (os.path.join(os.path.join(os.getcwd(), 'dist'), 'embed-arm64.exe'),
         'py2exestub/embed-arm64.exe'),
        (os.path.join(os.path.join(os.getcwd(), 'dist'), 'stub-win32.exe'),
         'py2exestub/stub-win32.exe'),
        (os.path.join(os.path.join(os.getcwd(), 'dist'), 'stub-amd64.exe'),
         'py2exestub/stub-amd64.exe'),
        (os.path.join(os.path.join(os.getcwd(), 'dist'), 'stub-arm64.exe'),
         'py2exestub/stub-arm64.exe'),
        (os.path.join(os.path.join(os.getcwd(), 'dist'), f'_resourceediting-{arch}.pyd'),
         'py2exestub/_resourceediting.pyd')
    ]
    return files


def _write_new_records(file_names: list[tuple[str, str]]) -> list[bytes]:
    """Write new record to wheel."""
    output: list[bytes] = []
    for file_name, dest_file_name in file_names:
        with open(file_name, 'rb') as f:
            data = f.read()

        digest = hashlib.sha256(data).digest()
        output.append(
            f"{dest_file_name},sha256={base64.urlsafe_b64encode(digest).rstrip(b"=").decode("ascii")},{len(data)}\n".encode())
    return output


def update_wheel(src_wheel: str, dest_wheel: str, arch: str):
    files: list[tuple[str, str]] = _get_files(arch)
    with zipfile.ZipFile(src_wheel, 'r') as zin:
        with zipfile.ZipFile(dest_wheel, 'w') as zout:
            for item in zin.infolist():
                if not item.filename.endswith('.dist-info/RECORD') and not item.filename.endswith('.dist-info/WHEEL'):
                    data = zin.read(item.filename)
                    # Copy file to new wheel
                    zout.writestr(item, data)
                else:
                    lines = zin.read(item.filename).splitlines(keepends=True)
                    if item.filename.endswith('.dist-info/RECORD'):
                        lines += _write_new_records(files)
                        zout.writestr(item, b''.join(lines).decode('ascii'))
                    else:
                        new_lines: list[bytes] = []
                        for line in lines:
                            if b'Root-Is-Purelib: true' in line:
                                # replace the true in wline to false.
                                new_lines.append(line.replace(b'true', b'false'))
                            elif b'Tag: py3-none-any' in line:
                                new_lines.append(
                                    line.replace(b'any', f'win_{arch}'.encode() if arch != b'win32' else arch))
                            else:
                                new_lines.append(line)
                        zout.writestr(item, b''.join(new_lines).decode('ascii'))

            for file_name, arc_name in files:
                with open(file_name, 'rb') as f:
                    zout.writestr(arc_name, f.read())

    print(f'  {src_wheel} -> {dest_wheel}')


def main():
    package_version = os.getenv(
        'package_version',
        '0.1.0' # defaulting if not present.
    )
    update_wheel(
        os.path.join(
            os.path.join(os.getcwd(), 'dist'),
            f'py2exestub-{package_version}-py3-none-any.whl'),
        os.path.join(
            os.path.join(os.getcwd(), 'dist'),
            f'py2exestub-{package_version}-py3-none-win32.whl'),
        'win32')
    update_wheel(
        os.path.join(
            os.path.join(os.getcwd(), 'dist'),
            f'py2exestub-{package_version}-py3-none-any.whl'),
        os.path.join(
            os.path.join(os.getcwd(), 'dist'),
            f'py2exestub-{package_version}-py3-none-win_amd64.whl'),
        'amd64')
    update_wheel(
        os.path.join(
            os.path.join(os.getcwd(), 'dist'),
            f'py2exestub-{package_version}-py3-none-any.whl'),
        os.path.join(
            os.path.join(os.getcwd(), 'dist'),
            f'py2exestub-{package_version}-py3-none-win_arm64.whl'),
        'arm64')
    print('Success')
    os.unlink(os.path.join(os.path.join(os.getcwd(), 'dist'), f'py2exestub-{package_version}-py3-none-any.whl'))
    files: list[tuple[str, str]] = list(set(_get_files('win32') + _get_files('amd64') + _get_files('arm64')))
    for file_name, arc_name in files:
        os.unlink(file_name)


if __name__ == '__main__':
    main()
