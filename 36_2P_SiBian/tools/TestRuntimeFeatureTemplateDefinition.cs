using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using NXOpen;
using NXOpen.Features;
using NXOpen.UF;
using NXOpen.UserDefinedTemplate;

public static class TestRuntimeFeatureTemplateDefinition
{
    private static readonly Session Session = Session.GetSession();
    private static readonly UFSession Uf = UFSession.GetUFSession();

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    private struct UdfsDefData
    {
        public IntPtr frecs;
        public int numFrecs;
        public IntPtr expDefs;
        public int numExp;
        public IntPtr refs;
        public IntPtr refPrompts;
        public int numRef;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string name;
        public int explosionFlag;
    }

    [DllImport("libufun.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int UF_MODL_udfs_create_def(
        out uint featureTag, ref UdfsDefData data);

    public static void Main(string[] args)
    {
        string directory = args != null && args.Length > 0
            ? Path.GetFullPath(args[0].Trim('"'))
            : Environment.CurrentDirectory;
        Directory.CreateDirectory(directory);
        string partPath = Path.Combine(directory, "__runtime_full_template_definition_test.prt");
        string reportPath = Path.Combine(directory, "runtime_full_template_definition_test.log");
        string exportedPath = Path.Combine(directory, "__runtime_full_template_export_test.prt");
        var report = new List<string>();

        try
        {
            if (File.Exists(partPath)) File.Delete(partPath);
            Part part = Session.Parts.NewDisplay(partPath, Part.Units.Millimeters);

            Tag firstTag;
            Uf.Modl.CreateBlock1(FeatureSigns.Nullsign,
                new[] { 0.0, 0.0, 0.0 }, new[] { "100", "100", "10" }, out firstTag);
            Tag secondTag;
            Uf.Modl.CreateBlock1(FeatureSigns.Nullsign,
                new[] { 20.0, 20.0, 0.0 }, new[] { "20", "20", "20" }, out secondTag);

            Feature first = NXOpen.Utilities.NXObjectManager.Get(firstTag) as Feature;
            Feature second = NXOpen.Utilities.NXObjectManager.Get(secondTag) as Feature;
            if (first == null || second == null)
                throw new InvalidOperationException("Scratch block features were not resolved.");

            Body targetBody = first.GetBodies()[0];
            Body toolBody = second.GetBodies()[0];
            Tag subtractTag;
            Uf.Modl.SubtractBodiesWithRetainedOptions(
                targetBody.Tag, toolBody.Tag, false, false, out subtractTag);
            Feature subtract =
                NXOpen.Utilities.NXObjectManager.Get(subtractTag) as Feature;
            if (subtract == null)
                throw new InvalidOperationException("Scratch subtract feature was not resolved.");
            first.SetName("BASE_EXTERNAL");
            second.SetName("MEMBER_TOOL");
            subtract.SetName("MEMBER_FINAL");

            if (File.Exists(exportedPath)) File.Delete(exportedPath);
            Body finalBody = subtract.GetBodies()[0];
            Uf.Part.Export(exportedPath, 1, new Tag[] { finalBody.Tag });
            report.Add("PRE_DEFINITION_EXPORT " + exportedPath);

            IntPtr featureArray = Marshal.AllocHGlobal(sizeof(uint) * 2);
            IntPtr referenceArray = Marshal.AllocHGlobal(sizeof(uint));
            IntPtr referencePrompt = Marshal.StringToHGlobalAnsi("Target body");
            IntPtr referencePromptArray = Marshal.AllocHGlobal(IntPtr.Size);
            uint udfDefinitionTag = 0;
            try
            {
                Marshal.WriteInt32(featureArray, 0, unchecked((int)(uint)secondTag));
                Marshal.WriteInt32(featureArray, sizeof(uint), unchecked((int)(uint)subtractTag));
                Marshal.WriteInt32(referenceArray, 0, unchecked((int)(uint)targetBody.Tag));
                Marshal.WriteIntPtr(referencePromptArray, 0, referencePrompt);
                var data = new UdfsDefData
                {
                    frecs = featureArray,
                    numFrecs = 2,
                    expDefs = IntPtr.Zero,
                    numExp = 0,
                    refs = referenceArray,
                    refPrompts = referencePromptArray,
                    numRef = 1,
                    name = "RuntimeFullTemplate",
                    explosionFlag = 0
                };
                int result = UF_MODL_udfs_create_def(out udfDefinitionTag, ref data);
                report.Add("UDFS_CREATE result=" + result + " tag=" + udfDefinitionTag);
                if (result != 0 || udfDefinitionTag == 0)
                    throw new InvalidOperationException("UF_MODL_udfs_create_def failed: " + result);
            }
            finally
            {
                Marshal.FreeHGlobal(featureArray);
                Marshal.FreeHGlobal(referenceArray);
                Marshal.FreeHGlobal(referencePromptArray);
                Marshal.FreeHGlobal(referencePrompt);
            }

            Feature udfDefinitionFeature =
                NXOpen.Utilities.NXObjectManager.Get((Tag)udfDefinitionTag) as Feature;
            if (udfDefinitionFeature == null)
                throw new InvalidOperationException("UDF definition feature was not resolved.");
            part.UserDefinedTemplates.ConvertUdfToFeaturetemplate(udfDefinitionFeature);

            Definition definition = part.UserDefinedTemplates.FindDefinitionObjectInPart();
            report.Add("DEFINITION " + (definition != null ? definition.Tag.ToString() : "null"));
            if (definition == null)
                throw new InvalidOperationException("Feature-template definition was not created.");

            DefinitionBuilder builder = part.UserDefinedTemplates.CreateDefinitionBuilder(definition);
            try
            {
                NXObject[] objects = definition.GetObjects();
                builder.SetObjects(objects);
                ConfigurationManager manager = part.ConfigurationManager;
                ConfigurableObject root = manager.CreateRootNode(
                    ConfigurationManager.TemplateType.Fts);
                root.SetParameter(ConfigurableObject.PropertyId.Title, "RuntimeFullTemplate");
                root.SetParameter(ConfigurableObject.PropertyId.TemplateName, "RuntimeFullTemplate");
                root.Update(ConfigurableObject.UpdateType.ExternalChange);
                builder.ConfigurableObject = root;
                NXObject committed = builder.Commit();
                report.Add("FTS_CONFIG_COMMIT " +
                    (committed != null ? committed.Tag.ToString() : "null") +
                    " objects=" + objects.Length +
                    " refs=" + builder.GetReferences().Length);
            }
            finally
            {
                try { builder.Destroy(); } catch { }
            }
            foreach (Feature feature in part.Features)
            {
                bool hidden = false;
                try { hidden = feature.IsInternal; } catch { }
                report.Add("FEATURE " + feature.Tag + " type=" + feature.FeatureType +
                           " name=" + feature.Name + " clr=" + feature.GetType().FullName +
                           " internal=" + hidden);
            }

            Feature udtGroup = null;
            foreach (Feature feature in part.Features)
            {
                if (feature.FeatureType.IndexOf("UDT Feature Group", StringComparison.OrdinalIgnoreCase) >= 0)
                    udtGroup = feature;
            }
            if (udtGroup == null)
                throw new InvalidOperationException("Converted UDT feature group was not found.");
            report.Add("EXPORT_SKIPPED group=" + udtGroup.Tag);

            PartSaveStatus saveStatus = part.Save(
                BasePart.SaveComponents.False, BasePart.CloseAfterSave.False);
            saveStatus.Dispose();
            File.WriteAllLines(reportPath, report);
        }
        catch (Exception ex)
        {
            report.Add("ERROR " + ex.GetType().FullName + " " + ex.Message);
            report.Add(ex.StackTrace ?? "");
            File.WriteAllLines(reportPath, report);
            throw;
        }
    }

    public static int GetUnloadOption(string dummy)
    {
        return (int)Session.LibraryUnloadOption.Immediately;
    }
}
