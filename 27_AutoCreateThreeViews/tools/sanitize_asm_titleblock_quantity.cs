using System;
using System.Collections.Generic;
using NXOpen;
using NXOpen.Annotations;

public class SanitizeAsmTitleBlockQuantity
{
    private const string QuantityTitle = "数量";

    public static void Main(string[] args)
    {
        if (args == null || args.Length == 0)
        {
            throw new ArgumentException("At least one ASM template path is required.");
        }

        Session session = Session.GetSession();
        foreach (string path in args)
        {
            PartLoadStatus loadStatus;
            BasePart basePart = session.Parts.OpenBaseDisplay(path, out loadStatus);
            loadStatus.Dispose();
            Part part = basePart as Part;
            if (part == null)
            {
                throw new InvalidOperationException("Not an NX part: " + path);
            }

            int clearedCells = 0;
            int titleBlocks = 0;
            foreach (TitleBlock titleBlock in part.DraftingManager.TitleBlocks)
            {
                titleBlocks++;
                DefineTitleBlockBuilder builder =
                    part.DraftingManager.TitleBlocks.CreateDefineTitleBlockBuilder(titleBlock);
                try
                {
                    foreach (TitleBlockCellBuilder cell in builder.Cells.GetContents())
                    {
                        if (cell == null)
                        {
                            continue;
                        }

                        string label = cell.Label ?? string.Empty;
                        string editable = cell.EditableText ?? string.Empty;
                        string evaluated = cell.Text ?? string.Empty;
                        bool referencesQuantity =
                            label.Contains(QuantityTitle) ||
                            editable.Contains(QuantityTitle) ||
                            evaluated.Contains(QuantityTitle);
                        if (!referencesQuantity)
                        {
                            continue;
                        }

                        cell.Lock = false;
                        cell.EditableText = string.Empty;
                        clearedCells++;
                        Console.WriteLine(
                            "Cleared title-block quantity cell: label=\"" + label +
                            "\", editable=\"" + editable + "\"");
                    }

                    builder.Commit();
                }
                finally
                {
                    builder.Destroy();
                }
            }

            if (part.HasUserAttribute(
                    QuantityTitle,
                    NXObject.AttributeType.Integer,
                    -1))
            {
                part.DeleteUserAttribute(
                    NXObject.AttributeType.Integer,
                    QuantityTitle,
                    true,
                    Update.Option.Later);
                Console.WriteLine("Deleted Integer part attribute template: 数量");
            }

            part.Save(
                BasePart.SaveComponents.False,
                BasePart.CloseAfterSave.False).Dispose();
            part.Close(
                BasePart.CloseWholeTree.False,
                BasePart.CloseModified.CloseModified,
                null);

            Console.WriteLine(
                "Saved " + path + "; titleBlocks=" + titleBlocks +
                ", clearedCells=" + clearedCells);
        }
    }

    public static int GetUnloadOption(string dummy)
    {
        return (int)Session.LibraryUnloadOption.Immediately;
    }
}
