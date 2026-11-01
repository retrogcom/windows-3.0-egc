@echo off
set PATH=C:\BIN;Z:\
set INCLUDE=C:\INCLUDE
cd \OUT
C:\BIN\MASM /Mx /Zi /L C:\SRC\EGCBASE.ASM,EGCBASE.OBJ,EGCBASE.LST,NUL; > C:\LOGS\ASM.TXT
if errorlevel 1 goto fail
C:\BIN\LINK EGCBASE.OBJ,DISPLAY.DRV,EGCBASE.MAP/MAP/AL:16,,C:\SRC\EGCBASE.DEF; > C:\LOGS\LINK.TXT
if errorlevel 1 goto fail
C:\BIN\RC -r C:\SRC\EGCBASE.RC > C:\LOGS\RC1.TXT
if errorlevel 1 goto fail
C:\BIN\RC -v -b C:\SRC\EGCBASE.RES DISPLAY.DRV > C:\LOGS\RC2.TXT
if errorlevel 1 goto fail
C:\BIN\MAPSYM EGCBASE.MAP > C:\LOGS\SYM.TXT
if errorlevel 1 goto fail
copy DISPLAY.DRV EGCBASE.DRV > NUL
if errorlevel 1 goto fail
echo completed > C:\LOGS\BUILD.OK
exit
:fail
echo failed > C:\LOGS\BUILD.ERR
exit
