@echo off
echo mkfs
mkfs
echo copying shell
copy/y %root%\bld\bin\sh.exe a:\
copy/y %root%\bld\bin\hello.exe a:\hello
copy/y %root%\data\alice.txt a:\

