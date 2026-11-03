@ECHO OFF
REM Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
REM Phoenix command line launcher for cmd.exe; delegates to phoenix.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0phoenix.ps1" %*
