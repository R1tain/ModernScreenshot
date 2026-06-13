using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace ModernScreenshot
{
    public partial class MainForm : Form
    {
        private NotifyIcon? trayIcon;

        [DllImport("user32.dll")]
        private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

        [DllImport("user32.dll")]
        private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

        private const int HOTKEY_ID = 1;
        private const uint MOD_CONTROL = 0x0002;
        private const uint VK_1 = 0x31;

        public MainForm()
        {
            InitializeComponent();
            InitializeTrayIcon();
            RegisterHotKey(this.Handle, HOTKEY_ID, MOD_CONTROL, VK_1);

            // 隐藏主窗体
            this.WindowState = FormWindowState.Minimized;
            this.ShowInTaskbar = false;
            this.Opacity = 0;
        }

        private void InitializeTrayIcon()
        {
            trayIcon = new NotifyIcon
            {
                Text = "ModernScreenshot (Ctrl+1)",
                Visible = true
            };

            // 创建简单的图标
            using (Bitmap bmp = new Bitmap(16, 16))
            using (Graphics g = Graphics.FromImage(bmp))
            {
                g.Clear(Color.Transparent);
                g.FillRectangle(Brushes.DodgerBlue, 2, 2, 12, 12);
                g.DrawRectangle(Pens.White, 3, 3, 10, 10);
                trayIcon.Icon = Icon.FromHandle(bmp.GetHicon());
            }

            var contextMenu = new ContextMenuStrip();
            contextMenu.Items.Add("截图 (F1)", null, (s, e) => StartCapture());
            contextMenu.Items.Add("-");
            contextMenu.Items.Add("退出", null, (s, e) => Application.Exit());
            trayIcon.ContextMenuStrip = contextMenu;

            trayIcon.DoubleClick += (s, e) => StartCapture();
        }

        protected override void WndProc(ref Message m)
        {
            const int WM_HOTKEY = 0x0312;

            if (m.Msg == WM_HOTKEY && m.WParam.ToInt32() == HOTKEY_ID)
            {
                StartCapture();
            }

            base.WndProc(ref m);
        }

        private void StartCapture()
        {
            var captureForm = new CaptureForm();
            captureForm.ShowDialog();
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            UnregisterHotKey(this.Handle, HOTKEY_ID);
            trayIcon?.Dispose();
            base.OnFormClosing(e);
        }

        private void InitializeComponent()
        {
            this.SuspendLayout();
            this.AutoScaleDimensions = new SizeF(7F, 15F);
            this.AutoScaleMode = AutoScaleMode.Font;
            this.ClientSize = new Size(1, 1);
            this.FormBorderStyle = FormBorderStyle.None;
            this.Name = "MainForm";
            this.StartPosition = FormStartPosition.Manual;
            this.Location = new Point(-10000, -10000);
            this.ResumeLayout(false);
        }
    }
}
