using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace ModernScreenshot
{
    /// <summary>
    /// 现代化毛玻璃风格工具栏
    /// </summary>
    public class ModernToolbar : Panel
    {
        // Aero Glass API
        [DllImport("dwmapi.dll")]
        private static extern int DwmExtendFrameIntoClientArea(IntPtr hwnd, ref MARGINS margins);

        [DllImport("dwmapi.dll")]
        private static extern int DwmIsCompositionEnabled(out bool enabled);

        [StructLayout(LayoutKind.Sequential)]
        private struct MARGINS
        {
            public int Left, Right, Top, Bottom;
        }

        private bool isGlassEnabled = false;

        public ModernToolbar()
        {
            this.DoubleBuffered = true;
            this.BackColor = Color.FromArgb(230, 40, 40, 40); // 半透明深色背景
            this.Height = 60;
            this.Padding = new Padding(15, 10, 15, 10);

            // 检查 Aero Glass 是否可用
            DwmIsCompositionEnabled(out isGlassEnabled);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;

            // 绘制毛玻璃背景
            using (LinearGradientBrush brush = new LinearGradientBrush(
                this.ClientRectangle,
                Color.FromArgb(200, 45, 45, 50),
                Color.FromArgb(220, 35, 35, 40),
                LinearGradientMode.Vertical))
            {
                g.FillRoundedRectangle(brush, 0, 0, this.Width, this.Height, 12);
            }

            // 顶部高光
            using (LinearGradientBrush highlight = new LinearGradientBrush(
                new Rectangle(0, 0, this.Width, this.Height / 2),
                Color.FromArgb(40, 255, 255, 255),
                Color.FromArgb(0, 255, 255, 255),
                LinearGradientMode.Vertical))
            {
                g.FillRoundedRectangle(highlight, 0, 0, this.Width, this.Height / 2, 12);
            }

            // 绘制边框
            using (Pen borderPen = new Pen(Color.FromArgb(80, 255, 255, 255), 1.5f))
            {
                g.DrawRoundedRectangle(borderPen, 0, 0, this.Width - 1, this.Height - 1, 12);
            }
        }

        protected override CreateParams CreateParams
        {
            get
            {
                CreateParams cp = base.CreateParams;
                cp.ExStyle |= 0x00000020; // WS_EX_TRANSPARENT
                return cp;
            }
        }
    }

    /// <summary>
    /// 现代化圆形按钮
    /// </summary>
    public class ModernButton : Button
    {
        private bool isHovered = false;
        private bool isPressed = false;
        private Color accentColor = Color.FromArgb(0, 120, 215); // Windows 蓝色

        public string IconText { get; set; } = "";
        public bool IsPrimary { get; set; } = false;

        public ModernButton()
        {
            this.FlatStyle = FlatStyle.Flat;
            this.FlatAppearance.BorderSize = 0;
            this.BackColor = Color.Transparent;
            this.ForeColor = Color.White;
            this.Font = new Font("Segoe UI", 9F, FontStyle.Regular);
            this.Size = new Size(80, 40);
            this.Cursor = Cursors.Hand;
            this.FlatAppearance.MouseOverBackColor = Color.Transparent;
            this.FlatAppearance.MouseDownBackColor = Color.Transparent;
        }

        protected override void OnMouseEnter(EventArgs e)
        {
            base.OnMouseEnter(e);
            isHovered = true;
            this.Invalidate();
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            base.OnMouseLeave(e);
            isHovered = false;
            isPressed = false;
            this.Invalidate();
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
            isPressed = true;
            this.Invalidate();
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            base.OnMouseUp(e);
            isPressed = false;
            this.Invalidate();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;

            Rectangle rect = new Rectangle(0, 0, this.Width - 1, this.Height - 1);

            // 背景色
            Color bgColor;
            if (IsPrimary)
            {
                bgColor = isPressed ? Color.FromArgb(200, 0, 100, 180) :
                         isHovered ? Color.FromArgb(220, 0, 110, 195) :
                         Color.FromArgb(200, 0, 120, 215);
            }
            else
            {
                bgColor = isPressed ? Color.FromArgb(120, 80, 80, 80) :
                         isHovered ? Color.FromArgb(100, 100, 100, 100) :
                         Color.FromArgb(80, 60, 60, 60);
            }

            // 绘制圆角背景
            using (SolidBrush brush = new SolidBrush(bgColor))
            {
                g.FillRoundedRectangle(brush, rect.X, rect.Y, rect.Width, rect.Height, 8);
            }

            // 绘制边框
            Color borderColor = IsPrimary ?
                Color.FromArgb(180, 100, 180, 255) :
                Color.FromArgb(80, 200, 200, 200);

            using (Pen pen = new Pen(borderColor, 1f))
            {
                g.DrawRoundedRectangle(pen, rect.X, rect.Y, rect.Width, rect.Height, 8);
            }

            // 绘制图标和文字
            if (!string.IsNullOrEmpty(IconText))
            {
                using (Font iconFont = new Font("Segoe UI Symbol", 12F, FontStyle.Regular))
                {
                    SizeF iconSize = g.MeasureString(IconText, iconFont);
                    PointF iconPoint = new PointF(
                        (this.Width - iconSize.Width) / 2,
                        6
                    );
                    g.DrawString(IconText, iconFont, Brushes.White, iconPoint);
                }

                using (Font textFont = new Font("Segoe UI", 8F, FontStyle.Regular))
                {
                    SizeF textSize = g.MeasureString(this.Text, textFont);
                    PointF textPoint = new PointF(
                        (this.Width - textSize.Width) / 2,
                        this.Height - textSize.Height - 4
                    );
                    g.DrawString(this.Text, textFont, Brushes.White, textPoint);
                }
            }
            else
            {
                // 只有文字，居中显示
                using (Font font = new Font("Segoe UI", 9F, FontStyle.Regular))
                {
                    SizeF textSize = g.MeasureString(this.Text, font);
                    PointF textPoint = new PointF(
                        (this.Width - textSize.Width) / 2,
                        (this.Height - textSize.Height) / 2
                    );
                    g.DrawString(this.Text, font, Brushes.White, textPoint);
                }
            }
        }
    }

    /// <summary>
    /// 图形扩展方法
    /// </summary>
    public static class GraphicsExtensions
    {
        public static void FillRoundedRectangle(this Graphics g, Brush brush, float x, float y, float width, float height, float radius)
        {
            using (GraphicsPath path = GetRoundedRectPath(x, y, width, height, radius))
            {
                g.FillPath(brush, path);
            }
        }

        public static void DrawRoundedRectangle(this Graphics g, Pen pen, float x, float y, float width, float height, float radius)
        {
            using (GraphicsPath path = GetRoundedRectPath(x, y, width, height, radius))
            {
                g.DrawPath(pen, path);
            }
        }

        private static GraphicsPath GetRoundedRectPath(float x, float y, float width, float height, float radius)
        {
            GraphicsPath path = new GraphicsPath();
            float diameter = radius * 2;

            path.AddArc(x, y, diameter, diameter, 180, 90);
            path.AddArc(x + width - diameter, y, diameter, diameter, 270, 90);
            path.AddArc(x + width - diameter, y + height - diameter, diameter, diameter, 0, 90);
            path.AddArc(x, y + height - diameter, diameter, diameter, 90, 90);
            path.CloseFigure();

            return path;
        }
    }
}
