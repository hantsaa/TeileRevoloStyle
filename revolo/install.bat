@echo off
rem Installe RevoloStyle.vst3 pour FL Studio (et tout hôte VST3) — clic droit : "Exécuter en tant qu'administrateur"
set "DEST=%CommonProgramFiles%\VST3"
echo Installation de RevoloStyle.vst3 dans "%DEST%" ...
xcopy /E /I /Y "%~dp0RevoloStyle.vst3" "%DEST%\RevoloStyle.vst3"
if errorlevel 1 (
  echo.
  echo ECHEC : relance ce fichier en tant qu'administrateur.
) else (
  echo.
  echo OK. Dans FL Studio : Options ^> Manage plugins ^> Find installed plugins.
)
pause
