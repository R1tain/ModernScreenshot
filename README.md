# ModernScreenshot

现代化的 Windows 截图工具，轻量、快速、功能丰富。

**🦀 Rust 重写版本现已发布！体积从 25MB 降至 < 3MB**

[![Build Status](https://github.com/R1tain/ModernScreenshot/workflows/Build%20and%20Release/badge.svg)](https://github.com/R1tain/ModernScreenshot/actions)
[![Release](https://img.shields.io/github/v/release/R1tain/ModernScreenshot)](https://github.com/R1tain/ModernScreenshot/releases)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## ✨ 主要特性

- 🚀 **轻量快速** - 仅 2-3MB，启动 < 50ms
- 🎨 **现代化 UI** - Pixpin 风格毛玻璃工具栏
- 🔧 **丰富工具** - 矩形、箭头、画笔、文字、马赛克
- 📋 **多种输出** - 复制到剪贴板、保存文件、钉在屏幕
- ⌨️ **快捷键** - Ctrl+1 一键截图
- 💾 **无依赖** - 单文件绿色版，无需 .NET 运行时
- 🔄 **撤销重做** - Ctrl+Z / Ctrl+Y
- 🖱️ **多屏支持** - 完美支持多显示器

## 📥 下载安装

前往 [Releases](https://github.com/R1tain/ModernScreenshot/releases) 下载最新版本：

### 🦀 Rust 版本（推荐）

| 文件 | 说明 | 大小 |
|------|------|------|
| `ModernScreenshot_Rust_*.zip` | **Rust 单文件版（推荐）** | **~2-3 MB** ⭐ |

- ✅ 无需任何运行时
- ✅ 启动极快 (< 50ms)
- ✅ 内存占用极低 (~8MB)
- ✅ 单文件绿色版

### C# 版本（已弃用）

由于 Windows Forms 不支持 trimming，C# 版本体积无法优化到 5MB 以下，已停止开发。

### 系统要求
- Windows 10/11 (x64)
- 无需任何运行时或依赖

## 📖 使用说明

### 快捷键
- `Ctrl+1` - 启动截图
- `Esc` - 取消截图
- `Ctrl+C` - 复制到剪贴板
- `Ctrl+S` - 保存到文件
- `Ctrl+Z` - 撤销
- `Ctrl+Y` - 重做

### 标注工具
- 🟦 **矩形** - 绘制矩形标注
- ➡️ **箭头** - 绘制指示箭头
- ✏️ **画笔** - 自由绘制
- 📝 **文字** - 添加文字说明
- 🔲 **马赛克** - 模糊隐私信息

### 输出方式
- 💾 **保存** - 保存为图片文件
- 📋 **复制** - 复制到剪贴板
- 📌 **钉图** - 钉在屏幕上
- ❌ **取消** - 取消截图

## 🔧 开发

### Rust 版本

```bash
# 克隆项目
git clone https://github.com/R1tain/ModernScreenshot.git
cd ModernScreenshot

# 编译
cargo build --release

# 运行
target/release/modernscreenshot.exe
```

### 技术栈
- Rust + Windows API (Win32)
- GDI/GDI+ 绘图
- 无第三方 GUI 框架依赖

## 📊 性能对比

| 指标 | C# 版本 | Rust 版本 |
|------|---------|-----------|
| 文件体积 | ~25MB | **2-3MB** ✅ |
| 启动时间 | ~500ms | **< 50ms** ✅ |
| 内存占用 | ~30MB | **~8MB** ✅ |
| 运行时依赖 | .NET 8.0 | **无** ✅ |

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可

MIT License

## 🙏 致谢

设计灵感来自 [Pixpin](https://pixpinapp.com/) 和 [Snipaste](https://www.snipaste.com/)
