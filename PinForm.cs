using System;
using System.Drawing;
using System.Windows.Forms;

namespace ModernScreenshot
{
    public class PinForm : Form
    {
        private Bitmap pinnedImage;
        private bool isDragging;
        private Point dragStartPoint;
        private double zoomFactor = 1.0;

        public PinForm(Bitmap image)
        {
            pinnedImage = image;
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            this.SuspendLayout();

            this.FormBorderStyle = FormBorderStyle.None;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.TopMost = true;
            this.ShowInTaskbar = false;
            this.DoubleBuffered = true;
            this.BackColor = Color.White;

            this.ClientSize = pinnedImage.Size;

            this.MouseDown += PinForm_MouseDown;
            this.MouseMove += PinForm_MouseMove;
            this.MouseUp += PinForm_MouseUp;
            this.MouseWheel += PinForm_MouseWheel;
            this.Paint += PinForm_Paint;
            this.KeyDown += PinForm_KeyDown;

            // 右键菜单
            var contextMenu = new ContextMenuStrip();
            contextMenu.Items.Add("复制", null, (s, e) => Clipboard.SetImage(pinnedImage));
            contextMenu.Items.Add("另存为...", null, (s, e) => SaveImage());
            contextMenu.Items.Add("-");
            contextMenu.Items.Add("关闭", null, (s, e) => this.Close());
            this.ContextMenuStrip = contextMenu;

            this.ResumeLayout(false);
        }

        private void PinForm_Paint(object? sender, PaintEventArgs e)
        {
            e.Graphics.DrawImage(pinnedImage, 0, 0, this.ClientSize.Width, this.ClientSize.Height);

            // 绘制边框
            using (var pen = new Pen(Color.DodgerBlue, 2))
            {
                e.Graphics.DrawRectangle(pen, 0, 0, this.ClientSize.Width - 1, this.ClientSize.Height - 1);
            }
        }

        private void PinForm_MouseDown(object? sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                isDragging = true;
                dragStartPoint = e.Location;
                this.Cursor = Cursors.SizeAll;
            }
        }

        private void PinForm_MouseMove(object? sender, MouseEventArgs e)
        {
            if (isDragging)
            {
                Point newLocation = this.Location;
                newLocation.X += e.X - dragStartPoint.X;
                newLocation.Y += e.Y - dragStartPoint.Y;
                this.Location = newLocation;
            }
        }

        private void PinForm_MouseUp(object? sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                isDragging = false;
                this.Cursor = Cursors.Default;
            }
        }

        private void PinForm_MouseWheel(object? sender, MouseEventArgs e)
        {
            // 滚轮缩放
            double delta = e.Delta > 0 ? 1.1 : 0.9;
            zoomFactor *= delta;

            int newWidth = (int)(pinnedImage.Width * zoomFactor);
            int newHeight = (int)(pinnedImage.Height * zoomFactor);

            if (newWidth > 50 && newHeight > 50 && newWidth < 5000 && newHeight < 5000)
            {
                this.ClientSize = new Size(newWidth, newHeight);
                this.Invalidate();
            }
        }

        private void PinForm_KeyDown(object? sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Escape)
            {
                this.Close();
            }
        }

        private void SaveImage()
        {
            var saveDialog = new SaveFileDialog
            {
                Filter = "PNG图片|*.png|JPEG图片|*.jpg",
                FileName = $"钉图_{DateTime.Now:yyyyMMdd_HHmmss}.png"
            };

            if (saveDialog.ShowDialog() == DialogResult.OK)
            {
                pinnedImage.Save(saveDialog.FileName);
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                pinnedImage?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
