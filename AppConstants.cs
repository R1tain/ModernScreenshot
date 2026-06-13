using System;
using System.Drawing;

namespace ModernScreenshot
{
    /// <summary>
    /// 应用程序常量定义
    /// </summary>
    public static class AppConstants
    {
        // 路径配置
        public const string DEFAULT_INSTALL_DRIVE = "D:";
        public const string APP_NAME = "ModernScreenshot";

        // 热键配置
        public const uint HOTKEY_MODIFIER_NONE = 0x0000;
        public const uint VK_F1 = 0x70;
        public const int HOTKEY_ID = 1;

        // 绘图配置
        public const int DEFAULT_DRAW_WIDTH = 3;
        public const int MOSAIC_BLOCK_SIZE = 10;
        public const int ARROW_SIZE = 15;
        public const double ARROW_ANGLE = Math.PI / 6;

        // 颜色配置
        public static readonly Color DEFAULT_DRAW_COLOR = Color.Red;
        public static readonly Color OVERLAY_COLOR = Color.FromArgb(100, 0, 0, 0);
        public static readonly Color SELECTION_BORDER_COLOR = Color.DodgerBlue;
        public static readonly Color TOOLBAR_BACKGROUND = Color.FromArgb(240, 50, 50, 50);

        // 工具栏配置
        public const int TOOLBAR_HEIGHT = 50;
        public const int TOOLBAR_WIDTH = 500;
        public const int TOOLBAR_MARGIN = 5;

        // 性能配置
        public const int MAX_UNDO_STEPS = 50;

        // 字体配置
        public static readonly string DEFAULT_FONT_FAMILY = "Microsoft YaHei";
        public const int DEFAULT_FONT_SIZE = 12;
        public const int INPUT_FONT_SIZE = 10;

        // 缩放配置
        public const double ZOOM_FACTOR_IN = 1.1;
        public const double ZOOM_FACTOR_OUT = 0.9;
        public const int MIN_ZOOM_SIZE = 50;
        public const int MAX_ZOOM_SIZE = 5000;

        // 通知配置
        public const int NOTIFICATION_WIDTH = 200;
        public const int NOTIFICATION_HEIGHT = 60;
        public const int NOTIFICATION_DURATION = 1500;

        // 预设颜色
        public static readonly (Color Color, string Name)[] PRESET_COLORS = new[]
        {
            (Color.Red, "红色"),
            (Color.Orange, "橙色"),
            (Color.Yellow, "黄色"),
            (Color.LimeGreen, "绿色"),
            (Color.DodgerBlue, "蓝色"),
            (Color.Purple, "紫色"),
            (Color.Black, "黑色"),
            (Color.White, "白色")
        };

        // 预设线宽
        public static readonly int[] PRESET_WIDTHS = { 1, 2, 3, 5, 8, 10 };
    }
}
