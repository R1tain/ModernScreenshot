using System;
using System.Drawing;
using System.Windows.Forms;

namespace ModernScreenshot
{
    /// <summary>
    /// 屏幕捕获辅助类
    /// </summary>
    public static class ScreenCaptureHelper
    {
        /// <summary>
        /// 捕获所有屏幕的完整区域
        /// </summary>
        public static (Bitmap Screenshot, Rectangle Bounds) CaptureAllScreens()
        {
            var bounds = CalculateAllScreensBounds();
            var bitmap = new Bitmap(bounds.Width, bounds.Height);

            using (Graphics g = Graphics.FromImage(bitmap))
            {
                g.CopyFromScreen(bounds.Location, Point.Empty, bounds.Size);
            }

            return (bitmap, bounds);
        }

        /// <summary>
        /// 计算所有屏幕的总边界
        /// </summary>
        public static Rectangle CalculateAllScreensBounds()
        {
            var screens = Screen.AllScreens;

            if (screens.Length == 0)
            {
                return new Rectangle(0, 0, 1920, 1080); // 默认分辨率
            }

            int minX = int.MaxValue;
            int minY = int.MaxValue;
            int maxX = int.MinValue;
            int maxY = int.MinValue;

            foreach (var screen in screens)
            {
                minX = Math.Min(minX, screen.Bounds.X);
                minY = Math.Min(minY, screen.Bounds.Y);
                maxX = Math.Max(maxX, screen.Bounds.Right);
                maxY = Math.Max(maxY, screen.Bounds.Bottom);
            }

            return new Rectangle(minX, minY, maxX - minX, maxY - minY);
        }

        /// <summary>
        /// 从源位图中裁剪指定区域并应用标注
        /// </summary>
        public static Bitmap CropAndRender(Bitmap source, Rectangle cropRect, Action<Graphics> renderAction)
        {
            var result = new Bitmap(cropRect.Width, cropRect.Height);

            using (var g = Graphics.FromImage(result))
            {
                RenderHelper.ConfigureHighQualityRendering(g);

                // 复制裁剪区域
                g.DrawImage(
                    source,
                    new Rectangle(0, 0, cropRect.Width, cropRect.Height),
                    cropRect,
                    GraphicsUnit.Pixel
                );

                // 应用标注（坐标转换）
                g.TranslateTransform(-cropRect.X, -cropRect.Y);
                renderAction?.Invoke(g);
            }

            return result;
        }
    }
}
