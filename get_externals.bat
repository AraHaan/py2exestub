@echo off
mkdir externals
cd externals
git clone https://github.com/AraHaan/py2exe.git --branch cp313
git clone https://github.com/AraHaan/audioop.git
curl -L -o "%~dp0externals\miniz-3.1.0.zip" ^ https://github.com/richgel999/miniz/releases/download/3.1.0/miniz-3.1.0.zip
mkdir "miniz-3.1.0"
tar -xf "%~dp0externals\miniz-3.1.0.zip" -C "%~dp0externals\miniz-3.1.0"
