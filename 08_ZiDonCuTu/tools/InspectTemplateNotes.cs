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
        "template_note_inspect.log");

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
        File.WriteAllText(LogPath, "[" + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "] begin" + Environment.NewLine);

        Session session = Session.GetSession();
        ListingWindow listing = session.ListingWindow;
        listing.Open();

        foreach (string path in TemplatePaths)
        {
            InspectTemplate(session, listing, path);
        }

        WriteLog(listing, "end");
    }

    private static void InspectTemplate(Session session, ListingWindow listing, string path)
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
            basePart = session.Parts.OpenBaseDisplay(path, out loadStatus);
            Part part = basePart as Part;
            if (part == null)
            {
                WriteLog(listing, "  not part");
                return;
            }

            foreach (NXObject.AttributeInformation attr in part.GetUserAttributes())
            {
                if (attr.Title == QuantityTitle)
                {
                    WriteLog(listing, "  ATTR title=" + attr.Title + " type=" + attr.Type.ToString() + " string=" + Safe(attr.StringValue) + " int=" + attr.IntegerValue.ToString());
                }
            }

            BaseNote[] notes = part.Notes.ToArray();
            WriteLog(listing, "  NOTE count=" + notes.Length.ToString());
            for (int i = 0; i < notes.Length; ++i)
            {
                string[] lines = notes[i].GetText();
                string joined = string.Join("\\n", lines);
                if (joined.IndexOf(QuantityTitle, StringComparison.OrdinalIgnoreCase) >= 0 ||
                    joined.IndexOf("<", StringComparison.OrdinalIgnoreCase) >= 0 ||
                    joined.Trim().Length == 0)
                {
                    WriteLog(listing, "  NOTE[" + i.ToString() + "] type=" + notes[i].GetType().FullName + " lines=" + lines.Length.ToString());
                    for (int j = 0; j < lines.Length; ++j)
                    {
                        WriteLog(listing, "    L" + j.ToString() + "=" + Safe(lines[j]));
                    }
                }
            }

            InspectTabularCells(listing, part);
        }
        catch (Exception ex)
        {
            WriteLog(listing, "  failed: " + ex.ToString());
        }
        finally
        {
            if (basePart != null)
            {
                try
                {
                    basePart.Close(BasePart.CloseWholeTree.False, BasePart.CloseModified.CloseModified, null);
                }
                catch
                {
                }
            }
            if (loadStatus != null)
            {
                loadStatus.Dispose();
            }
        }
    }

    private static void InspectTabularCells(ListingWindow listing, Part part)
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
            WriteLog(listing, "  TABNOTE tag=" + key + " rows=" + rows.ToString() + " cols=" + columns.ToString());

            for (int r = 0; r < rows; ++r)
            {
                Tag row = Tag.Null;
                uf.Tabnot.AskNthRow(tabnote, r, out row);
                for (int c = 0; c < columns; ++c)
                {
                    Tag column = Tag.Null;
                    Tag cell = Tag.Null;
                    string text = null;
                    string eval = null;
                    try
                    {
                        uf.Tabnot.AskNthColumn(tabnote, c, out column);
                        uf.Tabnot.AskCellAtRowCol(row, column, out cell);
                        uf.Tabnot.AskCellText(cell, out text);
                        uf.Tabnot.AskEvaluatedCellText(cell, out eval);
                    }
                    catch
                    {
                        continue;
                    }
                    if (!string.IsNullOrEmpty(text))
                    {
                        WriteLog(listing, "    r=" + r.ToString() + " c=" + c.ToString() + " raw=" + Safe(text) + " eval=" + Safe(eval));
                    }
                }
            }
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
