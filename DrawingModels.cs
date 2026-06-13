using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;

namespace ModernScreenshot
{
    /// <summary>
    /// 操作历史管理器
    /// </summary>
    public class ActionHistoryManager
    {
        private readonly List<DrawAction> _actions;
        private readonly Stack<DrawAction> _redoStack;
        private readonly int _maxHistorySize;

        public ActionHistoryManager(int maxHistorySize = AppConstants.MAX_UNDO_STEPS)
        {
            _actions = new List<DrawAction>();
            _redoStack = new Stack<DrawAction>();
            _maxHistorySize = maxHistorySize;
        }

        /// <summary>
        /// 获取所有操作
        /// </summary>
        public IReadOnlyList<DrawAction> Actions => _actions.AsReadOnly();

        /// <summary>
        /// 是否可以撤销
        /// </summary>
        public bool CanUndo => _actions.Count > 0;

        /// <summary>
        /// 是否可以重做
        /// </summary>
        public bool CanRedo => _redoStack.Count > 0;

        /// <summary>
        /// 添加操作
        /// </summary>
        public void AddAction(DrawAction action)
        {
            _actions.Add(action);
            _redoStack.Clear();

            // 限制历史记录大小
            while (_actions.Count > _maxHistorySize)
            {
                _actions.RemoveAt(0);
            }
        }

        /// <summary>
        /// 撤销操作
        /// </summary>
        public bool Undo()
        {
            if (!CanUndo) return false;

            var action = _actions[_actions.Count - 1];
            _actions.RemoveAt(_actions.Count - 1);
            _redoStack.Push(action);

            return true;
        }

        /// <summary>
        /// 重做操作
        /// </summary>
        public bool Redo()
        {
            if (!CanRedo) return false;

            var action = _redoStack.Pop();
            _actions.Add(action);

            return true;
        }

        /// <summary>
        /// 清除所有历史
        /// </summary>
        public void Clear()
        {
            _actions.Clear();
            _redoStack.Clear();
        }
    }

    /// <summary>
    /// 绘图操作
    /// </summary>
    public class DrawAction
    {
        public ToolMode Tool { get; set; }
        public Color Color { get; set; }
        public int Width { get; set; }
        public List<Point> Points { get; set; } = new List<Point>();
        public string? Text { get; set; }

        /// <summary>
        /// 渲染此操作
        /// </summary>
        public void Render(Graphics g)
        {
            if (Points.Count == 0) return;

            using (var pen = new Pen(Color, Width))
            using (var brush = new SolidBrush(Color))
            {
                pen.StartCap = System.Drawing.Drawing2D.LineCap.Round;
                pen.EndCap = System.Drawing.Drawing2D.LineCap.Round;
                pen.LineJoin = System.Drawing.Drawing2D.LineJoin.Round;

                switch (Tool)
                {
                    case ToolMode.Pen:
                        if (Points.Count > 1)
                        {
                            g.DrawLines(pen, Points.ToArray());
                        }
                        break;

                    case ToolMode.Rectangle:
                        if (Points.Count >= 2)
                        {
                            var rect = RenderHelper.GetRectFromPoints(Points[0], Points[^1]);
                            g.DrawRectangle(pen, rect);
                        }
                        break;

                    case ToolMode.Arrow:
                        if (Points.Count >= 2)
                        {
                            RenderHelper.DrawArrow(g, pen, Points[0], Points[^1]);
                        }
                        break;

                    case ToolMode.Text:
                        if (Points.Count > 0 && !string.IsNullOrEmpty(Text))
                        {
                            using (var font = new Font(AppConstants.DEFAULT_FONT_FAMILY, AppConstants.DEFAULT_FONT_SIZE))
                            {
                                g.DrawString(Text, font, brush, Points[0]);
                            }
                        }
                        break;
                }
            }
        }
    }

    /// <summary>
    /// 工具模式
    /// </summary>
    public enum ToolMode
    {
        None,
        Rectangle,
        Arrow,
        Pen,
        Text,
        Mosaic
    }
}
