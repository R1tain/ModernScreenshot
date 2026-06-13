using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using System.Collections.Generic;

namespace ModernScreenshot
{
    /// <summary>
    /// 截图捕获窗体（重构版）
    /// </summary>
    public class CaptureFormRefactored : Form
    {
        // 核心数据
        private Bitmap? _screenCapture;
        private Rectangle _selectionRect;
        private Point _startPoint;
        private Point _currentPoint;
        private bool _isSelecting;
        private bool _isSelected;

        // 工具栏
        private Panel? _toolbarPanel;
        private ToolStripButton? _currentToolButton;

        // 绘图状态
        private ToolMode _currentTool = ToolMode.None;
        private Color _drawColor = AppConstants.DEFAULT_DRAW_COLOR;
        private int _drawWidth = AppConstants.DEFAULT_DRAW_WIDTH;

        // 历史管理
        private readonly ActionHistoryManager _historyManager;
        private DrawAction? _currentAction;

        public CaptureFormRefactored()
        {
            _historyManager = new ActionHistoryManager();
            InitializeComponent();
            CaptureScreen();
        }

        private void InitializeComponent()
        {
            this.SuspendLayout();

            // 窗体配置
            this.FormBorderStyle = FormBorderStyle.None;
            this.WindowState = FormWindowState.Maximized;
            this.TopMost = true;
            this.DoubleBuffered = true;
            this.Cursor = Cursors.Cross;
            this.BackColor = Color.Black;
            this.KeyPreview = true;

            // 事件订阅
            this.KeyDown += OnKeyDown;
            this.MouseDown += OnMouseDown;
            this.MouseMove += OnMouseMove;
            this.MouseUp += OnMouseUp;
            this.Paint += OnPaint;

            this.ResumeLayout(false);
        }

        private void CaptureScreen()
        {
            var (screenshot, bounds) = ScreenCaptureHelper.CaptureAllScreens();
            _screenCapture = screenshot;
            this.Bounds = bounds;
        }

        #region 事件处理

        private void OnKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Escape)
            {
                this.Close();
            }
            else if (e.Control && e.KeyCode == Keys.Z)
            {
                if (_historyManager.Undo())
                {
                    this.Invalidate();
                }
            }
            else if (e.Control && e.KeyCode == Keys.Y)
            {
                if (_historyManager.Redo())
                {
                    this.Invalidate();
                }
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

        private void OnMouseDown(object? sender, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left) return;

            if (!_isSelected)
            {
                // 开始选择区域
                _isSelecting = true;
                _startPoint = e.Location;
                _currentPoint = e.Location;
            }
            else if (_selectionRect.Contains(e.Location))
            {
                // 在选定区域内绘制
                StartDrawing(e.Location);
            }
        }

        private void OnMouseMove(object? sender, MouseEventArgs e)
        {
            if (_isSelecting)
            {
                _currentPoint = e.Location;
                this.Invalidate();
            }
            else if (_currentAction != null)
            {
                _currentAction.Points.Add(e.Location);
                this.Invalidate();
            }
        }

        private void OnMouseUp(object? sender, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left) return;

            if (_isSelecting)
            {
                FinishSelection();
            }
            else if (_currentAction != null)
            {
                FinishDrawing();
            }
        }

        private void OnPaint(object? sender, PaintEventArgs e)
        {
            if (_screenCapture == null) return;

            RenderHelper.ConfigureHighQualityRendering(e.Graphics);

            if (!_isSelected && !_isSelecting)
            {
                // 初始状态
                RenderInitialState(e.Graphics);
            }
            else
            {
                // 选择状态
                RenderSelectionState(e.Graphics);
            }
        }

        #endregion

        #region 渲染逻辑

        private void RenderInitialState(Graphics g)
        {
            g.DrawImage(_screenCapture!, 0, 0);
            using (var darkBrush = new SolidBrush(AppConstants.OVERLAY_COLOR))
            {
                g.FillRectangle(darkBrush, this.ClientRectangle);
            }
        }

        private void RenderSelectionState(Graphics g)
        {
            // 绘制暗化背景
            g.DrawImage(_screenCapture!, 0, 0);
            using (var darkBrush = new SolidBrush(AppConstants.OVERLAY_COLOR))
            {
                g.FillRectangle(darkBrush, this.ClientRectangle);
            }

            // 获取当前选择矩形
            var currentRect = GetCurrentSelectionRect();

            if (currentRect.Width > 0 && currentRect.Height > 0)
            {
                // 绘制清晰的选区
                g.DrawImage(_screenCapture!, currentRect, currentRect, GraphicsUnit.Pixel);

                // 绘制所有标注
                foreach (var action in _historyManager.Actions)
                {
                    if (action.Tool == ToolMode.Mosaic)
                    {
                        RenderMosaicAction(g, action);
                    }
                    else
                    {
                        action.Render(g);
                    }
                }

                // 绘制当前操作
                if (_currentAction != null)
                {
                    if (_currentAction.Tool == ToolMode.Mosaic)
                    {
                        RenderMosaicAction(g, _currentAction);
                    }
                    else
                    {
                        _currentAction.Render(g);
                    }
                }

                // 绘制边框
                using (var pen = new Pen(AppConstants.SELECTION_BORDER_COLOR, 2))
                {
                    g.DrawRectangle(pen, currentRect);
                }

                // 绘制尺寸信息
                DrawSizeInfo(g, currentRect);
            }
        }

        private void RenderMosaicAction(Graphics g, DrawAction action)
        {
            if (action.Points.Count >= 2 && _screenCapture != null)
            {
                var rect = RenderHelper.GetRectFromPoints(action.Points[0], action.Points[^1]);
                RenderHelper.ApplyMosaic(g, _screenCapture, rect);
            }
        }

        private void DrawSizeInfo(Graphics g, Rectangle rect)
        {
            string sizeText = $"{rect.Width} × {rect.Height}";
            using (var font = new Font("Segoe UI", 10))
            using (var brush = new SolidBrush(Color.White))
            using (var backBrush = new SolidBrush(Color.FromArgb(200, 0, 0, 0)))
            {
                var textSize = g.MeasureString(sizeText, font);
                var textRect = new RectangleF(
                    rect.X,
                    rect.Y - textSize.Height - 5,
                    textSize.Width + 10,
                    textSize.Height + 4
                );

                if (textRect.Y < 0)
                {
                    textRect.Y = rect.Y + 5;
                }

                g.FillRectangle(backBrush, textRect);
                g.DrawString(sizeText, font, brush, textRect.X + 5, textRect.Y + 2);
            }
        }

        #endregion

        #region 绘图逻辑

        private void StartDrawing(Point location)
        {
            if (_currentTool == ToolMode.Text)
            {
                HandleTextInput(location);
            }
            else if (_currentTool != ToolMode.None)
            {
                _currentAction = new DrawAction
                {
                    Tool = _currentTool,
                    Color = _drawColor,
                    Width = _drawWidth,
                    Points = new List<Point> { location }
                };
            }
        }

        private void FinishDrawing()
        {
            if (_currentAction != null)
            {
                _historyManager.AddAction(_currentAction);
                _currentAction = null;
                this.Invalidate();
            }
        }

        private void HandleTextInput(Point location)
        {
            using (var textDialog = new TextInputDialog())
            {
                if (textDialog.ShowDialog() == DialogResult.OK && !string.IsNullOrEmpty(textDialog.InputText))
                {
                    var textAction = new DrawAction
                    {
                        Tool = ToolMode.Text,
                        Color = _drawColor,
                        Width = _drawWidth,
                        Points = new List<Point> { location },
                        Text = textDialog.InputText
                    };
                    _historyManager.AddAction(textAction);
                    this.Invalidate();
                }
            }
        }

        private void FinishSelection()
        {
            _isSelecting = false;
            _isSelected = true;

            _selectionRect = RenderHelper.GetRectFromPoints(_startPoint, _currentPoint);

            if (_selectionRect.Width > 10 && _selectionRect.Height > 10)
            {
                ShowToolbar();
            }

            this.Invalidate();
        }

        private Rectangle GetCurrentSelectionRect()
        {
            if (_isSelecting)
            {
                return RenderHelper.GetRectFromPoints(_startPoint, _currentPoint);
            }
            return _selectionRect;
        }

        #endregion

        #region 工具栏

        private void ShowToolbar()
        {
            if (_toolbarPanel != null) return;

            _toolbarPanel = new Panel
            {
                BackColor = AppConstants.TOOLBAR_BACKGROUND,
                Size = new Size(AppConstants.TOOLBAR_WIDTH, AppConstants.TOOLBAR_HEIGHT),
                Location = CalculateToolbarPosition()
            };

            var toolbar = CreateToolbar();
            _toolbarPanel.Controls.Add(toolbar);
            this.Controls.Add(_toolbarPanel);
            _toolbarPanel.BringToFront();
        }

        private Point CalculateToolbarPosition()
        {
            int x = _selectionRect.X;
            int y = _selectionRect.Bottom + AppConstants.TOOLBAR_MARGIN;

            // 确保工具栏在屏幕内
            if (y + AppConstants.TOOLBAR_HEIGHT > this.Height)
            {
                y = _selectionRect.Y - AppConstants.TOOLBAR_HEIGHT - AppConstants.TOOLBAR_MARGIN;
            }

            return new Point(x, y);
        }

        private ToolStrip CreateToolbar()
        {
            var toolbar = new ToolStrip
            {
                BackColor = Color.Transparent,
                Renderer = new ToolStripProfessionalRenderer(new DarkColorTable()),
                GripStyle = ToolStripGripStyle.Hidden,
                Dock = DockStyle.Fill
            };

            // 工具按钮
            AddToolButton(toolbar, "矩形", ToolMode.Rectangle);
            AddToolButton(toolbar, "箭头", ToolMode.Arrow);
            AddToolButton(toolbar, "画笔", ToolMode.Pen);
            AddToolButton(toolbar, "文字", ToolMode.Text);
            AddToolButton(toolbar, "马赛克", ToolMode.Mosaic);

            toolbar.Items.Add(new ToolStripSeparator());

            // 颜色选择
            AddColorSelector(toolbar);

            // 线宽选择
            AddWidthSelector(toolbar);

            toolbar.Items.Add(new ToolStripSeparator());

            // 编辑按钮
            AddButton(toolbar, "撤销", (s, e) => { if (_historyManager.Undo()) this.Invalidate(); });
            AddButton(toolbar, "重做", (s, e) => { if (_historyManager.Redo()) this.Invalidate(); });

            toolbar.Items.Add(new ToolStripSeparator());

            // 操作按钮
            AddButton(toolbar, "保存", (s, e) => SaveToFile());
            AddButton(toolbar, "复制", (s, e) => CopyToClipboard());
            AddButton(toolbar, "钉图", (s, e) => PinImage());
            AddButton(toolbar, "取消", (s, e) => this.Close());

            return toolbar;
        }

        private void AddToolButton(ToolStrip toolbar, string text, ToolMode mode)
        {
            var btn = new ToolStripButton(text)
            {
                CheckOnClick = true
            };

            btn.Click += (s, e) =>
            {
                if (_currentToolButton != null && _currentToolButton != btn)
                {
                    _currentToolButton.Checked = false;
                }
                _currentToolButton = btn.Checked ? btn : null;
                _currentTool = btn.Checked ? mode : ToolMode.None;
                this.Cursor = btn.Checked ? Cursors.Cross : Cursors.Default;
            };

            toolbar.Items.Add(btn);
        }

        private void AddColorSelector(ToolStrip toolbar)
        {
            var colorBtn = new ToolStripDropDownButton("颜色")
            {
                BackColor = _drawColor,
                ForeColor = RenderHelper.GetContrastColor(_drawColor)
            };

            foreach (var (color, name) in AppConstants.PRESET_COLORS)
            {
                var item = new ToolStripMenuItem(name)
                {
                    BackColor = color,
                    ForeColor = RenderHelper.GetContrastColor(color)
                };
                item.Click += (s, e) =>
                {
                    _drawColor = color;
                    colorBtn.BackColor = color;
                    colorBtn.ForeColor = RenderHelper.GetContrastColor(color);
                };
                colorBtn.DropDownItems.Add(item);
            }

            // 自定义颜色
            colorBtn.DropDownItems.Add(new ToolStripSeparator());
            var customItem = new ToolStripMenuItem("自定义...");
            customItem.Click += (s, e) =>
            {
                using (var colorDialog = new ColorDialog { Color = _drawColor })
                {
                    if (colorDialog.ShowDialog() == DialogResult.OK)
                    {
                        _drawColor = colorDialog.Color;
                        colorBtn.BackColor = _drawColor;
                        colorBtn.ForeColor = RenderHelper.GetContrastColor(_drawColor);
                    }
                }
            };
            colorBtn.DropDownItems.Add(customItem);

            toolbar.Items.Add(colorBtn);
        }

        private void AddWidthSelector(ToolStrip toolbar)
        {
            var widthBtn = new ToolStripDropDownButton($"线宽 {_drawWidth}");

            foreach (var width in AppConstants.PRESET_WIDTHS)
            {
                var item = new ToolStripMenuItem($"{width} 像素");
                item.Click += (s, e) =>
                {
                    _drawWidth = width;
                    widthBtn.Text = $"线宽 {width}";
                };
                widthBtn.DropDownItems.Add(item);
            }

            toolbar.Items.Add(widthBtn);
        }

        private void AddButton(ToolStrip toolbar, string text, EventHandler onClick)
        {
            var btn = new ToolStripButton(text);
            btn.Click += onClick;
            toolbar.Items.Add(btn);
        }

        #endregion

        #region 输出功能

        private void CopyToClipboard()
        {
            if (_screenCapture == null || _selectionRect.IsEmpty) return;

            var finalImage = GetFinalImage();
            Clipboard.SetImage(finalImage);
            ShowNotification("已复制到剪贴板");
            this.Close();
        }

        private void SaveToFile()
        {
            if (_screenCapture == null || _selectionRect.IsEmpty) return;

            var saveDialog = new SaveFileDialog
            {
                Filter = "PNG图片|*.png|JPEG图片|*.jpg",
                FileName = $"截图_{DateTime.Now:yyyyMMdd_HHmmss}.png",
                InitialDirectory = PathManager.ScreenshotsDirectory
            };

            if (saveDialog.ShowDialog() == DialogResult.OK)
            {
                var finalImage = GetFinalImage();
                finalImage.Save(saveDialog.FileName);
                ShowNotification($"已保存到: {saveDialog.FileName}");
                this.Close();
            }
        }

        private void PinImage()
        {
            if (_screenCapture == null || _selectionRect.IsEmpty) return;

            var pinnedImage = GetFinalImage();
            var pinForm = new PinForm(pinnedImage);
            pinForm.Show();
            this.Close();
        }

        private Bitmap GetFinalImage()
        {
            if (_screenCapture == null)
                throw new InvalidOperationException("No screen capture available");

            return ScreenCaptureHelper.CropAndRender(_screenCapture, _selectionRect, g =>
            {
                foreach (var action in _historyManager.Actions)
                {
                    if (action.Tool == ToolMode.Mosaic)
                    {
                        RenderMosaicAction(g, action);
                    }
                    else
                    {
                        action.Render(g);
                    }
                }
            });
        }

        private void ShowNotification(string message)
        {
            var notifyForm = new Form
            {
                FormBorderStyle = FormBorderStyle.None,
                StartPosition = FormStartPosition.Manual,
                Size = new Size(AppConstants.NOTIFICATION_WIDTH, AppConstants.NOTIFICATION_HEIGHT),
                BackColor = Color.FromArgb(50, 50, 50),
                TopMost = true,
                ShowInTaskbar = false
            };

            var label = new Label
            {
                Text = message,
                ForeColor = Color.White,
                Font = new Font(AppConstants.DEFAULT_FONT_FAMILY, 12),
                TextAlign = ContentAlignment.MiddleCenter,
                Dock = DockStyle.Fill
            };

            notifyForm.Controls.Add(label);
            notifyForm.Location = new Point(
                (Screen.PrimaryScreen?.WorkingArea.Width ?? 1920) / 2 - AppConstants.NOTIFICATION_WIDTH / 2,
                (Screen.PrimaryScreen?.WorkingArea.Height ?? 1080) - 100
            );

            notifyForm.Show();

            var timer = new Timer { Interval = AppConstants.NOTIFICATION_DURATION };
            timer.Tick += (s, e) =>
            {
                timer.Stop();
                notifyForm.Close();
            };
            timer.Start();
        }

        #endregion

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _screenCapture?.Dispose();
                _toolbarPanel?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
