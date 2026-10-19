@echo off
rem Instalador do Def Jam: Fight for NY (port de PC): monta o jogo a partir da sua .iso. Codigo legivel em source\setup\instalar.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0source\setup\instalar.ps1" %*
