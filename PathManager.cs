using System;
using System.IO;

namespace ModernScreenshot
{
    /// <summary>
    /// 路径管理器 - 统一管理应用程序的所有文件路径
    /// </summary>
    public static class PathManager
    {
        private static string? _appDirectory;

        /// <summary>
        /// 应用程序根目录（默认 D:\ModernScreenshot）
        /// </summary>
        public static string AppDirectory
        {
            get
            {
                if (_appDirectory == null)
                {
                    // 优先使用应用程序所在目录
                    _appDirectory = AppDomain.CurrentDomain.BaseDirectory;

                    // 如果在 C 盘，尝试切换到 D 盘
                    if (_appDirectory.StartsWith("C:", StringComparison.OrdinalIgnoreCase))
                    {
                        string driveDPath = @"D:\ModernScreenshot";
                        if (Directory.Exists("D:\\"))
                        {
                            _appDirectory = driveDPath;
                        }
                    }

                    EnsureDirectoryExists(_appDirectory);
                }
                return _appDirectory;
            }
        }

        /// <summary>
        /// 截图保存目录
        /// </summary>
        public static string ScreenshotsDirectory
        {
            get
            {
                string path = Path.Combine(AppDirectory, "Screenshots");
                EnsureDirectoryExists(path);
                return path;
            }
        }

        /// <summary>
        /// 配置文件目录
        /// </summary>
        public static string ConfigDirectory
        {
            get
            {
                string path = Path.Combine(AppDirectory, "Config");
                EnsureDirectoryExists(path);
                return path;
            }
        }

        /// <summary>
        /// 临时文件目录
        /// </summary>
        public static string TempDirectory
        {
            get
            {
                string path = Path.Combine(AppDirectory, "Temp");
                EnsureDirectoryExists(path);
                return path;
            }
        }

        /// <summary>
        /// 获取默认截图保存路径
        /// </summary>
        public static string GetDefaultScreenshotPath()
        {
            string fileName = $"截图_{DateTime.Now:yyyyMMdd_HHmmss}.png";
            return Path.Combine(ScreenshotsDirectory, fileName);
        }

        /// <summary>
        /// 获取配置文件路径
        /// </summary>
        public static string GetConfigPath(string configName)
        {
            return Path.Combine(ConfigDirectory, configName);
        }

        /// <summary>
        /// 确保目录存在
        /// </summary>
        private static void EnsureDirectoryExists(string path)
        {
            if (!Directory.Exists(path))
            {
                Directory.CreateDirectory(path);
            }
        }

        /// <summary>
        /// 清理临时文件（可选）
        /// </summary>
        public static void ClearTempFiles()
        {
            try
            {
                if (Directory.Exists(TempDirectory))
                {
                    var files = Directory.GetFiles(TempDirectory);
                    foreach (var file in files)
                    {
                        try
                        {
                            File.Delete(file);
                        }
                        catch
                        {
                            // 忽略删除失败
                        }
                    }
                }
            }
            catch
            {
                // 忽略清理失败
            }
        }
    }
}
