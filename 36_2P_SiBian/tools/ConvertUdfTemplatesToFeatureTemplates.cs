using System;
using System.Collections.Generic;
using System.IO;
using NXOpen;
using NXOpen.Features;
using NXOpen.UserDefinedTemplate;

public static class ConvertUdfTemplatesToFeatureTemplates
{
    private static readonly Session Session = Session.GetSession();

    public static void Main(string[] args)
    {
        string reportPath = Path.Combine(
            args != null && args.Length > 0
                ? Path.GetDirectoryName(Path.GetFullPath(args[0].Trim('"'))) ?? Environment.CurrentDirectory
                : Environment.CurrentDirectory,
            "feature_template_conversion.log");
        try
        {
            File.WriteAllText(reportPath, "START" + Environment.NewLine);
            if (args == null || args.Length == 0)
            {
                throw new ArgumentException("Pass one or more full template PRT paths after -args.");
            }

            var report = new List<string>();
            foreach (string argument in args)
            {
                string templatePath = Path.GetFullPath(argument.Trim('"'));
                ConvertOne(templatePath, report);
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
    }

    private static void ConvertOne(string templatePath, IList<string> report)
    {
        if (!File.Exists(templatePath))
        {
            throw new FileNotFoundException("Template part was not found.", templatePath);
        }

        PartLoadStatus loadStatus;
        Part part = Session.Parts.OpenDisplay(templatePath, out loadStatus);
        loadStatus.Dispose();
        if (part == null)
        {
            throw new InvalidOperationException("NX did not open template part: " + templatePath);
        }
        File.AppendAllText(
            Path.Combine(Path.GetDirectoryName(templatePath), "feature_template_conversion.log"),
            "OPENED " + templatePath + Environment.NewLine);

        report.Add("BEGIN " + templatePath);
        Definition definition = part.UserDefinedTemplates.FindDefinitionObjectInPart();
        if (definition == null)
        {
            report.Add("UDF definition: automatic authoring-part resolution");
            File.AppendAllText(
                Path.Combine(Path.GetDirectoryName(templatePath), "feature_template_conversion.log"),
                "CONVERT BEGIN" + Environment.NewLine);

            // In a UDF authoring part NX resolves the definition itself. Passing
            // that definition feature back to NX 2412 can be interpreted as an
            // instantiated UDF and can terminate the batch process internally.
            part.UserDefinedTemplates.ConvertUdfToFeaturetemplate(null);
            File.AppendAllText(
                Path.Combine(Path.GetDirectoryName(templatePath), "feature_template_conversion.log"),
                "CONVERT END" + Environment.NewLine);
            definition = part.UserDefinedTemplates.FindDefinitionObjectInPart();
        }
        if (definition == null)
        {
            throw new InvalidOperationException(
                "Conversion returned without a feature-template definition: " + templatePath);
        }

        NXObject[] objects = definition.GetObjects();
        report.Add("FEATURE_TEMPLATE " + definition.Tag + " objectCount=" + objects.Length);

        string[] editableNames = EditableExpressionNames(Path.GetFileName(templatePath));
        var editableExpressions = new List<Expression>();
        foreach (string expressionName in editableNames)
        {
            Expression expression = part.Expressions.FindObject(expressionName);
            if (expression == null)
            {
                throw new InvalidOperationException(
                    "Editable expression was not found: " + expressionName + " in " + templatePath);
            }
            editableExpressions.Add(expression);
        }

        DefinitionBuilder definitionBuilder =
            part.UserDefinedTemplates.CreateDefinitionBuilder(definition);
        try
        {
            definitionBuilder.SetObjects(objects);
            report.Add("DEFINITION_BUILDER_CONFIG_BEFORE " +
                       (definitionBuilder.ConfigurableObject != null
                           ? definitionBuilder.ConfigurableObject.Tag.ToString()
                           : "null"));
            Expression[] existingEditable = definitionBuilder.GetEditableExpressions();
            if (existingEditable.Length > 0)
            {
                definitionBuilder.RemoveEditableExpressions(existingEditable);
            }
            definitionBuilder.AddEditableExpressions(editableExpressions.ToArray());
            report.Add("EDITABLE_BEFORE_COMMIT " +
                       definitionBuilder.GetEditableExpressions().Length);

            ConfigurationManager configurationManager = part.ConfigurationManager;
            ConfigurableObject root = configurationManager.CreateRootNode(
                ConfigurationManager.TemplateType.Fts);
            root.SetParameter(ConfigurableObject.PropertyId.Title, "2P_SiBian");
            root.SetParameter(ConfigurableObject.PropertyId.TemplateName, "2P_SiBian");

            ConfigurableObject expressionGroup = configurationManager.CreateItemNode(
                ConfigurationManager.ItemType.Group);
            expressionGroup.SetParameter(ConfigurableObject.PropertyId.Title, "参数");
            configurationManager.DragDropNode(root, expressionGroup, root);

            foreach (Expression expression in editableExpressions)
            {
                ItemNodeExpression expressionNode =
                    (ItemNodeExpression)configurationManager.CreateItemNode(
                        ConfigurationManager.ItemType.Number);
                expressionNode.SetReferenceExpression(expression);
                expressionNode.SetParameter(
                    ConfigurableObject.PropertyId.Title,
                    EditableExpressionLabel(Path.GetFileName(templatePath), expression.Name));
                configurationManager.DragDropNode(root, expressionNode, expressionGroup);
            }

            ConfigurableObject referenceGroup = configurationManager.CreateItemNode(
                ConfigurationManager.ItemType.Group);
            referenceGroup.SetParameter(ConfigurableObject.PropertyId.Title, "定位参考");
            configurationManager.DragDropNode(root, referenceGroup, root);

            int referenceOrdinal = 0;
            int edgeReferenceOrdinal = 0;
            foreach (NXObject reference in definitionBuilder.GetReferences())
            {
                ItemNodeGeometry geometryNode =
                    (ItemNodeGeometry)configurationManager.CreateItemNode(
                        ConfigurationManager.ItemType.Geometry);
                geometryNode.SetWaveLink(reference);
                geometryNode.SetParameter(
                    ConfigurableObject.PropertyId.Title,
                    ReferenceLabel(reference, edgeReferenceOrdinal));
                configurationManager.DragDropNode(root, geometryNode, referenceGroup);
                if (reference is Edge)
                {
                    ++edgeReferenceOrdinal;
                }
                ++referenceOrdinal;
            }

            root.Update(ConfigurableObject.UpdateType.ExternalChange);
            definitionBuilder.ConfigurableObject = root;
            report.Add("NEW_ROOT " + root.Tag);
            NXObject committedDefinition = definitionBuilder.Commit();
            report.Add("DEFINITION_COMMIT " +
                       (committedDefinition != null ? committedDefinition.Tag.ToString() : "null") +
                       " editableAfter=" + definitionBuilder.GetEditableExpressions().Length +
                       " configAfter=" +
                       (definitionBuilder.ConfigurableObject != null
                           ? definitionBuilder.ConfigurableObject.Tag.ToString()
                           : "null"));
        }
        finally
        {
            definitionBuilder.Destroy();
        }
        report.Add("EDITABLE_EXPRESSIONS " + string.Join(",", editableNames));

        PartSaveStatus saveStatus = part.Save(
            BasePart.SaveComponents.False,
            BasePart.CloseAfterSave.False);
        saveStatus.Dispose();
        report.Add("SAVED " + templatePath);

        part.Close(
            BasePart.CloseWholeTree.True,
            BasePart.CloseModified.CloseModified,
            null);
    }

    private static string EditableExpressionLabel(string templateFileName, string expressionName)
    {
        string name = templateFileName.ToUpperInvariant();
        string expression = expressionName.ToLowerInvariant();
        if (expression == "p16" || expression == "p42" || expression == "p33")
        {
            return "板厚";
        }
        if (expression == "p24" || expression == "p43" || expression == "p34")
        {
            return "折弯R";
        }
        if (expression == "p17" || expression == "p45" || expression == "p36" ||
            expression == "p52" || expression == "p61")
        {
            return "间隙";
        }
        return expressionName;
    }

    private static string ReferenceLabel(NXObject reference, int ordinal)
    {
        if (reference is Face)
        {
            return "选择面";
        }
        if (reference is Edge)
        {
            int edgeOrdinal = ordinal;
            return edgeOrdinal == 0 ? "第一条边" :
                   edgeOrdinal == 1 ? "第二条边" : "第三条边";
        }
        return "内部定位";
    }

    private static string[] EditableExpressionNames(string templateFileName)
    {
        string name = templateFileName.ToUpperInvariant();
        if (name.StartsWith("2P_SIBIAN_1_"))
        {
            return new[] { "p16", "p17", "p24" };
        }
        if (name.StartsWith("2P_SIBIAN_90R_"))
        {
            return new[] { "p33", "p34", "p36" };
        }
        if (name.StartsWith("2P_SIBIAN_90_"))
        {
            return new[] { "p42", "p43", "p45" };
        }
        if (name.StartsWith("90JIANXICAOR_"))
        {
            return new[] { "p42", "p43", "p61" };
        }
        if (name.StartsWith("90JIANXICAO_"))
        {
            return new[] { "p42", "p43", "p52" };
        }
        throw new InvalidOperationException(
            "No editable-expression mapping is defined for template: " + templateFileName);
    }

    public static int GetUnloadOption(string dummy)
    {
        return (int)Session.LibraryUnloadOption.Immediately;
    }
}
