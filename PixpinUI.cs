using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace ModernScreenshot
{
    /// <summary>
    /// 现代化工具栏 - Pixpin 风格
    /// </summary>
    public class PixpinToolbar : Panel
    {
        public PixpinToolbar()
        {
            this.DoubleBuffered = true;
            this.BackColor = Color.Transparent;
            this.Height = 50;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;

            // 深色半透明背景 - Pixpin 风格
            using (SolidBrush bgBrush = new SolidBrush(Color.FromArgb(240, 40, 42, 46)))
            {
                GraphicsPath path = GetRoundedRectPath(1, 1, this.Width - 2, this.Height - 2, 8);
                g.FillPath(bgBrush, path);
            }

            // 内发光边框
            using (Pen innerGlow = new Pen(Color.FromArgb(60, 255, 255, 255), 1))
            {
                GraphicsPath path = GetRoundedRectPath(1, 1, this.Width - 2, this.Height - 2, 8);
                g.DrawPath(innerGlow, path);
            }

            // 外阴影（黑色边框）
            using (Pen shadow = new Pen(Color.FromArgb(120, 0, 0, 0), 1))
            {
                GraphicsPath path = GetRoundedRectPath(0, 0, this.Width - 1, this.Height - 1, 8);
                g.DrawPath(shadow, path);
            }
        }

        private GraphicsPath GetRoundedRectPath(float x, float y, float width, float height, float radius)
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

    /// <summary>
    /// Pixpin 风格工具按钮
    /// </summary>
    public class PixpinButton : Control
    {
        private bool isHovered = false;
        private bool isPressed = false;
        private bool isSelected = false;

        public string IconText { get; set; } = "📷";
        public string ButtonText { get; set; } = "按钮";
        public bool IsPrimary { get; set; } = false;
        public bool IsIconOnly { get; set; } = false;

        public event EventHandler? ButtonClick;

        public PixpinButton()
        {
            this.DoubleBuffered = true;
            this.Size = new Size(44, 36);
            this.Cursor = Cursors.Hand;
            this.Font = new Font("Microsoft YaHei UI", 9F);
        }

        public bool Selected
        {
            get => isSelected;
            set
            {
                if (isSelected != value)
                {
                    isSelected = value;
                    this.Invalidate();
                }
            }
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
            if (e.Button == MouseButtons.Left)
            {
                isPressed = true;
                this.Invalidate();
            }
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            base.OnMouseUp(e);
            if (e.Button == MouseButtons.Left && isPressed)
            {
                isPressed = false;
                this.Invalidate();
                ButtonClick?.Invoke(this, EventArgs.Empty);
            }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;

            Rectangle rect = new Rectangle(1, 1, this.Width - 2, this.Height - 2);

            // 背景色
            Color bgColor;
            if (IsPrimary)
            {
                // 保存按钮 - 蓝色渐变
                if (isPressed)
                    bgColor = Color.FromArgb(255, 16, 110, 190);
                else if (isHovered)
                    bgColor = Color.FromArgb(255, 24, 132, 228);
                else
                    bgColor = Color.FromArgb(255, 20, 122, 215);
            }
            else if (isSelected)
            {
                // 工具选中状态 - 蓝色半透明
                bgColor = Color.FromArgb(180, 40, 130, 220);
            }
            else if (isHovered)
            {
                // 悬停状态
                bgColor = Color.FromArgb(140, 70, 73, 78);
            }
            else if (isPressed)
            {
                // 按下状态
                bgColor = Color.FromArgb(160, 50, 52, 56);
            }
            else
            {
                // 默认状态 - 透明
                bgColor = Color.FromArgb(0, 60, 63, 68);
            }

            // 绘制背景
            if (bgColor.A > 0)
            {
                using (SolidBrush brush = new SolidBrush(bgColor))
                using (GraphicsPath path = GetRoundedRect(rect, 6))
                {
                    g.FillPath(brush, path);
                }

                // 绘制边框（仅在悬停、按下或选中时）
                if (isHovered || isPressed || isSelected)
                {
                    Color borderColor = IsPrimary ?
                        Color.FromArgb(200, 100, 180, 255) :
                        isSelected ?
                        Color.FromArgb(200, 80, 160, 255) :
                        Color.FromArgb(100, 130, 135, 144);

                    using (Pen pen = new Pen(borderColor, 1))
                    using (GraphicsPath path = GetRoundedRect(rect, 6))
                    {
                        g.DrawPath(pen, path);
                    }
                }
            }

            // 绘制图标和文字
            if (IsIconOnly)
            {
                // 只显示图标（工具按钮）
                DrawIcon(g, IconText);
            }
            else
            {
                // 显示图标 + 文字（操作按钮）
                DrawIconWithText(g, IconText, ButtonText);
            }
        }

        private void DrawIcon(Graphics g, string icon)
        {
            using (Font iconFont = new Font("Segoe UI Symbol", 16F, FontStyle.Regular))
            {
                SizeF size = g.MeasureString(icon, iconFont);
                PointF point = new PointF(
                    (this.Width - size.Width) / 2,
                    (this.Height - size.Height) / 2
                );

                // 绘制图标阴影
                g.DrawString(icon, iconFont, new SolidBrush(Color.FromArgb(80, 0, 0, 0)),
                    point.X + 0.5f, point.Y + 0.5f);

                // 绘制图标
                g.DrawString(icon, iconFont, Brushes.White, point);
            }
        }

        private void DrawIconWithText(Graphics g, string icon, string text)
        {
            // 图标
            using (Font iconFont = new Font("Segoe UI Emoji", 13F, FontStyle.Regular))
            {
                SizeF iconSize = g.MeasureString(icon, iconFont);
                float iconX = 8;
                float iconY = (this.Height - iconSize.Height) / 2;

                g.DrawString(icon, iconFont, new SolidBrush(Color.FromArgb(80, 0, 0, 0)),
                    iconX + 0.5f, iconY + 0.5f);
                g.DrawString(icon, iconFont, Brushes.White, iconX, iconY);
            }

            // 文字
            using (Font textFont = new Font("Microsoft YaHei UI", 9F, FontStyle.Regular))
            {
                SizeF textSize = g.MeasureString(text, textFont);
                float textX = 28;
                float textY = (this.Height - textSize.Height) / 2;

                g.DrawString(text, textFont, new SolidBrush(Color.FromArgb(80, 0, 0, 0)),
                    textX + 0.5f, textY + 0.5f);
                g.DrawString(text, textFont, Brushes.White, textX, textY);
            }
        }

        private GraphicsPath GetRoundedRect(Rectangle rect, int radius)
        {
            GraphicsPath path = new GraphicsPath();
            int diameter = radius * 2;

            path.AddArc(rect.X, rect.Y, diameter, diameter, 180, 90);
            path.AddArc(rect.Right - diameter, rect.Y, diameter, diameter, 270, 90);
            path.AddArc(rect.Right - diameter, rect.Bottom - diameter, diameter, diameter, 0, 90);
            path.AddArc(rect.X, rect.Bottom - diameter, diameter, diameter, 90, 90);
            path.CloseFigure();

            return path;
        }
    }

    /// <summary>
    /// 分隔线
    /// </summary>
    public class ToolbarSeparator : Control
    {
        public ToolbarSeparator()
        {
            this.Width = 1;
            this.Height = 24;
            this.BackColor = Color.Transparent;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            using (Pen pen = new Pen(Color.FromArgb(60, 150, 150, 150)))
            {
                e.Graphics.DrawLine(pen, 0, 4, 0, this.Height - 4);
            }
        }
    }
}
