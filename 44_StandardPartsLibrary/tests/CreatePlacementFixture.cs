using System;
using System.IO;
using NXOpen;
using NXOpen.UF;

// Run with run_journal.exe and pass an unused destination .prt with -args.
// Creates a disposable part for interactive placement regression checks.
public class CreatePlacementFixture
{
    public static void Main(string[] args)
    {
        if (args.Length != 1 || File.Exists(args[0]))
            throw new ArgumentException("Pass one new test part path.");
        UFSession uf = UFSession.GetUFSession();
        Tag part, feature, body;
        uf.Part.New(args[0], 1, out part);
        uf.Modl.CreateBlock1(FeatureSigns.Nullsign, new double[] {0, 0, -10},
            new string[] {"120", "80", "10"}, out feature);
        uf.Modl.AskFeatBody(feature, out body);
        uf.Obj.SetName(body, "PLACEMENT_TEST_TARGET");
        foreach (double x in new double[] {20, 60, 100})
        {
            Tag point;
            uf.Curve.CreatePoint(new double[] {x, 40, 0}, out point);
        }
        uf.Part.Save();
        Console.WriteLine("FIXTURE_CREATED:" + args[0]);
    }
    public static int GetUnloadOption(string dummy)
    {
        return (int)Session.LibraryUnloadOption.Immediately;
    }
}
