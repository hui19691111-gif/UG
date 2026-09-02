using System;
using System.IO;
using NXOpen;

public class NXJournal
{
    private const string LogPath = @"D:\UG智辉钣金插件\logs\template_quantity_integer.log";

    private static readonly string[] TemplatePaths =
    {
        @"D:\UG智辉钣金插件\DATA\A4-noviews-template.prt",
        @"D:\UG智辉钣金插件\DATA\A4-noviews-template-.prt",
        @"D:\UG智辉钣金插件\DATA\A4-noviews-template1.prt",
        @"D:\UG智辉钣金插件\DATA\A4-noviews-template2.prt"
    };

    public static void Main(string[] args)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(LogPath));
        File.AppendAllText(LogPath, "[" + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "] begin" + Environment.NewLine);

        Session session = Session.GetSession();
        ListingWindow listing = session.ListingWindow;
        listing.Open();

        foreach (string path in TemplatePaths)
        {
            if (!File.Exists(path))
            {
                WriteLog(listing, "Missing template: " + path);
                continue;
            }

            PartLoadStatus loadStatus = null;
            BasePart basePart = null;
            try
            {
                FileAttributes attributes = File.GetAttributes(path);
                if ((attributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
                {
                    File.SetAttributes(path, attributes & ~FileAttributes.ReadOnly);
                    WriteLog(listing, "Cleared readonly: " + path);
                }

                basePart = session.Parts.OpenBaseDisplay(path, out loadStatus);
                Part part = basePart as Part;
                if (part == null)
                {
                    WriteLog(listing, "Not a part: " + path);
                    continue;
                }

                DeleteQuantity(part);
                part.SetUserAttribute("数量", -1, 0, Update.Option.Now);

                PartSaveStatus saveStatus = part.Save(
                    BasePart.SaveComponents.False,
                    BasePart.CloseAfterSave.True);
                if (saveStatus != null)
                {
                    saveStatus.Dispose();
                }
                WriteLog(listing, "Updated quantity attribute to integer: " + path);
            }
            catch (Exception ex)
            {
                WriteLog(listing, "Failed: " + path + " :: " + ex.Message);
                if (basePart != null)
                {
                    try
                    {
                        basePart.Close(
                            BasePart.CloseWholeTree.False,
                            BasePart.CloseModified.DontCloseModified,
                            null);
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
        File.AppendAllText(LogPath, "[" + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "] end" + Environment.NewLine);
    }

    private static void DeleteQuantity(NXObject obj)
    {
        NXObject.AttributeType[] types =
        {
            NXObject.AttributeType.String,
            NXObject.AttributeType.Integer,
            NXObject.AttributeType.Real
        };

        foreach (NXObject.AttributeType type in types)
        {
            bool deleted = true;
            while (deleted)
            {
                deleted = false;
                try
                {
                    if (obj.HasUserAttribute("数量", type, -1))
                    {
                        obj.DeleteUserAttribute(type, "数量", true, Update.Option.Now);
                        deleted = true;
                    }
                }
                catch
                {
                }
            }
        }
    }

    public static int GetUnloadOption(string dummy)
    {
        return (int)Session.LibraryUnloadOption.Immediately;
    }

    private static void WriteLog(ListingWindow listing, string message)
    {
        listing.WriteLine(message);
        File.AppendAllText(LogPath, "[" + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "] " + message + Environment.NewLine);
    }
}
