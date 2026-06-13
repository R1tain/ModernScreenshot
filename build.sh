#!/bin/bash

# 现代截图工具 - 构建脚本（仅用于开发，需要在 Windows 上运行）

echo "===================================="
echo "现代截图工具 - 构建脚本"
echo "===================================="
echo ""

# 检查 .NET SDK
if ! command -v dotnet &> /dev/null; then
    echo "[错误] 未找到 .NET SDK，请先安装 .NET 6.0 SDK"
    echo "下载地址: https://dotnet.microsoft.com/download"
    exit 1
fi

echo "[1/4] 清理旧的构建文件..."
dotnet clean -c Release
if [ $? -ne 0 ]; then
    echo "[错误] 清理失败"
    exit 1
fi

echo ""
echo "[2/4] 还原 NuGet 包..."
dotnet restore
if [ $? -ne 0 ]; then
    echo "[错误] 还原失败"
    exit 1
fi

echo ""
echo "[3/4] 编译项目..."
dotnet build -c Release
if [ $? -ne 0 ]; then
    echo "[错误] 编译失败"
    exit 1
fi

echo ""
echo "[4/4] 发布独立可执行文件..."
dotnet publish -c Release -r win-x64 --self-contained -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true
if [ $? -ne 0 ]; then
    echo "[错误] 发布失败"
    exit 1
fi

echo ""
echo "===================================="
echo "构建成功！"
echo "===================================="
echo ""
echo "可执行文件位置:"
echo "bin/Release/net6.0-windows/win-x64/publish/ModernScreenshot.exe"
echo ""
ls -lh bin/Release/net6.0-windows/win-x64/publish/ModernScreenshot.exe 2>/dev/null || echo "文件大小信息不可用"
