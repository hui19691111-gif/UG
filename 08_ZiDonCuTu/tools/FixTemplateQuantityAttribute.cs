using System;
using System.Collections;
using System.IO;
using NXOpen;
using NXOpen.UF;

public class NXJournal
{
    private const string PluginRoot = "D:\\UG\u667A\u8F89\u94A3\u91D1\u63D2\u4EF6";
    private const string QuantityTitle = "\u6570\u91CF";

    private static readonly string LogPath = Path.Combine(
        PluginRoot,
        "logs",
        "template_quantity_fix.log");

    private static readonly string BackupRoot = Path.Combine(
        PluginRoot,
        "backup",
        "template_quantity_fix_" + "20260710_1144");

    private static readonly string[] TemplatePaths =
    {
        Path.Combine(PluginRoot, "DATA", "A4-noviews-template.prt"),
        Path.Combine(PluginRoot, "DATA", "A4-noviews-template1.prt")
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
            if (!File.Exists(backupPath))
            {
                File.Copy(path, backupPath, true);
                WriteLog(listing, "  backup=" + backupPath);
            }

            basePart = session.Parts.OpenBaseDisplay(path, out loadStatus);
            Part part = basePart as Part;
            if (part == null)
            {
                WriteLog(listing, "  not part");
                return;
            }

            WriteQuantityAttributes(listing, part, "before");
            int changedCells = BreakQuantityTabularReferences(listing, part);
            WriteLog(listing, "  changed tabular cells=" + changedCells.ToString());
            DeleteQuantityString(part);
            EnsureIntegerAttribute(part);
            WriteQuantityAttributes(listing, part, "after");

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

    private static int BreakQuantityTabularReferences(ListingWindow listing, Part part)
    {
        UFSession uf = UFSession.GetUFSession();
        int changed = 0;
        ArrayList processed = new ArrayList();
        Tag objectTag = Tag.Null;
        while (true)
        {
            uf.Obj.CycleObjsInPart(part.Tag, UFConstants.UF_tabular_note_type, ref objectTag);
            if (objectTag == Tag.Null)
            {
                break;
            }

            int type = 0;
            int subtype = 0;
            uf.Obj.AskTypeAndSubtype(objectTag, out type, out subtype);

            Tag tabnote = Tag.Null;
            if (subtype == UFConstants.UF_tabular_note_subtype)
            {
                tabnote = objectTag;
            }
            else if (subtype == UFConstants.UF_tabular_note_section_subtype)
            {
                try
                {
                    uf.Tabnot.AskTabularNoteOfSection(objectTag, out tabnote);
                }
                catch
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            string tabnoteKey = tabnote.ToString();
            if (tabnote == Tag.Null || processed.Contains(tabnoteKey))
            {
                continue;
            }
            processed.Add(tabnoteKey);

            int rows = 0;
            int columns = 0;
            uf.Tabnot.AskNmRows(tabnote, out rows);
            uf.Tabnot.AskNmColumns(tabnote, out columns);
            WriteLog(listing, "  tabnote=" + tabnote.ToString() + " rows=" + rows.ToString() + " cols=" + columns.ToString());

            for (int r = 0; r < rows; ++r)
            {
                Tag row = Tag.Null;
                uf.Tabnot.AskNthRow(tabnote, r, out row);
                for (int c = 0; c < columns; ++c)
                {
                    Tag column = Tag.Null;
                    Tag cell = Tag.Null;
                    string text = null;
                    try
                    {
                        uf.Tabnot.AskNthColumn(tabnote, c, out column);
                        uf.Tabnot.AskCellAtRowCol(row, column, out cell);
                        uf.Tabnot.AskCellText(cell, out text);
                    }
                    catch
                    {
                        continue;
                    }

                    if (IsQuantityReferenceCell(text))
                    {
                        WriteLog(listing, "    replace cell r=" + r.ToString() + " c=" + c.ToString() + " text=" + Safe(text));
                        uf.Tabnot.SetCellText(cell, "0");
                        changed++;
                    }
                }
            }

            try
            {
                uf.Tabnot.Update(tabnote);
            }
            catch
            {
            }
        }

        return changed;
    }

    private static bool IsQuantityReferenceCell(string text)
    {
        if (string.IsNullOrEmpty(text))
        {
            return false;
        }
        return text.IndexOf("@" + QuantityTitle, StringComparison.OrdinalIgnoreCase) >= 0 ||
            text.IndexOf(QuantityTitle, StringComparison.OrdinalIgnoreCase) >= 0 && text.IndexOf("<WRef", StringComparison.OrdinalIgnoreCase) >= 0;
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
        if (part.HasUserAttribute(QuantityTitle, NXObject.AttributeType.Integer, -1))
        {
            part.SetUserAttribute(QuantityTitle, -1, 0, Update.Option.Now);
            return;
        }

        part.SetUserAttribute(QuantityTitle, -1, 0, Update.Option.Now);
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
