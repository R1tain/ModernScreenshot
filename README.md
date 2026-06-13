# ModernScreenshot

现代化的 Windows 截图工具，轻量、快速、功能丰富。

[![Build Status](https://github.com/R1tain/ModernScreenshot/workflows/Build%20and%20Release/badge.svg)](https://github.com/R1tain/ModernScreenshot/actions)
[![Release](https://img.shields.io/github/v/release/R1tain/ModernScreenshot)](https://github.com/R1tain/ModernScreenshot/releases)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## ✨ 主要特性

- 🚀 **轻量快速** - 最小内存占用，启动迅速
- 🎨 **丰富工具** - 矩形、箭头、画笔、文字、马赛克
- 📋 **多种输出** - 复制到剪贴板、保存文件、钉在屏幕
- ⌨️ **快捷键** - F1 一键截图
- 💾 **路径管理** - 所有数据保存在安装目录，默认 D 盘
- 🔄 **撤销重做** - Ctrl+Z / Ctrl+Y
- 🖱️ **多屏支持** - 完美支持多显示器

## 📥 下载安装

前往 [Releases](https://github.com/R1tain/ModernScreenshot/releases) 下载最新版本：

| 文件 | 说明 | 大小 |
|------|------|------|
| `ModernScreenshot_Setup_x.x.x.exe` | 安装程序（推荐） | ~5 MB |
| `ModernScreenshot_Portable_x.x.x.zip` | 绿色便携版（需 .NET 8.0） | ~1 MB |
| `ModernScreenshot_Portable_Full_x.x.x.zip` | 完整便携版（含运行时） | ~70 MB |

### 系统要求
- Windows 10/11 (x64)
- .NET 8.0 Desktop Runtime（安装程序版本会自动处理）

## 📖 使用说明

### 快捷键
- `F1` - 启动截图
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
- **复制** - 复制到剪贴板，可直接粘贴
- **保存** - 保存到本地（默认：`D:\ModernScreenshot\Screenshots`）
- **钉图** - 钉在屏幕上，可拖动和缩放

## 📂 目录结构

```
D:\ModernScreenshot\              # 应用程序根目录
├── ModernScreenshot.exe          # 主程序
├── Screenshots\                  # 截图保存目录
├── Config\                       # 配置文件目录
└── Temp\                         # 临时文件目录
```

> **注意**: 所有数据保存在安装目录下，不会在 C 盘留下任何文件。卸载时会完全清理，请提前备份重要截图。

## 🏗️ 从源码构建

### 前置要求
- [.NET 8.0 SDK](https://dotnet.microsoft.com/download/dotnet/8.0)
- Windows 10/11

### 构建步骤
```bash
# 克隆仓库
git clone https://github.com/R1tain/ModernScreenshot.git
cd ModernScreenshot

# 还原依赖
dotnet restore

# 编译
dotnet build -c Release

# 发布（自包含版本）
dotnet publish -c Release -r win-x64 --self-contained true
```

### 生成安装程序
需要安装 [Inno Setup](https://jrsoftware.org/isdl.php) 和 [WiX Toolset](https://wixtoolset.org/releases/)，然后运行 GitHub Actions 工作流自动构建。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

MIT License

## 🙏 致谢

感谢所有贡献者和用户的支持！

---

**技术栈**: .NET 8.0 | WinForms | GDI+ | GitHub Actions
