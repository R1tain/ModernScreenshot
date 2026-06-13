using System;
using System.Drawing;
using System.Windows.Forms;

namespace ModernScreenshot
{
    public class TextInputDialog : Form
    {
        private TextBox? textBox;
        private Button? okButton;
        private Button? cancelButton;

        public string InputText { get; private set; } = "";

        public TextInputDialog()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            this.SuspendLayout();

            this.Text = "输入文字";
            this.Size = new Size(400, 150);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.TopMost = true;

            var label = new Label
            {
                Text = "请输入要添加的文字：",
                Location = new Point(15, 15),
                AutoSize = true
            };

            textBox = new TextBox
            {
                Location = new Point(15, 40),
                Size = new Size(355, 25),
                Font = new Font("Microsoft YaHei", 10)
            };

            okButton = new Button
            {
                Text = "确定",
                Location = new Point(210, 75),
                Size = new Size(80, 30),
                DialogResult = DialogResult.OK
            };

            cancelButton = new Button
            {
                Text = "取消",
                Location = new Point(295, 75),
                Size = new Size(80, 30),
                DialogResult = DialogResult.Cancel
            };

            okButton.Click += (s, e) =>
            {
                InputText = textBox?.Text ?? "";
                this.Close();
            };

            this.Controls.AddRange(new Control[] { label, textBox, okButton, cancelButton });
            this.AcceptButton = okButton;
            this.CancelButton = cancelButton;

            this.ResumeLayout(false);
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            textBox?.Focus();
        }
    }
}
