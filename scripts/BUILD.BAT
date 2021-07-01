@echo off
rem
rem Save the current directory
rem
set root=c:\src\os
set cdset=
if not exist %ROOT%\bld\bin\cdset.exe goto :nocdset
%ROOT%\bld\bin\cdset.exe cdset > %TEMP%\cdset.bat
call %TEMP%\cdset.bat
:nocdset

del %root%\build.log

set MAKE=nmake /nologo /s %1 %2 %3 %4 %5 %6 %7 %8 %9

rem 
rem utilities
rem
echo f2array  >> %root%\build.log
cd %root%\hosted\f2array  >> %root%\build.log
%make%	       >> %root%\build.log

echo cdset >> %root%\build.log
cd %root%\hosted\cdset >> %root%\build.log
%make% >> %root%\build.log

echo escpath >> %root%\build.log
cd %root%\hosted\escpath
%make% >> %root%\build.log

echo mkver >> %root%\build.log
cd %root%\hosted\mkver
%make% >> %root%\build.log

rem
rem boot sector
rem
echo bootsect >> %root%\build.log
cd %root%\bootsect >> %root%\build.log
%make% >> %root%\build.log

rem
rem hosted
rem
echo fat (hosted) >> %root%\build.log
cd %root%\hosted\fat >> %root%\build.log
%make% >> %root%\build.log

echo blkcache >> %root%\build.log
cd %root%\hosted\blkcache >> %root%\build.log
%make% >> %root%\build.log

echo mkfs >> %root%\build.log
cd %root%\hosted\mkfs >> %root%\build.log
%make% >> %root%\build.log

echo fscopy >> %root%\build.log
cd %root%\hosted\fscopy >> %root%\build.log
%make% >> %root%\build.log

rem
rem kernel
rem    
echo fdd >> %root%\build.log
cd %root%\kernel\fdd
%make% >> %root%\build.log

echo tty >> %root%\build.log
cd %root%\kernel\tty
%make% >> %root%\build.log

echo ttycook >> %root%\build.log
cd %root%\kernel\ttycook
%make% >> %root%\build.log

echo console >> %root%\build.log
cd %root%\kernel\console
%make% >> %root%\build.log

echo fat (kernel) >> %root%\build.log
cd %root%\kernel\fat 
%make% /f makefile >> %root%\build.log

echo sched >> %root%\build.log
cd %root%\kernel\sched 
%make% /f makefile >> %root%\build.log

echo kernel >> %root%\build.log
cd %root%\kernel 
%make% >> %root%\build.log

rem
rem clib and native programs
rem
echo crt0 >> %root%\build.log
cd %root%\clib\crt0
%make% >> %root%\build.log

echo crt >> %root%\build.log
cd %root%\clib\lib
%make% >> %root%\build.log

echo shell >> %root%\build.log
cd %root%\native\shell
%make% >> %root%\build.log

echo hello >> %root%\build.log
cd %root%\native\hello
%make% >> %root%\build.log



rem
rem Restore the current directory
rem
if "%cdset%" == "" goto :nocdset2
cd %cdset%
:nocdset2

rem
rem Report errors
rem
find "error" %root%\build.log

