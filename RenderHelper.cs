using System;
using System.Drawing;

namespace ModernScreenshot
{
    /// <summary>
    /// 图形渲染辅助类
    /// </summary>
    public static class RenderHelper
    {
        /// <summary>
        /// 计算颜色的对比色（黑色或白色）
        /// </summary>
        public static Color GetContrastColor(Color color)
        {
            double luminance = 0.299 * color.R + 0.587 * color.G + 0.114 * color.B;
            return luminance > 128 ? Color.Black : Color.White;
        }

        /// <summary>
        /// 从两个点创建矩形
        /// </summary>
        public static Rectangle GetRectFromPoints(Point p1, Point p2)
        {
            int x = Math.Min(p1.X, p2.X);
            int y = Math.Min(p1.Y, p2.Y);
            int width = Math.Abs(p2.X - p1.X);
            int height = Math.Abs(p2.Y - p1.Y);
            return new Rectangle(x, y, width, height);
        }

        /// <summary>
        /// 配置高质量图形渲染
        /// </summary>
        public static void ConfigureHighQualityRendering(Graphics graphics)
        {
            graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
            graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            graphics.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
        }

        /// <summary>
        /// 绘制箭头
        /// </summary>
        public static void DrawArrow(Graphics g, Pen pen, Point start, Point end)
        {
            // 绘制线段
            g.DrawLine(pen, start, end);

            // 计算箭头角度
            double angle = Math.Atan2(end.Y - start.Y, end.X - start.X);

            // 计算箭头两个端点
            Point arrow1 = new Point(
                end.X - (int)(AppConstants.ARROW_SIZE * Math.Cos(angle - AppConstants.ARROW_ANGLE)),
                end.Y - (int)(AppConstants.ARROW_SIZE * Math.Sin(angle - AppConstants.ARROW_ANGLE))
            );

            Point arrow2 = new Point(
                end.X - (int)(AppConstants.ARROW_SIZE * Math.Cos(angle + AppConstants.ARROW_ANGLE)),
                end.Y - (int)(AppConstants.ARROW_SIZE * Math.Sin(angle + AppConstants.ARROW_ANGLE))
            );

            // 填充箭头
            using (var brush = new SolidBrush(pen.Color))
            {
                g.FillPolygon(brush, new Point[] { end, arrow1, arrow2 });
            }
        }

        /// <summary>
        /// 应用马赛克效果
        /// </summary>
        public static void ApplyMosaic(Graphics g, Bitmap sourceBitmap, Rectangle rect)
        {
            if (sourceBitmap == null) return;

            // 裁剪到有效区域
            Rectangle clampedRect = Rectangle.Intersect(
                rect,
                new Rectangle(0, 0, sourceBitmap.Width, sourceBitmap.Height)
            );

            if (clampedRect.Width <= 0 || clampedRect.Height <= 0) return;

            // 使用临时位图提高性能
            using (Bitmap tempBitmap = new Bitmap(clampedRect.Width, clampedRect.Height))
            {
                using (Graphics tempG = Graphics.FromImage(tempBitmap))
                {
                    tempG.DrawImage(
                        sourceBitmap,
                        new Rectangle(0, 0, clampedRect.Width, clampedRect.Height),
                        clampedRect,
                        GraphicsUnit.Pixel
                    );
                }

                // 绘制马赛克块
                for (int y = 0; y < clampedRect.Height; y += AppConstants.MOSAIC_BLOCK_SIZE)
                {
                    for (int x = 0; x < clampedRect.Width; x += AppConstants.MOSAIC_BLOCK_SIZE)
                    {
                        int blockWidth = Math.Min(AppConstants.MOSAIC_BLOCK_SIZE, clampedRect.Width - x);
                        int blockHeight = Math.Min(AppConstants.MOSAIC_BLOCK_SIZE, clampedRect.Height - y);

                        // 采样中心点颜色
                        int sampleX = Math.Min(x + AppConstants.MOSAIC_BLOCK_SIZE / 2, clampedRect.Width - 1);
                        int sampleY = Math.Min(y + AppConstants.MOSAIC_BLOCK_SIZE / 2, clampedRect.Height - 1);
                        Color pixelColor = tempBitmap.GetPixel(sampleX, sampleY);

                        using (var brush = new SolidBrush(pixelColor))
                        {
                            g.FillRectangle(brush, clampedRect.X + x, clampedRect.Y + y, blockWidth, blockHeight);
                        }
                    }
                }
            }
        }

        /// <summary>
        /// 绘制带阴影的文本
        /// </summary>
        public static void DrawTextWithBackground(Graphics g, string text, Font font, Brush brush, PointF location)
        {
            var textSize = g.MeasureString(text, font);
            var backgroundRect = new RectangleF(
                location.X - 2,
                location.Y - 2,
                textSize.Width + 4,
                textSize.Height + 4
            );

            // 绘制半透明背景
            using (var backBrush = new SolidBrush(Color.FromArgb(180, 0, 0, 0)))
            {
                g.FillRectangle(backBrush, backgroundRect);
            }

            g.DrawString(text, font, brush, location);
        }
    }
}
