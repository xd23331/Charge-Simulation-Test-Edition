@echo off
rem ============================================================
rem  城市新能源汽车充电资源调度仿真系统 - 前端启动脚本
rem  运行库 DLL 已放在 build 目录下，可直接双击运行。
rem ============================================================
setlocal
cd /d "%~dp0"

if not exist "build\charging_system_gui.exe" (
    echo [错误] 未找到 build\charging_system_gui.exe
    echo 请先编译前端程序。
    pause
    exit /b 1
)

start "" "build\charging_system_gui.exe"
endlocal