#!/bin/bash

echo "=== 开始编译 Rust 版本 ==="
cd /root/claude/ModernScreenshot

echo ""
echo "1. 检查 Rust 工具链..."
if ! command -v cargo &> /dev/null; then
    echo "❌ Rust 未安装，正在安装..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    source $HOME/.cargo/env
fi

rustc --version
cargo --version

echo ""
echo "2. 编译 Release 版本..."
cargo build --release 2>&1 | tail -20

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ 编译成功！"

    echo ""
    echo "3. 检查文件大小..."
    if [ -f target/release/modernscreenshot.exe ]; then
        SIZE=$(stat -f%z target/release/modernscreenshot.exe 2>/dev/null || stat -c%s target/release/modernscreenshot.exe 2>/dev/null)
        SIZE_MB=$(echo "scale=2; $SIZE / 1024 / 1024" | bc)

        echo "📊 文件大小: $SIZE_MB MB"

        if (( $(echo "$SIZE_MB < 5.0" | bc -l) )); then
            echo "✅ 体积达标！(< 5MB)"
        else
            echo "⚠️  体积超标 (目标 < 5MB)"
        fi
    else
        echo "❌ 未找到编译产物"
    fi

    echo ""
    echo "4. 文件信息..."
    ls -lh target/release/modernscreenshot.exe 2>/dev/null || ls -lh target/release/modernscreenshot.exe

else
    echo ""
    echo "❌ 编译失败"
    exit 1
fi

echo ""
echo "=== 编译完成 ==="
