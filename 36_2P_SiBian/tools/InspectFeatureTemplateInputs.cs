using System;
using System.Collections.Generic;
using System.IO;
using NXOpen;
using NXOpen.UserDefinedTemplate;

public static class InspectFeatureTemplateInputs
{
    private static readonly Session Session = Session.GetSession();

    public static void Main(string[] args)
    {
        if (args == null || args.Length == 0)
        {
            throw new ArgumentException("Pass feature-template PRT paths after -args.");
        }

        string outputDirectory = Path.GetDirectoryName(Path.GetFullPath(args[0].Trim('"')));
        string reportPath = Path.Combine(outputDirectory, "feature_template_inputs.log");
        var report = new List<string>();
        string scratchPath = Path.Combine(
            outputDirectory,
            "__feature_template_inspection_target_" + DateTime.UtcNow.Ticks + ".prt");

        Part targetPart = Session.Parts.NewDisplay(scratchPath, Part.Units.Millimeters);
        try
        {
            foreach (string argument in args)
            {
                InspectOne(targetPart, Path.GetFullPath(argument.Trim('"')), report);
            }
            File.WriteAllLines(reportPath, report);
        }
        catch (Exception ex)
        {
            File.WriteAllText(
                reportPath,
                "ERROR TYPE=" + ex.GetType().FullName + Environment.NewLine +
                "MESSAGE=" + ex.Message + Environment.NewLine +
                "STACK=" + ex.StackTrace + Environment.NewLine);
            throw;
        }
        finally
        {
            targetPart.Close(
                BasePart.CloseWholeTree.True,
                BasePart.CloseModified.CloseModified,
                null);
        }
    }

    private static void InspectOne(Part targetPart, string templatePath, IList<string> report)
    {
        InstantiationBuilder builder =
            targetPart.UserDefinedTemplates.CreateInstantiationBuilder(null);
        try
        {
            Part authoringPart = builder.LoadAuthoringPart(templatePath);
            if (authoringPart == null)
            {
                throw new InvalidOperationException("Failed to load authoring part: " + templatePath);
            }

            report.Add("===== " + Path.GetFileName(templatePath) + " =====");
            Definition definition = authoringPart.UserDefinedTemplates.FindDefinitionObjectInPart();
            DefinitionBuilder definitionBuilder =
                authoringPart.UserDefinedTemplates.CreateDefinitionBuilder(definition);
            try
            {
                ConfigurableObject root = authoringPart.ConfigurationManager.GetRootNode();
                ConfigurableObject ftsRoot = authoringPart.ConfigurationManager.GetRootNodeByType(
                    (int)ConfigurationManager.TemplateType.Fts);
                ConfigurableObject configuredRoot = definitionBuilder.ConfigurableObject;
                report.Add("ROOT manager=" + (root != null ? root.Tag.ToString() : "null") +
                           " fts=" + (ftsRoot != null ? ftsRoot.Tag.ToString() : "null") +
                           " definition=" + (configuredRoot != null ? configuredRoot.Tag.ToString() : "null"));
                Expression[] editableExpressions = definitionBuilder.GetEditableExpressions();
                report.Add("DEFINITION_EDITABLE_EXPRESSIONS " + editableExpressions.Length);
                foreach (Expression editable in editableExpressions)
                {
                    report.Add("  EDITABLE " + DescribeExpression(editable));
                }
            }
            finally
            {
                definitionBuilder.Destroy();
            }
            Expression[] expressions = builder.GetExpressions();
            report.Add("EXPRESSIONS " + expressions.Length);
            for (int index = 0; index < expressions.Length; ++index)
            {
                Expression expression = expressions[index];
                bool canBeEdited;
                Expression matched = builder.GetMatchedExpression(expression, out canBeEdited);
                report.Add(
                    "  [" + index + "] original=" + DescribeExpression(expression) +
                    " editable=" + canBeEdited +
                    " matched=" + DescribeExpression(matched));
            }

            NXObject[] references = builder.GetReferences();
            report.Add("REFERENCES " + references.Length);
            for (int index = 0; index < references.Length; ++index)
            {
                NXObject reference = references[index];
                report.Add("  [" + index + "] " + DescribeObject(reference));
            }
        }
        finally
        {
            builder.Destroy();
        }
    }

    private static string DescribeExpression(Expression expression)
    {
        if (expression == null)
        {
            return "<null>";
        }
        return "tag=" + expression.Tag +
               " name=" + expression.Name +
               " equation=" + expression.Equation +
               " rhs=" + expression.RightHandSide;
    }

    private static string DescribeObject(NXObject value)
    {
        if (value == null)
        {
            return "<null>";
        }
        return "tag=" + value.Tag +
               " type=" + value.GetType().FullName +
               " name=" + value.Name +
               " journal=" + value.JournalIdentifier;
    }

    public static int GetUnloadOption(string dummy)
    {
        return (int)Session.LibraryUnloadOption.Immediately;
    }
}
