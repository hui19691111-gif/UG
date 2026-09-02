using System;
using System.Collections;
using System.IO;
using NXOpen;
using NXOpen.Annotations;
using NXOpen.UF;

public class NXJournal
{
    private const string PluginRoot = "D:\\UG\u667A\u8F89\u94A3\u91D1\u63D2\u4EF6";
    private const string QuantityTitle = "\u6570\u91CF";

    private static readonly string LogPath = Path.Combine(
        PluginRoot,
        "logs",
        "template_quantity_reference_restore.log");

    private static readonly string BackupRoot = Path.Combine(
        PluginRoot,
        "backup",
        "template_quantity_reference_restore_" + DateTime.Now.ToString("yyyyMMdd_HHmmss"));

    private static readonly string[] TemplatePaths =
    {
        Path.Combine(PluginRoot, "DATA", "A4-noviews-template.prt"),
        Path.Combine(PluginRoot, "DATA", "A4-noviews-template-.prt"),
        Path.Combine(PluginRoot, "DATA", "A4-noviews-template1.prt"),
        Path.Combine(PluginRoot, "DATA", "A4-noviews-template2.prt")
    };

    public static void Main(string[] args)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(LogPath));
        Directory.CreateDirectory(BackupRoot);
        File.WriteAllText(LogPath, "[" + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "] begin" + Environment.NewLine);

        Session session = Session.GetSession();
        ListingWindow listing = session.ListingWindow;
        listing.Open();

        foreach (string path in TemplatePaths)
        {
            FixTemplate(session, listing, path);
        }

        WriteLog(listing, "end");
    }

    private static void FixTemplate(Session session, ListingWindow listing, string path)
    {
        WriteLog(listing, "TEMPLATE " + path);
        if (!File.Exists(path))
        {
            WriteLog(listing, "  missing");
            return;
        }

        PartLoadStatus loadStatus = null;
        BasePart basePart = null;
        try
        {
            FileAttributes attributes = File.GetAttributes(path);
            if ((attributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
            {
                File.SetAttributes(path, attributes & ~FileAttributes.ReadOnly);
                WriteLog(listing, "  cleared readonly");
            }

            string backupPath = Path.Combine(BackupRoot, Path.GetFileName(path));
            File.Copy(path, backupPath, true);
            WriteLog(listing, "  backup=" + backupPath);

            basePart = session.Parts.OpenBaseDisplay(path, out loadStatus);
            Part part = basePart as Part;
            if (part == null)
            {
                WriteLog(listing, "  not part");
                return;
            }

            WriteQuantityAttributes(listing, part, "before");

            ArrayList valueCells = FindQuantityValueCells(listing, part);
            WriteLog(listing, "  quantity value cells=" + valueCells.Count.ToString());

            UFSession uf = UFSession.GetUFSession();
            for (int i = 0; i < valueCells.Count; ++i)
            {
                uf.Tabnot.SetCellText((Tag)valueCells[i], "0");
            }

            DeleteQuantityString(part);
            EnsureIntegerAttribute(part);

            string referenceText = CreatePartQuantityReference(part);
            WriteLog(listing, "  generated reference=" + Safe(referenceText));
            if (string.IsNullOrEmpty(referenceText) || referenceText.IndexOf("@" + QuantityTitle, StringComparison.OrdinalIgnoreCase) < 0)
            {
                throw new InvalidOperationException("Failed to generate quantity attribute reference text.");
            }

            for (int i = 0; i < valueCells.Count; ++i)
            {
                uf.Tabnot.SetCellText((Tag)valueCells[i], referenceText);
            }
            UpdateAllTabnotes(part);

            WriteQuantityAttributes(listing, part, "after");
            LogQuantityCells(listing, part);

            PartSaveStatus saveStatus = part.Save(BasePart.SaveComponents.False, BasePart.CloseAfterSave.True);
            if (saveStatus != null)
            {
                saveStatus.Dispose();
            }
            WriteLog(listing, "  saved");
        }
        catch (Exception ex)
        {
            WriteLog(listing, "  failed: " + ex.ToString());
            if (basePart != null)
            {
                try
                {
                    basePart.Close(BasePart.CloseWholeTree.False, BasePart.CloseModified.DontCloseModified, null);
                }
                catch
                {
                }
            }
        }
        finally
        {
            if (loadStatus != null)
            {
                loadStatus.Dispose();
            }
        }
    }

    private static ArrayList FindQuantityValueCells(ListingWindow listing, Part part)
    {
        UFSession uf = UFSession.GetUFSession();
        ArrayList cells = new ArrayList();
        ArrayList processed = new ArrayList();
        Tag objectTag = Tag.Null;
        while (true)
        {
            uf.Obj.CycleObjsInPart(part.Tag, UFConstants.UF_tabular_note_type, ref objectTag);
            if (objectTag == Tag.Null)
            {
                break;
            }

            Tag tabnote = ResolveTabnote(uf, objectTag);
            string key = tabnote.ToString();
            if (tabnote == Tag.Null || processed.Contains(key))
            {
                continue;
            }
            processed.Add(key);

            int rows = 0;
            int columns = 0;
            uf.Tabnot.AskNmRows(tabnote, out rows);
            uf.Tabnot.AskNmColumns(tabnote, out columns);
            WriteLog(listing, "  tabnote=" + key + " rows=" + rows.ToString() + " cols=" + columns.ToString());

            for (int r = 0; r < rows; ++r)
            {
                Tag row = Tag.Null;
                uf.Tabnot.AskNthRow(tabnote, r, out row);
                for (int c = 0; c < columns - 1; ++c)
                {
                    Tag labelCell = GetCell(uf, tabnote, row, c);
                    if (labelCell == Tag.Null)
                    {
                        continue;
                    }

                    string labelRaw = null;
                    string labelEval = null;
                    try
                    {
                        uf.Tabnot.AskCellText(labelCell, out labelRaw);
                        uf.Tabnot.AskEvaluatedCellText(labelCell, out labelEval);
                    }
                    catch
                    {
                        continue;
                    }

                    if (IsQuantityLabel(labelRaw) || IsQuantityLabel(labelEval))
                    {
                        Tag valueCell = GetCell(uf, tabnote, row, c + 1);
                        if (valueCell != Tag.Null && !cells.Contains(valueCell))
                        {
                            cells.Add(valueCell);
                            WriteLog(listing, "    quantity value cell r=" + r.ToString() + " c=" + (c + 1).ToString());
                        }
                    }
                }
            }
        }

        return cells;
    }

    private static Tag ResolveTabnote(UFSession uf, Tag objectTag)
    {
        int type = 0;
        int subtype = 0;
        uf.Obj.AskTypeAndSubtype(objectTag, out type, out subtype);

        if (subtype == UFConstants.UF_tabular_note_subtype)
        {
            return objectTag;
        }
        if (subtype == UFConstants.UF_tabular_note_section_subtype)
        {
            Tag tabnote = Tag.Null;
            try
            {
                uf.Tabnot.AskTabularNoteOfSection(objectTag, out tabnote);
                return tabnote;
            }
            catch
            {
            }
        }
        return Tag.Null;
    }

    private static Tag GetCell(UFSession uf, Tag tabnote, Tag row, int columnIndex)
    {
        try
        {
            Tag column = Tag.Null;
            Tag cell = Tag.Null;
            uf.Tabnot.AskNthColumn(tabnote, columnIndex, out column);
            uf.Tabnot.AskCellAtRowCol(row, column, out cell);
            return cell;
        }
        catch
        {
            return Tag.Null;
        }
    }

    private static bool IsQuantityLabel(string text)
    {
        if (string.IsNullOrEmpty(text))
        {
            return false;
        }
        return text.IndexOf(QuantityTitle, StringComparison.OrdinalIgnoreCase) >= 0 &&
            text.IndexOf("@", StringComparison.OrdinalIgnoreCase) < 0;
    }

    private static void DeleteQuantityString(Part part)
    {
        bool deleted = true;
        while (deleted)
        {
            deleted = false;
            if (part.HasUserAttribute(QuantityTitle, NXObject.AttributeType.String, -1))
            {
                part.DeleteUserAttribute(NXObject.AttributeType.String, QuantityTitle, true, Update.Option.Now);
                deleted = true;
            }
        }
    }

    private static void EnsureIntegerAttribute(Part part)
    {
        part.SetUserAttribute(QuantityTitle, -1, 0, Update.Option.Now);
    }

    private static string CreatePartQuantityReference(Part part)
    {
        DraftingNoteBuilder builder = null;
        try
        {
            builder = part.Annotations.CreateDraftingNoteBuilder(null);
            string[] empty = new string[] { "" };
            builder.Text.TextBlock.SetText(empty);
            builder.Text.TextBlock.AddAttributeReference(part, QuantityTitle, false, 1, 1);
            string[] referenceText = builder.Text.TextBlock.GetText();
            builder.Text.TextBlock.SetText(empty);
            if (referenceText == null || referenceText.Length == 0)
            {
                return "";
            }
            return referenceText[0];
        }
        finally
        {
            if (builder != null)
            {
                builder.Destroy();
            }
        }
    }

    private static void UpdateAllTabnotes(Part part)
    {
        UFSession uf = UFSession.GetUFSession();
        ArrayList processed = new ArrayList();
        Tag objectTag = Tag.Null;
        while (true)
        {
            uf.Obj.CycleObjsInPart(part.Tag, UFConstants.UF_tabular_note_type, ref objectTag);
            if (objectTag == Tag.Null)
            {
                break;
            }
            Tag tabnote = ResolveTabnote(uf, objectTag);
            string key = tabnote.ToString();
            if (tabnote == Tag.Null || processed.Contains(key))
            {
                continue;
            }
            processed.Add(key);
            try
            {
                uf.Tabnot.Update(tabnote);
            }
            catch
            {
            }
        }
    }

    private static void LogQuantityCells(ListingWindow listing, Part part)
    {
        UFSession uf = UFSession.GetUFSession();
        ArrayList processed = new ArrayList();
        Tag objectTag = Tag.Null;
        while (true)
        {
            uf.Obj.CycleObjsInPart(part.Tag, UFConstants.UF_tabular_note_type, ref objectTag);
            if (objectTag == Tag.Null)
            {
                break;
            }
            Tag tabnote = ResolveTabnote(uf, objectTag);
            string key = tabnote.ToString();
            if (tabnote == Tag.Null || processed.Contains(key))
            {
                continue;
            }
            processed.Add(key);

            int rows = 0;
            int columns = 0;
            uf.Tabnot.AskNmRows(tabnote, out rows);
            uf.Tabnot.AskNmColumns(tabnote, out columns);
            for (int r = 0; r < rows; ++r)
            {
                Tag row = Tag.Null;
                uf.Tabnot.AskNthRow(tabnote, r, out row);
                for (int c = 0; c < columns; ++c)
                {
                    Tag cell = GetCell(uf, tabnote, row, c);
                    if (cell == Tag.Null)
                    {
                        continue;
                    }
                    string raw = null;
                    string eval = null;
                    try
                    {
                        uf.Tabnot.AskCellText(cell, out raw);
                        uf.Tabnot.AskEvaluatedCellText(cell, out eval);
                    }
                    catch
                    {
                        continue;
                    }
                    if (!string.IsNullOrEmpty(raw) && raw.IndexOf(QuantityTitle, StringComparison.OrdinalIgnoreCase) >= 0)
                    {
                        WriteLog(listing, "  quantity cell r=" + r.ToString() + " c=" + c.ToString() + " raw=" + Safe(raw) + " eval=" + Safe(eval));
                    }
                }
            }
        }
    }

    private static void WriteQuantityAttributes(ListingWindow listing, Part part, string label)
    {
        bool found = false;
        foreach (NXObject.AttributeInformation attr in part.GetUserAttributes())
        {
            if (attr.Title == QuantityTitle)
            {
                found = true;
                WriteLog(listing, "  " + label + " attr type=" + attr.Type.ToString() +
                    " string=" + Safe(attr.StringValue) +
                    " int=" + attr.IntegerValue.ToString());
            }
        }
        if (!found)
        {
            WriteLog(listing, "  " + label + " attr <none>");
        }
    }

    private static string Safe(string value)
    {
        return value == null ? "<null>" : value.Replace("\r", "\\r").Replace("\n", "\\n");
    }

    private static void WriteLog(ListingWindow listing, string message)
    {
        listing.WriteLine(message);
        File.AppendAllText(LogPath, "[" + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "] " + message + Environment.NewLine);
    }

    public static int GetUnloadOption(string dummy)
    {
        return (int)Session.LibraryUnloadOption.Immediately;
    }
}
