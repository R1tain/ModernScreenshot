@echo off
REM 现代截图工具 - 构建脚本

echo ====================================
echo 现代截图工具 - 构建脚本
echo ====================================
echo.

REM 检查 .NET SDK
dotnet --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未找到 .NET SDK，请先安装 .NET 6.0 SDK
    echo 下载地址: https://dotnet.microsoft.com/download
    pause
    exit /b 1
)

echo [1/4] 清理旧的构建文件...
dotnet clean -c Release
if %errorlevel% neq 0 goto error

echo.
echo [2/4] 还原 NuGet 包...
dotnet restore
if %errorlevel% neq 0 goto error

echo.
echo [3/4] 编译项目...
dotnet build -c Release
if %errorlevel% neq 0 goto error

echo.
echo [4/4] 发布独立可执行文件...
dotnet publish -c Release -r win-x64 --self-contained -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true
if %errorlevel% neq 0 goto error

echo.
echo ====================================
echo 构建成功！
echo ====================================
echo.
echo 可执行文件位置:
echo bin\Release\net6.0-windows\win-x64\publish\ModernScreenshot.exe
echo.
echo 文件大小:
dir /s bin\Release\net6.0-windows\win-x64\publish\ModernScreenshot.exe | find "ModernScreenshot.exe"
echo.
pause
exit /b 0

:error
echo.
echo ====================================
echo 构建失败！
echo ====================================
pause
exit /b 1
