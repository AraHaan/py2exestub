import os
import platform
import urllib.request
import zipfile
import argparse
import shutil
import subprocess
import py_compile
from pathlib import Path
from ._resourceediting import replace_resources

__all__ = ['DistBuilder']


class DistBuilder:
    @staticmethod
    def detect_architecture() -> str:
        """
        Returns the correct architecture string for the CPython embeddable zip.
        Output: 'win32' or 'amd64' or 'arm64'
        """
        machine = platform.machine().lower()
        if "arm" in machine:
            return "arm64"

        if "64" in machine:
            return "amd64"

        return "win32"

    @staticmethod
    def download_and_extract_cpython(version: str, target_dir: str):
        """
        Downloads the CPython embeddable ZIP for the detected architecture
        and extracts it into the target directory.
        """
        target_dir = os.path.join(target_dir, "build_tmp")
        arch = DistBuilder.detect_architecture()
        filename = f"python-{version}-embed-{arch}.zip"
        url = f"https://www.python.org/ftp/python/{version}/{filename}"
        os.makedirs(target_dir, exist_ok=True)
        zip_path = os.path.join(os.path.join(os.path.dirname(__file__), 'python_cache'), filename)
        print(f"Detected architecture: {arch}")
        if not os.path.exists(zip_path):
            os.makedirs(os.path.join(os.path.dirname(__file__), 'python_cache'), exist_ok=True)
            print(f"Downloading {url} ...")
            urllib.request.urlretrieve(url, zip_path)
            print(f"Downloaded to {zip_path}")
        else:
            print(f"Using cached {zip_path}")

        allowed_exts = {".pyd", ".dll", ".zip"}
        print("Extracting...")
        with zipfile.ZipFile(zip_path, "r") as z:
            for member in z.infolist():
                _, ext = os.path.splitext(member.filename.lower())
                if ext in allowed_exts:
                    z.extract(member, target_dir)

        print(f"Extracted to {target_dir}")

    @staticmethod
    def write_data_files(target_dir: str, program_name: str, stub_name: str):
        # use the __file__ filled in by the import system as the package's path.
        files_to_copy = {
            f'{os.path.dirname(__file__)}/embed.exe': f'{target_dir}/build_tmp/{program_name}.exe',
            f'{os.path.dirname(__file__)}/stub.exe': f'{target_dir}/{stub_name}.exe',
            f'{os.path.dirname(__file__)}/memimport.py': f'{target_dir}/site-packages/memimport.py',
            f'{os.path.dirname(__file__)}/zipextimporter.py': f'{target_dir}/site-packages/zipextimporter.py',
        }

        for src, dst in files_to_copy.items():
            print(f'{src} -> {dst}')
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)

    @staticmethod
    def install_requirements(version: str, target_dir: str, requirements_file: str):
        print(f'Ensuring Python {version} is installed...')
        subprocess.check_output(['py', 'install', version], text=True)
        subprocess.check_output([
            'py', f'-{version}', '-m', 'pip', 'install', '--upgrade', '-r', requirements_file,
            '-t', os.path.join(target_dir, 'site-packages')], text=True)
        print(f"Dependencies installed into {os.path.join(target_dir, 'site-packages')}")

    @staticmethod
    def _compile_one_py(src, dest, name, optimize=2, checked=True) -> Path:
        if dest is not None:
            dest = str(dest)

        mode = (
            py_compile.PycInvalidationMode.CHECKED_HASH
            if checked
            else py_compile.PycInvalidationMode.UNCHECKED_HASH
        )

        try:
            return Path(
                py_compile.compile(
                    str(src),
                    dest,
                    str(name),
                    doraise=True,
                    optimize=optimize,
                    invalidation_mode=mode,
                )
            )
        except py_compile.PyCompileError:
            return None

    @staticmethod
    def _make_zip(root_folder: str, target_dir: str, name: str, support_pyd_packing: bool = False,
                  support_png_packing: bool = False):
        output = f'{os.path.join(target_dir, name)}.zip'
        full_name = os.path.join(root_folder, name)
        pathlist = [path for path in Path(full_name).rglob('*.py')]
        pathlist_other = [path for path in Path(full_name).rglob('*.pyi')]
        [pathlist_other.append(path) for path in Path(full_name).rglob('*.typed')]
        if support_png_packing:
            [pathlist_other.append(path) for path in Path(full_name).rglob('*.png')]
        # for when packing all site-packages to zip also include the ".dist-info" folders.
        # [pathlist_other.append(path) for path in Path(full_name).rglob('*.dist-info/*')]
        if support_pyd_packing:
            [pathlist_other.append(path) for path in Path(full_name).rglob('*.pyd')]
            [pathlist_other.append(path) for path in Path(full_name).rglob('*.dll')]
        os.mkdir(f'{full_name}_tmp')
        with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as zf:
            for path in pathlist:
                pyc = DistBuilder._compile_one_py(
                    f'{path.parents[0]}/{path.name}',
                    f'{str(path.parents[0]).replace(name, f"{full_name}_tmp")}/{path.name + "c"}',
                    path.name)
                if pyc:
                    try:
                        parents = str(path.parents[0]).replace(
                            name, '') if name == 'site-packages' or name == 'lib' else path.parents[0]
                        zf.write(str(pyc), f'{parents}/{path.name + "c"}')
                    finally:
                        try:
                            pyc.unlink()
                        except:
                            pass
            for path in pathlist_other:
                parents = str(path.parents[0]).replace(
                    name, '') if name == 'site-packages' or name == 'lib' else path.parents[0]
                zf.write(str(path), f'{parents}/{path.name}')

            shutil.rmtree(f'{full_name}_tmp', ignore_errors=True)

    @staticmethod
    def _make_zip2(target_dir: str, name, _path):
        output: str = f'{os.path.join(target_dir, name)}.zip'
        pathlist = [path for path in _path.rglob('*.*')]
        with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as zf:
            for path in pathlist:
                zf.write(str(path), f'{path.name}')

    @staticmethod
    def write_pth_file(target_dir: str, package_name: str, version: str):
        """ Writes <package_name>._pth into build_tmp with the correct paths for the embeddable Python distribution. """
        build_tmp = os.path.join(target_dir, "build_tmp")
        os.makedirs(build_tmp, exist_ok=True)
        major, minor, *_ = version.split(".")
        pth_path = os.path.join(build_tmp, f'{package_name}._pth')
        contents = (
            f"python{major}.{minor}.zip\n"
            "site-packages.zip\n"
            ".\n"
        )

        with open(pth_path, "w", encoding="utf-8") as f:
            f.write(contents)

    @staticmethod
    def main():
        parser = argparse.ArgumentParser(
            description="Embed python with a specific CPython runtime version.",
            prefix_chars="--")
        parser.add_argument(
            "version",
            nargs="?",
            default="3.14.2",
            help="Python version to download (default: 3.14.2)")
        parser.add_argument(
            "target_dir",
            nargs="?",
            default=os.path.join(os.getcwd(), "dist"),
            help="Directory where files should be extracted (default: ./dist)")
        parser.add_argument(
            "icon_file",
            help="The icon file to use in the embed and stub exes")
        parser.add_argument(
            "console_title",
            help="The text to use for the console title in the embed exe")
        parser.add_argument(
            "package_name",
            help="The package name to use to run the embed exe")
        parser.add_argument(
            "requirements",
            nargs="?",
            default=None,
            help="The path to the requirements.txt file that is used to install site-packages for the embed exe to use."
        )
        args = parser.parse_args()
        DistBuilder.download_and_extract_cpython(args.version, args.target_dir)
        if args.package_name and args.console_title and args.icon_file is not None:
            DistBuilder.write_data_files(args.target_dir, args.package_name, args.console_title)
            replace_resources(
                f'{args.target_dir}/build_tmp/{args.package_name}.exe',
                None,
                args.icon_file,
                None,
                None,
                False)
            if args.requirements is not None:
                DistBuilder.install_requirements(args.version, args.target_dir, args.requirements)

            DistBuilder._make_zip(
                args.target_dir,
                os.path.join(args.target_dir, 'build_tmp'),
                'site-packages',
                support_pyd_packing=True)
            DistBuilder.write_pth_file(args.target_dir, args.package_name, args.version)
            DistBuilder._make_zip2(
                args.target_dir,
                'files',
                Path(os.path.join(args.target_dir, 'build_tmp')))
            replace_resources(
                f'{os.path.join(args.target_dir, args.console_title)}.exe',
                f'{os.path.join(args.target_dir, "files")}.zip',
                args.icon_file,
                args.console_title,
                args.package_name,
                True)
