using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace ModernScreenshot
{
    /// <summary>
    /// 深色工具栏主题
    /// </summary>
    public class DarkColorTable : ProfessionalColorTable
    {
        public override Color ToolStripDropDownBackground => Color.FromArgb(50, 50, 50);
        public override Color ImageMarginGradientBegin => Color.FromArgb(50, 50, 50);
        public override Color ImageMarginGradientMiddle => Color.FromArgb(50, 50, 50);
        public override Color ImageMarginGradientEnd => Color.FromArgb(50, 50, 50);
        public override Color MenuBorder => Color.FromArgb(80, 80, 80);
        public override Color MenuItemBorder => Color.DodgerBlue;
        public override Color MenuItemSelected => Color.FromArgb(70, 70, 70);
        public override Color MenuStripGradientBegin => Color.FromArgb(50, 50, 50);
        public override Color MenuStripGradientEnd => Color.FromArgb(50, 50, 50);
        public override Color MenuItemSelectedGradientBegin => Color.FromArgb(70, 70, 70);
        public override Color MenuItemSelectedGradientEnd => Color.FromArgb(70, 70, 70);
        public override Color MenuItemPressedGradientBegin => Color.FromArgb(60, 60, 60);
        public override Color MenuItemPressedGradientEnd => Color.FromArgb(60, 60, 60);
    }
}
