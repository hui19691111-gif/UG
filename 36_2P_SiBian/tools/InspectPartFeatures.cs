using System;
using System.Collections.Generic;
using System.IO;
using NXOpen;
using NXOpen.Features;

public static class InspectPartFeatures
{
    private static readonly Session Session = Session.GetSession();

    public static void Main(string[] args)
    {
        string path = Path.GetFullPath(args[0].Trim('"'));
        string reportPath = path + ".features.log";
        var report = new List<string>();
        PartLoadStatus status;
        Part part = Session.Parts.OpenDisplay(path, out status);
        status.Dispose();
        foreach (Feature feature in part.Features)
        {
            report.Add("FEATURE tag=" + feature.Tag + " type=" + feature.FeatureType +
                       " name=" + feature.Name + " bodies=" + feature.GetBodies().Length);
        }
        foreach (Body body in part.Bodies)
        {
            report.Add("BODY tag=" + body.Tag + " name=" + body.Name);
        }
        File.WriteAllLines(reportPath, report);
        part.Close(BasePart.CloseWholeTree.True, BasePart.CloseModified.CloseModified, null);
    }

    public static int GetUnloadOption(string dummy)
    {
        return (int)Session.LibraryUnloadOption.Immediately;
    }
}
