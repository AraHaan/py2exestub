import os
import platform
import urllib.request
import zipfile
import argparse
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
    def download_and_extract_cpython(version: str, target_dir: str, append_tmp: bool):
        """
        Downloads the CPython embeddable ZIP for the detected architecture
        and extracts it into the target directory.
        """
        if append_tmp:
            target_dir = os.path.join(target_dir, "build_tmp")
        else:
            target_dir = os.path.join(target_dir, "embed")

        arch = DistBuilder.detect_architecture()
        filename = f"python-{version}-embed-{arch}.zip"
        url = f"https://www.python.org/ftp/python/{version}/{filename}"
        os.makedirs(target_dir, exist_ok=True)
        zip_path = os.path.join(target_dir, filename)
        print(f"Detected architecture: {arch}")
        print(f"Downloading {url} ...")
        urllib.request.urlretrieve(url, zip_path)
        print(f"Downloaded to {zip_path}")
        allowed_exts = {".pyd", ".dll", ".zip"}
        print("Extracting...")
        with zipfile.ZipFile(zip_path, "r") as z:
            if append_tmp:
                for member in z.infolist():
                    _, ext = os.path.splitext(member.filename.lower())
                    if ext in allowed_exts:
                        z.extract(member, target_dir)
            else:
                z.extractall(target_dir)

        os.unlink(zip_path)
        print(f"Extracted to {target_dir}")

    @staticmethod
    def main():
        parser = argparse.ArgumentParser(
            description="Download and extract CPython embeddable ZIP with filtered files.",
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
        args = parser.parse_args()
        DistBuilder.download_and_extract_cpython(args.version, args.target_dir, True)
        DistBuilder.download_and_extract_cpython(args.version, args.target_dir, False)
