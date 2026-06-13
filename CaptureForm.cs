using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using System.Collections.Generic;
using System.Linq;

namespace ModernScreenshot
{
    public class CaptureForm : Form
    {
        private Bitmap? screenCapture;
        private Rectangle selectionRect;
        private Point startPoint;
        private Point currentPoint;
        private bool isSelecting;
        private bool isSelected;

        private Panel? toolbarPanel;
        private ToolMode currentTool = ToolMode.None;
        private Color drawColor = Color.Red;
        private int drawWidth = 3;

        private List<DrawAction> drawActions = new List<DrawAction>();
        private List<DrawAction> redoStack = new List<DrawAction>();
        private DrawAction? currentAction;

        public CaptureForm()
        {
            InitializeComponent();
            CaptureScreen();
        }

        private void InitializeComponent()
        {
            this.SuspendLayout();

            this.FormBorderStyle = FormBorderStyle.None;
            this.WindowState = FormWindowState.Maximized;
            this.TopMost = true;
            this.DoubleBuffered = true;
            this.Cursor = Cursors.Cross;
            this.BackColor = Color.Black;

            this.KeyPreview = true;
            this.KeyDown += CaptureForm_KeyDown;
            this.MouseDown += CaptureForm_MouseDown;
            this.MouseMove += CaptureForm_MouseMove;
            this.MouseUp += CaptureForm_MouseUp;
            this.Paint += CaptureForm_Paint;

            this.ResumeLayout(false);
        }

        private void CaptureScreen()
        {
            var bounds = Screen.PrimaryScreen?.Bounds ?? new Rectangle(0, 0, 1920, 1080);

            // 捕获所有屏幕
            int minX = Screen.AllScreens.Min(s => s.Bounds.X);
            int minY = Screen.AllScreens.Min(s => s.Bounds.Y);
            int maxX = Screen.AllScreens.Max(s => s.Bounds.Right);
            int maxY = Screen.AllScreens.Max(s => s.Bounds.Bottom);

            bounds = new Rectangle(minX, minY, maxX - minX, maxY - minY);

            screenCapture = new Bitmap(bounds.Width, bounds.Height);
            using (Graphics g = Graphics.FromImage(screenCapture))
            {
                g.CopyFromScreen(bounds.Location, Point.Empty, bounds.Size);
            }

            this.Bounds = bounds;
        }

        private void CaptureForm_KeyDown(object? sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Escape)
            {
                this.Close();
            }
            else if (e.Control && e.KeyCode == Keys.Z)
            {
                Undo();
            }
            else if (e.Control && e.KeyCode == Keys.Y)
            {
                Redo();
            }
            else if (e.Control && e.KeyCode == Keys.C)
            {
                CopyToClipboard();
            }
            else if (e.Control && e.KeyCode == Keys.S)
            {
                SaveToFile();
            }
        }

        private void CaptureForm_MouseDown(object? sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                if (!isSelected)
                {
                    // 开始选择区域
                    isSelecting = true;
                    startPoint = e.Location;
                    currentPoint = e.Location;
                }
                else if (selectionRect.Contains(e.Location))
                {
                    // 在选定区域内，开始绘制
                    if (currentTool == ToolMode.Text)
                    {
                        // 文字工具需要弹出输入框
                        var textDialog = new TextInputDialog();
                        if (textDialog.ShowDialog() == DialogResult.OK && !string.IsNullOrEmpty(textDialog.InputText))
                        {
                            var textAction = new DrawAction
                            {
                                Tool = ToolMode.Text,
                                Color = drawColor,
                                Width = drawWidth,
                                Points = new List<Point> { e.Location },
                                Text = textDialog.InputText
                            };
                            drawActions.Add(textAction);
                            redoStack.Clear();
                            this.Invalidate();
                        }
                    }
                    else if (currentTool != ToolMode.None)
                    {
                        currentAction = new DrawAction
                        {
                            Tool = currentTool,
                            Color = drawColor,
                            Width = drawWidth,
                            Points = new List<Point> { e.Location }
                        };
                    }
                }
            }
        }

        private void CaptureForm_MouseMove(object? sender, MouseEventArgs e)
        {
            if (isSelecting)
            {
                currentPoint = e.Location;
                this.Invalidate();
            }
            else if (currentAction != null)
            {
                currentAction.Points.Add(e.Location);
                this.Invalidate();
            }
        }

        private void CaptureForm_MouseUp(object? sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                if (isSelecting)
                {
                    isSelecting = false;
                    isSelected = true;

                    int x = Math.Min(startPoint.X, currentPoint.X);
                    int y = Math.Min(startPoint.Y, currentPoint.Y);
                    int width = Math.Abs(currentPoint.X - startPoint.X);
                    int height = Math.Abs(currentPoint.Y - startPoint.Y);

                    selectionRect = new Rectangle(x, y, width, height);

                    if (width > 10 && height > 10)
                    {
                        ShowToolbar();
                    }

                    this.Invalidate();
                }
                else if (currentAction != null)
                {
                    drawActions.Add(currentAction);
                    redoStack.Clear();
                    currentAction = null;
                    this.Invalidate();
                }
            }
        }

        private void CaptureForm_Paint(object? sender, PaintEventArgs e)
        {
            if (screenCapture == null) return;

            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

            // 绘制背景截图，选区外变暗
            if (!isSelected && !isSelecting)
            {
                e.Graphics.DrawImage(screenCapture, 0, 0);
                using (var darkBrush = new SolidBrush(Color.FromArgb(100, 0, 0, 0)))
                {
                    e.Graphics.FillRectangle(darkBrush, this.ClientRectangle);
                }
            }
            else
            {
                // 绘制暗化背景
                using (var darkBrush = new SolidBrush(Color.FromArgb(100, 0, 0, 0)))
                {
                    e.Graphics.DrawImage(screenCapture, 0, 0);
                    e.Graphics.FillRectangle(darkBrush, this.ClientRectangle);
                }

                // 计算当前选择矩形
                Rectangle currentRect = GetCurrentSelectionRect();

                // 绘制清晰的选区
                if (currentRect.Width > 0 && currentRect.Height > 0)
                {
                    e.Graphics.DrawImage(screenCapture, currentRect, currentRect, GraphicsUnit.Pixel);

                    // 绘制所有标注
                    foreach (var action in drawActions)
                    {
                        DrawAction(e.Graphics, action);
                    }

                    // 绘制当前标注
                    if (currentAction != null)
                    {
                        DrawAction(e.Graphics, currentAction);
                    }

                    // 绘制边框
                    using (var pen = new Pen(Color.DodgerBlue, 2))
                    {
                        e.Graphics.DrawRectangle(pen, currentRect);
                    }

                    // 绘制尺寸信息
                    DrawSizeInfo(e.Graphics, currentRect);
                }
            }
        }

        private Rectangle GetCurrentSelectionRect()
        {
            if (isSelecting)
            {
                int x = Math.Min(startPoint.X, currentPoint.X);
                int y = Math.Min(startPoint.Y, currentPoint.Y);
                int width = Math.Abs(currentPoint.X - startPoint.X);
                int height = Math.Abs(currentPoint.Y - startPoint.Y);
                return new Rectangle(x, y, width, height);
            }
            return selectionRect;
        }

        private void DrawSizeInfo(Graphics g, Rectangle rect)
        {
            string sizeText = $"{rect.Width} × {rect.Height}";
            using (var font = new Font("Segoe UI", 10))
            using (var brush = new SolidBrush(Color.White))
            using (var backBrush = new SolidBrush(Color.FromArgb(200, 0, 0, 0)))
            {
                var textSize = g.MeasureString(sizeText, font);
                var textRect = new RectangleF(rect.X, rect.Y - textSize.Height - 5, textSize.Width + 10, textSize.Height + 4);

                if (textRect.Y < 0)
                {
                    textRect.Y = rect.Y + 5;
                }

                g.FillRectangle(backBrush, textRect);
                g.DrawString(sizeText, font, brush, textRect.X + 5, textRect.Y + 2);
            }
        }

        private void DrawAction(Graphics g, DrawAction action)
        {
            if (action.Points.Count == 0) return;

            using (var pen = new Pen(action.Color, action.Width))
            using (var brush = new SolidBrush(action.Color))
            {
                pen.StartCap = LineCap.Round;
                pen.EndCap = LineCap.Round;
                pen.LineJoin = LineJoin.Round;

                switch (action.Tool)
                {
                    case ToolMode.Pen:
                        if (action.Points.Count > 1)
                        {
                            g.DrawLines(pen, action.Points.ToArray());
                        }
                        break;

                    case ToolMode.Rectangle:
                        if (action.Points.Count >= 2)
                        {
                            var rect = GetRectFromPoints(action.Points[0], action.Points[^1]);
                            g.DrawRectangle(pen, rect);
                        }
                        break;

                    case ToolMode.Arrow:
                        if (action.Points.Count >= 2)
                        {
                            DrawArrow(g, pen, action.Points[0], action.Points[^1]);
                        }
                        break;

                    case ToolMode.Text:
                        if (action.Points.Count > 0 && !string.IsNullOrEmpty(action.Text))
                        {
                            using (var font = new Font("Microsoft YaHei", 12))
                            {
                                g.DrawString(action.Text, font, brush, action.Points[0]);
                            }
                        }
                        break;

                    case ToolMode.Mosaic:
                        if (action.Points.Count >= 2)
                        {
                            ApplyMosaic(g, GetRectFromPoints(action.Points[0], action.Points[^1]));
                        }
                        break;
                }
            }
        }

        private void DrawArrow(Graphics g, Pen pen, Point start, Point end)
        {
            g.DrawLine(pen, start, end);

            double angle = Math.Atan2(end.Y - start.Y, end.X - start.X);
            int arrowSize = 15;

            Point arrow1 = new Point(
                end.X - (int)(arrowSize * Math.Cos(angle - Math.PI / 6)),
                end.Y - (int)(arrowSize * Math.Sin(angle - Math.PI / 6))
            );

            Point arrow2 = new Point(
                end.X - (int)(arrowSize * Math.Cos(angle + Math.PI / 6)),
                end.Y - (int)(arrowSize * Math.Sin(angle + Math.PI / 6))
            );

            using (var brush = new SolidBrush(pen.Color))
            {
                g.FillPolygon(brush, new Point[] { end, arrow1, arrow2 });
            }
        }

        private void ApplyMosaic(Graphics g, Rectangle rect)
        {
            if (screenCapture == null) return;

            int mosaicSize = 10;

            // 使用 LockBits 提高性能
            Rectangle clampedRect = Rectangle.Intersect(rect, new Rectangle(0, 0, screenCapture.Width, screenCapture.Height));
            if (clampedRect.Width <= 0 || clampedRect.Height <= 0) return;

            using (Bitmap tempBitmap = new Bitmap(clampedRect.Width, clampedRect.Height))
            {
                using (Graphics tempG = Graphics.FromImage(tempBitmap))
                {
                    tempG.DrawImage(screenCapture, new Rectangle(0, 0, clampedRect.Width, clampedRect.Height),
                        clampedRect, GraphicsUnit.Pixel);
                }

                for (int y = 0; y < clampedRect.Height; y += mosaicSize)
                {
                    for (int x = 0; x < clampedRect.Width; x += mosaicSize)
                    {
                        int blockWidth = Math.Min(mosaicSize, clampedRect.Width - x);
                        int blockHeight = Math.Min(mosaicSize, clampedRect.Height - y);

                        // 采样中心点颜色
                        int sampleX = Math.Min(x + mosaicSize / 2, clampedRect.Width - 1);
                        int sampleY = Math.Min(y + mosaicSize / 2, clampedRect.Height - 1);
                        Color pixelColor = tempBitmap.GetPixel(sampleX, sampleY);

                        using (var brush = new SolidBrush(pixelColor))
                        {
                            g.FillRectangle(brush, clampedRect.X + x, clampedRect.Y + y, blockWidth, blockHeight);
                        }
                    }
                }
            }
        }

        private Rectangle GetRectFromPoints(Point p1, Point p2)
        {
            int x = Math.Min(p1.X, p2.X);
            int y = Math.Min(p1.Y, p2.Y);
            int width = Math.Abs(p2.X - p1.X);
            int height = Math.Abs(p2.Y - p1.Y);
            return new Rectangle(x, y, width, height);
        }

        private void ShowToolbar()
        {
            if (toolbarPanel != null) return;

            // 使用 Pixpin 风格工具栏
            var toolbar = new PixpinToolbar
            {
                Size = new Size(660, 50),
                Location = new Point(selectionRect.X, selectionRect.Bottom + 10)
            };

            // 确保工具栏在屏幕内
            if (toolbar.Bottom > this.Height)
            {
                toolbar.Location = new Point(selectionRect.X, selectionRect.Y - 60);
            }
            if (toolbar.Right > this.Width)
            {
                toolbar.Location = new Point(this.Width - toolbar.Width - 10, toolbar.Location.Y);
            }

            int x = 10;
            int spacing = 4;

            // 工具按钮 - 只显示图标
            var rectBtn = CreateToolButton("□", "矩形", x, 7, ToolMode.Rectangle);
            toolbar.Controls.Add(rectBtn);
            x += rectBtn.Width + spacing;

            var arrowBtn = CreateToolButton("→", "箭头", x, 7, ToolMode.Arrow);
            toolbar.Controls.Add(arrowBtn);
            x += arrowBtn.Width + spacing;

            var penBtn = CreateToolButton("✎", "画笔", x, 7, ToolMode.Pen);
            toolbar.Controls.Add(penBtn);
            x += penBtn.Width + spacing;

            var textBtn = CreateToolButton("A", "文字", x, 7, ToolMode.Text);
            toolbar.Controls.Add(textBtn);
            x += textBtn.Width + spacing;

            var mosaicBtn = CreateToolButton("⊞", "马赛克", x, 7, ToolMode.Mosaic);
            toolbar.Controls.Add(mosaicBtn);
            x += mosaicBtn.Width + spacing;

            // 分隔线
            x += 6;
            var sep1 = new ToolbarSeparator { Location = new Point(x, 13) };
            toolbar.Controls.Add(sep1);
            x += sep1.Width + 6;

            // 撤销/重做
            var undoBtn = CreateActionButton("↶", "撤销", x, 7);
            undoBtn.ButtonClick += (s, e) => Undo();
            toolbar.Controls.Add(undoBtn);
            x += undoBtn.Width + spacing;

            var redoBtn = CreateActionButton("↷", "重做", x, 7);
            redoBtn.ButtonClick += (s, e) => Redo();
            toolbar.Controls.Add(redoBtn);
            x += redoBtn.Width + spacing;

            // 分隔线
            x += 6;
            var sep2 = new ToolbarSeparator { Location = new Point(x, 13) };
            toolbar.Controls.Add(sep2);
            x += sep2.Width + 6;

            // 保存按钮 - 主按钮，加宽突出
            var saveBtn = new PixpinButton
            {
                Location = new Point(x, 7),
                Size = new Size(80, 36),
                IconText = "💾",
                ButtonText = "保存",
                IsPrimary = true,
                IsIconOnly = false,
                Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Bold)
            };
            saveBtn.ButtonClick += (s, e) => SaveToFile();
            toolbar.Controls.Add(saveBtn);
            x += saveBtn.Width + spacing;

            // 复制按钮
            var copyBtn = CreateActionButton("📋", "复制", x, 7);
            copyBtn.ButtonClick += (s, e) => CopyToClipboard();
            toolbar.Controls.Add(copyBtn);
            x += copyBtn.Width + spacing;

            // 钉图按钮
            var pinBtn = CreateActionButton("📌", "钉图", x, 7);
            pinBtn.ButtonClick += (s, e) => PinImage();
            toolbar.Controls.Add(pinBtn);
            x += pinBtn.Width + spacing;

            // 分隔线
            x += 6;
            var sep3 = new ToolbarSeparator { Location = new Point(x, 13) };
            toolbar.Controls.Add(sep3);
            x += sep3.Width + 6;

            // 取消按钮
            var cancelBtn = CreateActionButton("✕", "取消", x, 7);
            cancelBtn.ButtonClick += (s, e) => this.Close();
            toolbar.Controls.Add(cancelBtn);

            toolbarPanel = toolbar;
            this.Controls.Add(toolbarPanel);
            toolbarPanel.BringToFront();
        }

        private PixpinButton CreateToolButton(string icon, string tooltip, int x, int y, ToolMode mode)
        {
            var btn = new PixpinButton
            {
                Location = new Point(x, y),
                Size = new Size(44, 36),
                IconText = icon,
                ButtonText = tooltip,
                IsIconOnly = true,
                Tag = mode
            };

            btn.ButtonClick += (s, e) =>
            {
                // 取消其他工具按钮的选中状态
                if (toolbarPanel != null)
                {
                    foreach (Control ctrl in toolbarPanel.Controls)
                    {
                        if (ctrl is PixpinButton pb && pb != btn && pb.Tag is ToolMode)
                        {
                            pb.Selected = false;
                        }
                    }
                }

                // 切换当前工具
                if (currentTool == mode)
                {
                    currentTool = ToolMode.None;
                    btn.Selected = false;
                    this.Cursor = Cursors.Default;
                }
                else
                {
                    currentTool = mode;
                    btn.Selected = true;
                    this.Cursor = Cursors.Cross;
                }
            };

            return btn;
        }

        private PixpinButton CreateActionButton(string icon, string text, int x, int y)
        {
            return new PixpinButton
            {
                Location = new Point(x, y),
                Size = new Size(68, 36),
                IconText = icon,
                ButtonText = text,
                IsIconOnly = false
            };
        }

        private Color GetContrastColor(Color color)
        {
            // 计算颜色亮度，选择黑色或白色作为对比色
            double luminance = 0.299 * color.R + 0.587 * color.G + 0.114 * color.B;
            return luminance > 128 ? Color.Black : Color.White;
        }

        private void Undo()
        {
            if (drawActions.Count > 0)
            {
                var action = drawActions[^1];
                drawActions.RemoveAt(drawActions.Count - 1);
                redoStack.Add(action);
                this.Invalidate();
            }
        }

        private void Redo()
        {
            if (redoStack.Count > 0)
            {
                var action = redoStack[^1];
                redoStack.RemoveAt(redoStack.Count - 1);
                drawActions.Add(action);
                this.Invalidate();
            }
        }

        private void CopyToClipboard()
        {
            if (screenCapture == null || selectionRect.IsEmpty) return;

            var croppedImage = GetFinalImage();
            Clipboard.SetImage(croppedImage);

            ShowNotification("已复制到剪贴板");
            this.Close();
        }

        private void SaveToFile()
        {
            if (screenCapture == null || selectionRect.IsEmpty) return;

            var saveDialog = new SaveFileDialog
            {
                Filter = "PNG图片|*.png|JPEG图片|*.jpg",
                FileName = $"截图_{DateTime.Now:yyyyMMdd_HHmmss}.png",
                InitialDirectory = PathManager.ScreenshotsDirectory
            };

            if (saveDialog.ShowDialog() == DialogResult.OK)
            {
                var croppedImage = GetFinalImage();
                croppedImage.Save(saveDialog.FileName);
                ShowNotification($"已保存到: {saveDialog.FileName}");
                this.Close();
            }
        }

        private void PinImage()
        {
            if (screenCapture == null || selectionRect.IsEmpty) return;

            var pinnedImage = GetFinalImage();
            var pinForm = new PinForm(pinnedImage);
            pinForm.Show();
            this.Close();
        }

        private Bitmap GetFinalImage()
        {
            if (screenCapture == null) throw new InvalidOperationException();

            var result = new Bitmap(selectionRect.Width, selectionRect.Height);
            using (var g = Graphics.FromImage(result))
            {
                g.SmoothingMode = SmoothingMode.AntiAlias;

                // 绘制选区内容
                g.DrawImage(screenCapture,
                    new Rectangle(0, 0, selectionRect.Width, selectionRect.Height),
                    selectionRect,
                    GraphicsUnit.Pixel);

                // 绘制标注（转换坐标）
                g.TranslateTransform(-selectionRect.X, -selectionRect.Y);
                foreach (var action in drawActions)
                {
                    DrawAction(g, action);
                }
            }

            return result;
        }

        private void ShowNotification(string message)
        {
            // 简单的通知实现
            var notifyForm = new Form
            {
                FormBorderStyle = FormBorderStyle.None,
                StartPosition = FormStartPosition.Manual,
                Size = new Size(200, 60),
                BackColor = Color.FromArgb(50, 50, 50),
                TopMost = true,
                ShowInTaskbar = false
            };

            var label = new Label
            {
                Text = message,
                ForeColor = Color.White,
                Font = new Font("Microsoft YaHei", 12),
                TextAlign = ContentAlignment.MiddleCenter,
                Dock = DockStyle.Fill
            };

            notifyForm.Controls.Add(label);
            notifyForm.Location = new Point(
                (Screen.PrimaryScreen?.WorkingArea.Width ?? 1920) / 2 - 100,
                (Screen.PrimaryScreen?.WorkingArea.Height ?? 1080) - 100
            );

            notifyForm.Show();

            var timer = new Timer { Interval = 1500 };
            timer.Tick += (s, e) =>
            {
                timer.Stop();
                notifyForm.Close();
            };
            timer.Start();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                screenCapture?.Dispose();
                toolbarPanel?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
