// NX2412 journal worker. This file is launched by run_journal.exe, not by the UI process.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using NXOpen;

public class NxPreviewWorker
{
    public static int Main(string[] args)
    {
        if (args == null || args.Length < 3) return 2;
        string listPath = args[0];
        string progressPath = args[1];
        bool backup = args[2] == "1";
        int failures = 0;
        foreach (string raw in File.ReadAllLines(listPath, Encoding.UTF8))
        {
            string path = raw.Trim();
            if (path.Length == 0) continue;
            try
            {
                Write(progressPath, "处理中", path, "NX 正在打开零件");
                Repair(path, backup);
                Write(progressPath, "完成", path, "正等轴测预览图已重建");
            }
            catch (Exception ex)
            {
                failures++;
                Write(progressPath, "失败", path, Clean(ex.Message));
            }
        }
        return failures == 0 ? 0 : 1;
    }

    private static void Repair(string path, bool backup)
    {
        if (!File.Exists(path)) throw new FileNotFoundException("零件文件不存在", path);
        if (backup)
        {
            string backupPath = path + ".bak";
            if (!File.Exists(backupPath)) File.Copy(path, backupPath, false);
        }

        Session session = Session.GetSession();
        PartLoadStatus loadStatus = null;
        BasePart part = null;
        try
        {
            part = session.Parts.OpenBaseDisplay(path, out loadStatus);
            if (loadStatus != null && loadStatus.NumberUnloadedParts > 0)
            {
                var details = new StringBuilder("零件未完整加载");
                for (int i = 0; i < loadStatus.NumberUnloadedParts; i++)
                    details.Append("; ").Append(loadStatus.GetPartName(i)).Append(": ").Append(loadStatus.GetStatusDescription(i));
                throw new InvalidOperationException(details.ToString());
            }

            ModelingView view = part.ModelingViews.WorkView;
            view.Orient(View.Canned.Isometric, View.ScaleAdjustment.Fit);
            view.Fit();
            view.Regenerate();
            view.UpdateDisplay();
            part.PartPreviewMode = BasePart.PartPreview.OnSave;
            PartSaveStatus saveStatus = part.Save(BasePart.SaveComponents.False, BasePart.CloseAfterSave.False);
            try
            {
                if (saveStatus != null && saveStatus.NumberUnsavedParts > 0)
                    throw new InvalidOperationException("保存失败，未保存零件数：" + saveStatus.NumberUnsavedParts);
            }
            finally { if (saveStatus != null) saveStatus.Dispose(); }
        }
        finally
        {
            if (loadStatus != null) loadStatus.Dispose();
            if (part != null)
            {
                try { part.Close(BasePart.CloseWholeTree.False, BasePart.CloseModified.CloseModified, null); }
                catch { }
            }
        }
    }

    private static void Write(string progressPath, string status, string path, string message)
    {
        using (var writer = new StreamWriter(progressPath, true, new UTF8Encoding(false)))
        {
            writer.Write(status); writer.Write('\t'); writer.Write(path); writer.Write('\t'); writer.WriteLine(Clean(message));
        }
    }

    private static string Clean(string value)
    {
        return (value ?? "未知错误").Replace('\r', ' ').Replace('\n', ' ').Replace('\t', ' ');
    }

    public static int GetUnloadOption(string dummy) { return (int)Session.LibraryUnloadOption.Immediately; }
}
