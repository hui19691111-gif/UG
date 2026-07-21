using System;
using System.Collections.Generic;
using System.IO;
using NXOpen;
using NXOpen.UF;
using NXOpen.UserDefinedTemplate;

public static class TestFeatureTemplateInstantiation
{
    private static readonly Session Session = Session.GetSession();
    private static readonly UFSession UfSession = UFSession.GetUFSession();

    public static void Main(string[] args)
    {
        string templatePath = Path.GetFullPath(args[0].Trim('"'));
        string outputDirectory = Path.GetDirectoryName(templatePath);
        string targetPath = Path.Combine(
            outputDirectory,
            "__feature_template_group_test_" + DateTime.UtcNow.Ticks + ".prt");
        string reportPath = Path.Combine(outputDirectory, "feature_template_group_test.log");
        var report = new List<string>();

        try
        {
            Part targetPart = Session.Parts.NewDisplay(targetPath, Part.Units.Millimeters);
            Tag blockFeature;
            UfSession.Modl.CreateBlock1(
                FeatureSigns.Nullsign,
                new[] { 0.0, 0.0, 0.0 },
                new[] { "100", "100", "2" },
                out blockFeature);

            Body targetBody = null;
            foreach (Body body in targetPart.Bodies)
            {
                targetBody = body;
                break;
            }
            if (targetBody == null)
            {
                throw new InvalidOperationException("The scratch block body was not created.");
            }

            Face selectedFace = null;
            foreach (Face face in targetBody.GetFaces())
            {
                if (face.GetEdges().Length >= 4)
                {
                    selectedFace = face;
                    break;
                }
            }
            if (selectedFace == null)
            {
                throw new InvalidOperationException("No four-edge block face was found.");
            }
            Edge[] selectedEdges = selectedFace.GetEdges();

            InstantiationBuilder builder =
                targetPart.UserDefinedTemplates.CreateInstantiationBuilder(null);
            try
            {
                builder.LoadAuthoringPart(templatePath);
                report.Add("EXPLODE_FLAG " + builder.ExplodeFlag);
                builder.LayerOption =
                    InstantiationBuilder.JaUserdefinedtemplateinstantiationLayerOption.Work;
                report.Add("BOOLEAN_FLAG " + builder.BooleanFlag);

                Expression[] expressions = builder.GetExpressions();
                foreach (Expression original in expressions)
                {
                    bool canBeEdited;
                    Expression matched = builder.GetMatchedExpression(original, out canBeEdited);
                    if (matched == null)
                    {
                        throw new InvalidOperationException("No matched expression for " + original.Name);
                    }
                    string name = original.Name.ToLowerInvariant();
                    double value = name == "p42" || name == "p33" || name == "p16" ? 2.0 :
                                   name == "p43" || name == "p34" || name == "p24" ? 0.5 : 0.2;
                    matched.RightHandSide =
                        value.ToString(System.Globalization.CultureInfo.InvariantCulture);
                    report.Add("EXPRESSION " + original.Name + " editable=" + canBeEdited + " value=" + value);
                }

                NXObject[] references = builder.GetReferences();
                int edgeIndex = 0;
                foreach (NXObject originalReference in references)
                {
                    NXObject matchedReference;
                    if (originalReference is Face)
                    {
                        matchedReference = selectedFace;
                    }
                    else if (originalReference is Body)
                    {
                        matchedReference = targetBody;
                    }
                    else if (originalReference is Edge)
                    {
                        if (edgeIndex >= selectedEdges.Length)
                        {
                            throw new InvalidOperationException("Not enough target edges for the template references.");
                        }
                        matchedReference = selectedEdges[edgeIndex++];
                    }
                    else
                    {
                        bool flipped;
                        NXObject automaticMatch =
                            builder.GetMatchedReference(originalReference, out flipped);
                        report.Add(
                            "REFERENCE_INTERNAL " + originalReference.GetType().FullName +
                            " automatic=" +
                            (automaticMatch != null ? automaticMatch.Tag.ToString() : "null"));
                        continue;
                    }
                    builder.SetMatchedReference(originalReference, matchedReference, false);
                    report.Add("REFERENCE " + originalReference.GetType().Name + " -> " + matchedReference.Tag);
                }

                NXObject committed = builder.Commit();
                report.Add("COMMITTED " + (committed != null ? committed.Tag.ToString() : "null"));
                NXObject[] committedObjects = builder.GetCommittedObjects();
                report.Add("COMMITTED_OBJECTS " + committedObjects.Length);
                foreach (NXObject value in committedObjects)
                {
                    report.Add("  " + value.Tag + " " + value.GetType().FullName + " " + value.Name);
                }
            }
            finally
            {
                builder.Destroy();
            }

            int featureCount = 0;
            NXOpen.Features.FeatureGroup templateGroup = null;
            NXOpen.Features.Feature udtTemplateFeature = null;
            var templateFeatureTags = new List<Tag>();
            foreach (NXOpen.Features.Feature feature in targetPart.Features)
            {
                ++featureCount;
                report.Add("FEATURE " + feature.Tag + " " + feature.FeatureType + " " + feature.Name +
                           " CLR=" + feature.GetType().FullName);
                if (feature.Tag != blockFeature)
                {
                    templateFeatureTags.Add(feature.Tag);
                }
                if (feature.FeatureType.IndexOf("UDT Feature Group", StringComparison.OrdinalIgnoreCase) >= 0)
                {
                    udtTemplateFeature = feature;
                }
                NXOpen.Features.FeatureGroup group = feature as NXOpen.Features.FeatureGroup;
                if (group != null)
                {
                    templateGroup = group;
                }
            }
            report.Add("FEATURE_COUNT " + featureCount);
            if (udtTemplateFeature == null)
            {
                throw new InvalidOperationException("No UDT template feature was found.");
            }
            Instantiation editableInstantiation =
                targetPart.UserDefinedTemplates.FindInstantiationObjectFromTemplateFeature(
                    udtTemplateFeature);
            if (editableInstantiation == null)
            {
                throw new InvalidOperationException("The UDT template feature has no editable instantiation object.");
            }
            InstantiationBuilder editBuilder =
                targetPart.UserDefinedTemplates.CreateInstantiationBuilder(editableInstantiation);
            try
            {
                Expression[] editExpressions = editBuilder.GetExpressions();
                report.Add("EDIT_EXPRESSIONS " + editExpressions.Length);
                foreach (Expression original in editExpressions)
                {
                    bool canBeEdited;
                    Expression matched = editBuilder.GetMatchedExpression(original, out canBeEdited);
                    report.Add("  EDIT " + original.Name + " editable=" + canBeEdited +
                               " matched=" + (matched != null ? matched.Name : "null"));
                }
                NXObject edited = editBuilder.Commit();
                report.Add("EDIT_COMMIT " + (edited != null ? edited.Tag.ToString() : "null"));
            }
            finally
            {
                editBuilder.Destroy();
            }

            if (templateGroup == null)
            {
                report.Add("GROUP_APPEND_SKIPPED no NXOpen.Features.FeatureGroup wrapper; testing UF feature set");
            }
            Tag appendedBlockTag;
            UfSession.Modl.CreateBlock1(
                FeatureSigns.Nullsign,
                new[] { 150.0, 0.0, 0.0 },
                new[] { "10", "10", "10" },
                out appendedBlockTag);
            NXOpen.Features.Feature appendedFeature =
                NXOpen.Utilities.NXObjectManager.Get(appendedBlockTag) as NXOpen.Features.Feature;
            if (appendedFeature == null)
            {
                throw new InvalidOperationException("The post-template feature was not resolved.");
            }
            if (templateGroup != null)
            {
                templateGroup.AddMembersWithRelocation(
                    new[] { appendedFeature }, true, false);
                templateGroup.SetName("2P_SiBian");
                NXOpen.Features.Feature[] groupMembers;
                templateGroup.GetMembers(out groupMembers);
                report.Add("GROUP_APPEND_OK group=" + templateGroup.Tag +
                           " memberCount=" + groupMembers.Length +
                           " appended=" + appendedFeature.Tag);
            }
            else
            {
                templateFeatureTags.Add(appendedFeature.Tag);
                Tag featureSetTag;
                UfSession.Modl.CreateSetOfFeature(
                    "2P_SiBian",
                    templateFeatureTags.ToArray(),
                    templateFeatureTags.Count,
                    1,
                    out featureSetTag);
                report.Add("FEATURE_SET_OK set=" + featureSetTag +
                           " memberCount=" + templateFeatureTags.Count);
            }

            PartSaveStatus saveStatus = targetPart.Save(
                BasePart.SaveComponents.False,
                BasePart.CloseAfterSave.False);
            saveStatus.Dispose();
            File.WriteAllLines(reportPath, report);
        }
        catch (Exception ex)
        {
            report.Add("ERROR TYPE=" + ex.GetType().FullName);
            report.Add("MESSAGE=" + ex.Message);
            report.Add("STACK=" + ex.StackTrace);
            File.WriteAllLines(reportPath, report);
            throw;
        }
    }

    public static int GetUnloadOption(string dummy)
    {
        return (int)Session.LibraryUnloadOption.Immediately;
    }
}
