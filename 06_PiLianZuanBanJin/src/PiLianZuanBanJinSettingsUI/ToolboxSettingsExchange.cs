using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace ZhsmToolboxSettingsUI
{
    public sealed class ToolboxSettingsRequest
    {
        public string WindowTitle { get; set; } = string.Empty;
        public bool ExportStep { get; set; }
        public bool ExportIges { get; set; }
        public bool StepPreserveAssembly { get; set; } = true;
        public bool IgesFlattenAssembly { get; set; }
        public string MultibodySignatureTolerance { get; set; } = "0.001";
        public string MultibodyMinBodyVolume { get; set; } = "10.0";
        public string MultibodyMinBodyMaxDimension { get; set; } = "2.0";
        public string MultibodyPlacementWarnBboxDelta { get; set; } = "0.5";
        public string MultibodyPlacementWarnAxisAngleDeg { get; set; } = "5.0";
        public bool MultibodyAutoOpen { get; set; } = true;
        public bool MultibodyIncludeHidden { get; set; }
        public string MultibodyToAssemblyLayerBlacklist { get; set; } = string.Empty;
        public string MultibodyToAssemblyNamePrefixBlacklist { get; set; } = "TEMP_,REF_,WAVE_,AUX_";
        public bool AssemblyToMultibodyAutoOpen { get; set; } = true;
        public bool AssemblyToMultibodyRecursive { get; set; } = true;
        public bool AssemblyToMultibodyInheritDisplay { get; set; } = true;
        public string AssemblyToMultibodyLayerBlacklist { get; set; } = string.Empty;
        public string AssemblyToMultibodyNamePrefixBlacklist { get; set; } = "TEMP_,REF_,WAVE_,AUX_";
        public string AutoDrawingProjectionStandard { get; set; } = "FirstAngle";
        public string AutoDrawingFirstAngleTemplatePath { get; set; } = string.Empty;
        public string AutoDrawingThirdAngleTemplatePath { get; set; } = string.Empty;
        public string AutoDrawingTemplatePath { get; set; } = string.Empty;
        public string AutoDrawingTopMarginMm { get; set; } = "15";
        public string AutoDrawingBottomMarginMm { get; set; } = "5";
        public string AutoDrawingLeftMarginMm { get; set; } = "5";
        public string AutoDrawingRightMarginMm { get; set; } = "5";
        public string AutoDrawingTitleBlockHeightMm { get; set; } = "30";
        public string AutoDrawingTitleBlockWidthMm { get; set; } = "150";
        public string AutoDrawingViewToFrameGapMm { get; set; } = "15";
        public string AutoDrawingViewGapMm { get; set; } = "21";
        public bool AutoDrawingRotateViewEnabled { get; set; } = true;
        public string AutoDrawingRotateViewMode { get; set; } = "ViewLongEdgeHorizontal";
        public bool OneClickColorShowPaletteEveryClick { get; set; } = true;
        public string OneClickColorPaletteKind { get; set; } = "Light";
        public bool OneClickColorColorSheetMetalBodies { get; set; } = true;
        public bool OneClickColorColorSameEntityGroups { get; set; } = true;
        public bool OneClickColorColorMirroredSameEntityGroups { get; set; }
        public bool OneClickColorColorRemainingEntities { get; set; } = true;
        public bool OneClickColorOnlyTopLevelOccurrence { get; set; } = true;
        public bool OneClickColorClearBodyColor { get; set; } = true;
        public bool OneClickColorClearFaceColor { get; set; } = true;
        public bool OneClickColorClearFeatureColor { get; set; } = true;
    }

    public sealed class ToolboxSettingsResponse
    {
        public bool Accepted { get; set; }
        public bool ExportStep { get; set; }
        public bool ExportIges { get; set; }
        public bool StepPreserveAssembly { get; set; } = true;
        public bool IgesFlattenAssembly { get; set; }
        public string MultibodySignatureTolerance { get; set; } = "0.001";
        public string MultibodyMinBodyVolume { get; set; } = "10.0";
        public string MultibodyMinBodyMaxDimension { get; set; } = "2.0";
        public string MultibodyPlacementWarnBboxDelta { get; set; } = "0.5";
        public string MultibodyPlacementWarnAxisAngleDeg { get; set; } = "5.0";
        public bool MultibodyAutoOpen { get; set; } = true;
        public bool MultibodyIncludeHidden { get; set; }
        public string MultibodyToAssemblyLayerBlacklist { get; set; } = string.Empty;
        public string MultibodyToAssemblyNamePrefixBlacklist { get; set; } = "TEMP_,REF_,WAVE_,AUX_";
        public bool AssemblyToMultibodyAutoOpen { get; set; } = true;
        public bool AssemblyToMultibodyRecursive { get; set; } = true;
        public bool AssemblyToMultibodyInheritDisplay { get; set; } = true;
        public string AssemblyToMultibodyLayerBlacklist { get; set; } = string.Empty;
        public string AssemblyToMultibodyNamePrefixBlacklist { get; set; } = "TEMP_,REF_,WAVE_,AUX_";
        public string AutoDrawingProjectionStandard { get; set; } = "FirstAngle";
        public string AutoDrawingFirstAngleTemplatePath { get; set; } = string.Empty;
        public string AutoDrawingThirdAngleTemplatePath { get; set; } = string.Empty;
        public string AutoDrawingTemplatePath { get; set; } = string.Empty;
        public string AutoDrawingTopMarginMm { get; set; } = "15";
        public string AutoDrawingBottomMarginMm { get; set; } = "5";
        public string AutoDrawingLeftMarginMm { get; set; } = "5";
        public string AutoDrawingRightMarginMm { get; set; } = "5";
        public string AutoDrawingTitleBlockHeightMm { get; set; } = "30";
        public string AutoDrawingTitleBlockWidthMm { get; set; } = "150";
        public string AutoDrawingViewToFrameGapMm { get; set; } = "15";
        public string AutoDrawingViewGapMm { get; set; } = "21";
        public bool AutoDrawingRotateViewEnabled { get; set; } = true;
        public string AutoDrawingRotateViewMode { get; set; } = "ViewLongEdgeHorizontal";
        public bool OneClickColorShowPaletteEveryClick { get; set; } = true;
        public string OneClickColorPaletteKind { get; set; } = "Light";
        public bool OneClickColorColorSheetMetalBodies { get; set; } = true;
        public bool OneClickColorColorSameEntityGroups { get; set; } = true;
        public bool OneClickColorColorMirroredSameEntityGroups { get; set; }
        public bool OneClickColorColorRemainingEntities { get; set; } = true;
        public bool OneClickColorOnlyTopLevelOccurrence { get; set; } = true;
        public bool OneClickColorClearBodyColor { get; set; } = true;
        public bool OneClickColorClearFaceColor { get; set; } = true;
        public bool OneClickColorClearFeatureColor { get; set; } = true;
    }

    public static class ToolboxSettingsExchange
    {
        private const string WorkspaceDeployRoot = @"I:\ZFBJ\deploy\zhaofu-suite";
        private const string ToolboxSettingsFileName = "toolbox.settings.json";
        private const string AutoDrawingSettingsFileName = "toolbox.auto-drawing.settings.json";
        private const string OneClickColorSettingsFileName = "one_click_color_config.json";
        private const string DefaultFirstAngleTemplateRelativePath = @"templates\auto-drawing\ZF-A4-first-angle-default.prt";
        private const string DefaultThirdAngleTemplateRelativePath = @"templates\auto-drawing\ZF-A4-third-angle-default.prt";

        public static ToolboxSettingsRequest LoadRequest(string path)
        {
            ToolboxSettingsRequest request = new ToolboxSettingsRequest();
            Dictionary<string, string> map = LoadKeyValueFile(path);
            request.WindowTitle = GetValue(map, "window_title", "昭福工具设置");
            request.ExportStep = GetBool(map, "export_step", request.ExportStep);
            request.ExportIges = GetBool(map, "export_iges", request.ExportIges);
            request.StepPreserveAssembly = GetBool(map, "step_preserve_assembly", request.StepPreserveAssembly);
            request.IgesFlattenAssembly = GetBool(map, "iges_flatten_assembly", request.IgesFlattenAssembly);
            request.MultibodySignatureTolerance = GetValue(map, "multibody_signature_tolerance", request.MultibodySignatureTolerance);
            request.MultibodyMinBodyVolume = GetValue(map, "multibody_min_body_volume", request.MultibodyMinBodyVolume);
            request.MultibodyMinBodyMaxDimension = GetValue(map, "multibody_min_body_max_dimension", request.MultibodyMinBodyMaxDimension);
            request.MultibodyPlacementWarnBboxDelta = GetValue(map, "multibody_placement_warn_bbox_delta", request.MultibodyPlacementWarnBboxDelta);
            request.MultibodyPlacementWarnAxisAngleDeg = GetValue(map, "multibody_placement_warn_axis_angle_deg", request.MultibodyPlacementWarnAxisAngleDeg);
            request.MultibodyAutoOpen = GetBool(map, "multibody_auto_open", request.MultibodyAutoOpen);
            request.MultibodyIncludeHidden = GetBool(map, "multibody_include_hidden", request.MultibodyIncludeHidden);
            request.MultibodyToAssemblyLayerBlacklist = GetValue(map, "multibody_to_assembly_layer_blacklist", request.MultibodyToAssemblyLayerBlacklist);
            request.MultibodyToAssemblyNamePrefixBlacklist = GetValue(map, "multibody_to_assembly_name_prefix_blacklist", request.MultibodyToAssemblyNamePrefixBlacklist);
            request.AssemblyToMultibodyAutoOpen = GetBool(map, "assembly_to_multibody_auto_open", request.AssemblyToMultibodyAutoOpen);
            request.AssemblyToMultibodyRecursive = GetBool(map, "assembly_to_multibody_recursive", request.AssemblyToMultibodyRecursive);
            request.AssemblyToMultibodyInheritDisplay = GetBool(map, "assembly_to_multibody_inherit_display", request.AssemblyToMultibodyInheritDisplay);
            request.AssemblyToMultibodyLayerBlacklist = GetValue(map, "assembly_to_multibody_layer_blacklist", request.AssemblyToMultibodyLayerBlacklist);
            request.AssemblyToMultibodyNamePrefixBlacklist = GetValue(map, "assembly_to_multibody_name_prefix_blacklist", request.AssemblyToMultibodyNamePrefixBlacklist);
            request.AutoDrawingProjectionStandard = LoadPersistedAutoDrawingSetting(
                "projection_standard",
                "auto_drawing_projection_standard",
                request.AutoDrawingProjectionStandard);
            request.AutoDrawingFirstAngleTemplatePath = LoadPersistedProjectionTemplatePath(
                "first_angle_template_path",
                "auto_drawing_first_angle_template_path",
                ResolveDefaultAutoDrawingTemplatePath("FirstAngle"),
                true);
            request.AutoDrawingThirdAngleTemplatePath = LoadPersistedProjectionTemplatePath(
                "third_angle_template_path",
                "auto_drawing_third_angle_template_path",
                ResolveDefaultAutoDrawingTemplatePath("ThirdAngle"),
                false);
            request.AutoDrawingTemplatePath = ResolveLegacyAutoDrawingTemplatePath(
                request.AutoDrawingProjectionStandard,
                request.AutoDrawingFirstAngleTemplatePath,
                request.AutoDrawingThirdAngleTemplatePath);
            request.AutoDrawingTopMarginMm = LoadPersistedAutoDrawingSetting("top_margin_mm", "auto_drawing_top_margin_mm", request.AutoDrawingTopMarginMm);
            request.AutoDrawingBottomMarginMm = LoadPersistedAutoDrawingSetting("bottom_margin_mm", "auto_drawing_bottom_margin_mm", request.AutoDrawingBottomMarginMm);
            request.AutoDrawingLeftMarginMm = LoadPersistedAutoDrawingSetting("left_margin_mm", "auto_drawing_left_margin_mm", request.AutoDrawingLeftMarginMm);
            request.AutoDrawingRightMarginMm = LoadPersistedAutoDrawingSetting("right_margin_mm", "auto_drawing_right_margin_mm", request.AutoDrawingRightMarginMm);
            request.AutoDrawingTitleBlockHeightMm = LoadPersistedAutoDrawingSetting("title_block_height_mm", "auto_drawing_title_block_height_mm", request.AutoDrawingTitleBlockHeightMm);
            request.AutoDrawingTitleBlockWidthMm = LoadRequestOrPersistedAutoDrawingSetting(
                map,
                "title_block_width_mm",
                "auto_drawing_title_block_width_mm",
                request.AutoDrawingTitleBlockWidthMm);
            request.AutoDrawingViewToFrameGapMm = LoadPersistedAutoDrawingSetting("view_to_frame_gap_mm", "auto_drawing_view_to_frame_gap_mm", request.AutoDrawingViewToFrameGapMm);
            request.AutoDrawingViewGapMm = LoadPersistedAutoDrawingSetting("view_gap_mm", "auto_drawing_view_gap_mm", request.AutoDrawingViewGapMm);
            request.AutoDrawingRotateViewEnabled = LoadPersistedAutoDrawingBoolean("rotate_view_enabled", "auto_drawing_rotate_view_enabled", request.AutoDrawingRotateViewEnabled);
            request.AutoDrawingRotateViewMode = LoadPersistedAutoDrawingSetting("rotate_view_mode", "auto_drawing_rotate_view_mode", request.AutoDrawingRotateViewMode);
            request.OneClickColorShowPaletteEveryClick = LoadPersistedOneClickColorBoolean("ShowPaletteEveryClick", request.OneClickColorShowPaletteEveryClick);
            request.OneClickColorPaletteKind = NormalizeOneClickColorPaletteKind(LoadPersistedOneClickColorString("ColorPaletteKind", "PaletteKind", request.OneClickColorPaletteKind));
            request.OneClickColorColorSheetMetalBodies = LoadPersistedOneClickColorBoolean("ColorSheetMetalBodies", request.OneClickColorColorSheetMetalBodies);
            request.OneClickColorColorSameEntityGroups = LoadPersistedOneClickColorBoolean("ColorSameEntityGroups", "SameBodiesShareColor", request.OneClickColorColorSameEntityGroups);
            request.OneClickColorColorMirroredSameEntityGroups = LoadPersistedOneClickColorBoolean("ColorMirroredSameEntityGroups", "MirroredBodiesShareColor", request.OneClickColorColorMirroredSameEntityGroups);
            request.OneClickColorColorRemainingEntities = LoadPersistedOneClickColorBoolean("ColorRemainingEntities", "ColorRemainingBodies", request.OneClickColorColorRemainingEntities);
            request.OneClickColorOnlyTopLevelOccurrence = LoadPersistedOneClickColorBoolean("OnlyTopLevelOccurrence", "AssemblyTopLevelOnly", request.OneClickColorOnlyTopLevelOccurrence);
            request.OneClickColorClearBodyColor = LoadPersistedOneClickColorBoolean("ClearBodyColor", "ClearBodyColors", request.OneClickColorClearBodyColor);
            request.OneClickColorClearFaceColor = LoadPersistedOneClickColorBoolean("ClearFaceColor", "ClearFaceColors", request.OneClickColorClearFaceColor);
            request.OneClickColorClearFeatureColor = LoadPersistedOneClickColorBoolean("ClearFeatureColor", "ClearFeatureColors", request.OneClickColorClearFeatureColor);
            return request;
        }

        public static void SaveResponse(ToolboxSettingsResponse response, string path)
        {
            if (string.IsNullOrWhiteSpace(path))
            {
                return;
            }

            Directory.CreateDirectory(Path.GetDirectoryName(path) ?? AppContext.BaseDirectory);
            StringBuilder builder = new StringBuilder();
            Append(builder, "accepted", response.Accepted);
            Append(builder, "export_step", response.ExportStep);
            Append(builder, "export_iges", response.ExportIges);
            Append(builder, "step_preserve_assembly", response.StepPreserveAssembly);
            Append(builder, "iges_flatten_assembly", response.IgesFlattenAssembly);
            Append(builder, "multibody_signature_tolerance", response.MultibodySignatureTolerance);
            Append(builder, "multibody_min_body_volume", response.MultibodyMinBodyVolume);
            Append(builder, "multibody_min_body_max_dimension", response.MultibodyMinBodyMaxDimension);
            Append(builder, "multibody_placement_warn_bbox_delta", response.MultibodyPlacementWarnBboxDelta);
            Append(builder, "multibody_placement_warn_axis_angle_deg", response.MultibodyPlacementWarnAxisAngleDeg);
            Append(builder, "multibody_auto_open", response.MultibodyAutoOpen);
            Append(builder, "multibody_include_hidden", response.MultibodyIncludeHidden);
            Append(builder, "multibody_to_assembly_layer_blacklist", response.MultibodyToAssemblyLayerBlacklist);
            Append(builder, "multibody_to_assembly_name_prefix_blacklist", response.MultibodyToAssemblyNamePrefixBlacklist);
            Append(builder, "assembly_to_multibody_auto_open", response.AssemblyToMultibodyAutoOpen);
            Append(builder, "assembly_to_multibody_recursive", response.AssemblyToMultibodyRecursive);
            Append(builder, "assembly_to_multibody_inherit_display", response.AssemblyToMultibodyInheritDisplay);
            Append(builder, "assembly_to_multibody_layer_blacklist", response.AssemblyToMultibodyLayerBlacklist);
            Append(builder, "assembly_to_multibody_name_prefix_blacklist", response.AssemblyToMultibodyNamePrefixBlacklist);
            Append(builder, "auto_drawing_projection_standard", response.AutoDrawingProjectionStandard);
            Append(builder, "auto_drawing_first_angle_template_path", response.AutoDrawingFirstAngleTemplatePath);
            Append(builder, "auto_drawing_third_angle_template_path", response.AutoDrawingThirdAngleTemplatePath);
            Append(builder, "auto_drawing_template_path", response.AutoDrawingTemplatePath);
            Append(builder, "auto_drawing_top_margin_mm", response.AutoDrawingTopMarginMm);
            Append(builder, "auto_drawing_bottom_margin_mm", response.AutoDrawingBottomMarginMm);
            Append(builder, "auto_drawing_left_margin_mm", response.AutoDrawingLeftMarginMm);
            Append(builder, "auto_drawing_right_margin_mm", response.AutoDrawingRightMarginMm);
            Append(builder, "auto_drawing_title_block_height_mm", response.AutoDrawingTitleBlockHeightMm);
            Append(builder, "auto_drawing_title_block_width_mm", response.AutoDrawingTitleBlockWidthMm);
            Append(builder, "auto_drawing_view_to_frame_gap_mm", response.AutoDrawingViewToFrameGapMm);
            Append(builder, "auto_drawing_view_gap_mm", response.AutoDrawingViewGapMm);
            Append(builder, "auto_drawing_rotate_view_enabled", response.AutoDrawingRotateViewEnabled);
            Append(builder, "auto_drawing_rotate_view_mode", response.AutoDrawingRotateViewMode);
            Append(builder, "one_click_color_show_palette_every_click", response.OneClickColorShowPaletteEveryClick);
            Append(builder, "one_click_color_palette_kind", NormalizeOneClickColorPaletteKind(response.OneClickColorPaletteKind));
            Append(builder, "one_click_color_color_sheet_metal_bodies", response.OneClickColorColorSheetMetalBodies);
            Append(builder, "one_click_color_color_same_entity_groups", response.OneClickColorColorSameEntityGroups);
            Append(builder, "one_click_color_color_mirrored_same_entity_groups", response.OneClickColorColorMirroredSameEntityGroups);
            Append(builder, "one_click_color_color_remaining_entities", response.OneClickColorColorRemainingEntities);
            Append(builder, "one_click_color_only_top_level_occurrence", response.OneClickColorOnlyTopLevelOccurrence);
            Append(builder, "one_click_color_clear_body_color", response.OneClickColorClearBodyColor);
            Append(builder, "one_click_color_clear_face_color", response.OneClickColorClearFaceColor);
            Append(builder, "one_click_color_clear_feature_color", response.OneClickColorClearFeatureColor);
            File.WriteAllText(path, builder.ToString(), Encoding.UTF8);
        }

        public static void SavePersistentSettings(ToolboxSettingsResponse response)
        {
            string deployRoot = ResolveDeployRoot();
            string configDirectory = Path.Combine(deployRoot, "config");
            Directory.CreateDirectory(configDirectory);

            string toolboxSettingsPath = Path.Combine(configDirectory, ToolboxSettingsFileName);
            JsonObject root = LoadJsonObject(toolboxSettingsPath);
            root["export_step"] = response.ExportStep;
            root["export_iges"] = response.ExportIges;
            root["step_preserve_assembly"] = response.StepPreserveAssembly;
            root["iges_flatten_assembly"] = response.IgesFlattenAssembly;
            root["multibody_signature_tolerance"] = response.MultibodySignatureTolerance;
            root["multibody_min_body_volume"] = response.MultibodyMinBodyVolume;
            root["multibody_min_body_max_dimension"] = response.MultibodyMinBodyMaxDimension;
            root["multibody_placement_warn_bbox_delta"] = response.MultibodyPlacementWarnBboxDelta;
            root["multibody_placement_warn_axis_angle_deg"] = response.MultibodyPlacementWarnAxisAngleDeg;
            root["multibody_auto_open"] = response.MultibodyAutoOpen;
            root["multibody_include_hidden"] = response.MultibodyIncludeHidden;
            root["multibody_to_assembly_layer_blacklist"] = response.MultibodyToAssemblyLayerBlacklist;
            root["multibody_to_assembly_name_prefix_blacklist"] = response.MultibodyToAssemblyNamePrefixBlacklist;
            root["assembly_to_multibody_auto_open"] = response.AssemblyToMultibodyAutoOpen;
            root["assembly_to_multibody_recursive"] = response.AssemblyToMultibodyRecursive;
            root["assembly_to_multibody_inherit_display"] = response.AssemblyToMultibodyInheritDisplay;
            root["assembly_to_multibody_layer_blacklist"] = response.AssemblyToMultibodyLayerBlacklist;
            root["assembly_to_multibody_name_prefix_blacklist"] = response.AssemblyToMultibodyNamePrefixBlacklist;
            root["auto_drawing_projection_standard"] = response.AutoDrawingProjectionStandard;
            root["auto_drawing_first_angle_template_path"] = response.AutoDrawingFirstAngleTemplatePath;
            root["auto_drawing_third_angle_template_path"] = response.AutoDrawingThirdAngleTemplatePath;
            root["auto_drawing_template_path"] = response.AutoDrawingTemplatePath;
            root["auto_drawing_top_margin_mm"] = response.AutoDrawingTopMarginMm;
            root["auto_drawing_bottom_margin_mm"] = response.AutoDrawingBottomMarginMm;
            root["auto_drawing_left_margin_mm"] = response.AutoDrawingLeftMarginMm;
            root["auto_drawing_right_margin_mm"] = response.AutoDrawingRightMarginMm;
            root["auto_drawing_title_block_height_mm"] = response.AutoDrawingTitleBlockHeightMm;
            root["auto_drawing_title_block_width_mm"] = response.AutoDrawingTitleBlockWidthMm;
            root["auto_drawing_view_to_frame_gap_mm"] = response.AutoDrawingViewToFrameGapMm;
            root["auto_drawing_view_gap_mm"] = response.AutoDrawingViewGapMm;
            root["auto_drawing_rotate_view_enabled"] = response.AutoDrawingRotateViewEnabled;
            root["auto_drawing_rotate_view_mode"] = response.AutoDrawingRotateViewMode;
            File.WriteAllText(toolboxSettingsPath, root.ToJsonString(new JsonSerializerOptions { WriteIndented = true }), Encoding.UTF8);

            JsonObject autoDrawingRoot = new JsonObject
            {
                ["projection_standard"] = response.AutoDrawingProjectionStandard,
                ["first_angle_template_path"] = response.AutoDrawingFirstAngleTemplatePath,
                ["third_angle_template_path"] = response.AutoDrawingThirdAngleTemplatePath,
                ["template_path"] = response.AutoDrawingTemplatePath,
                ["top_margin_mm"] = response.AutoDrawingTopMarginMm,
                ["bottom_margin_mm"] = response.AutoDrawingBottomMarginMm,
                ["left_margin_mm"] = response.AutoDrawingLeftMarginMm,
                ["right_margin_mm"] = response.AutoDrawingRightMarginMm,
                ["title_block_height_mm"] = response.AutoDrawingTitleBlockHeightMm,
                ["title_block_width_mm"] = response.AutoDrawingTitleBlockWidthMm,
                ["view_to_frame_gap_mm"] = response.AutoDrawingViewToFrameGapMm,
                ["view_gap_mm"] = response.AutoDrawingViewGapMm,
                ["rotate_view_enabled"] = response.AutoDrawingRotateViewEnabled,
                ["rotate_view_mode"] = response.AutoDrawingRotateViewMode,
                ["updated_at"] = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss")
            };
            string autoDrawingSettingsPath = Path.Combine(configDirectory, AutoDrawingSettingsFileName);
            File.WriteAllText(autoDrawingSettingsPath, autoDrawingRoot.ToJsonString(new JsonSerializerOptions { WriteIndented = true }), Encoding.UTF8);

            JsonObject oneClickColorRoot = new JsonObject
            {
                ["ShowPaletteEveryClick"] = response.OneClickColorShowPaletteEveryClick,
                ["ColorPaletteKind"] = NormalizeOneClickColorPaletteKind(response.OneClickColorPaletteKind),
                ["ColorSheetMetalBodies"] = response.OneClickColorColorSheetMetalBodies,
                ["ColorSameEntityGroups"] = response.OneClickColorColorSameEntityGroups,
                ["ColorMirroredSameEntityGroups"] = response.OneClickColorColorMirroredSameEntityGroups,
                ["ColorRemainingEntities"] = response.OneClickColorColorRemainingEntities,
                ["OnlyTopLevelOccurrence"] = response.OneClickColorOnlyTopLevelOccurrence,
                ["ClearBodyColor"] = response.OneClickColorClearBodyColor,
                ["ClearFaceColor"] = response.OneClickColorClearFaceColor,
                ["ClearFeatureColor"] = response.OneClickColorClearFeatureColor
            };
            string oneClickColorSettingsPath = Path.Combine(configDirectory, OneClickColorSettingsFileName);
            File.WriteAllText(oneClickColorSettingsPath, oneClickColorRoot.ToJsonString(new JsonSerializerOptions { WriteIndented = true }), Encoding.UTF8);
        }

        public static string LoadPersistedAutoDrawingTemplatePath()
        {
            string persisted = LoadLegacyAutoDrawingTemplatePath();
            return string.IsNullOrWhiteSpace(persisted)
                ? ResolveDefaultAutoDrawingTemplatePath("FirstAngle")
                : persisted;
        }

        private static string LoadLegacyAutoDrawingTemplatePath()
        {
            string deployRoot = ResolveDeployRoot();
            string configDirectory = Path.Combine(deployRoot, "config");
            string autoDrawingSettingsPath = Path.Combine(configDirectory, AutoDrawingSettingsFileName);
            string templatePath = ReadTemplatePathFromJson(autoDrawingSettingsPath, "template_path");
            if (!string.IsNullOrWhiteSpace(templatePath))
            {
                return templatePath;
            }

            string toolboxSettingsPath = Path.Combine(configDirectory, ToolboxSettingsFileName);
            return ReadTemplatePathFromJson(toolboxSettingsPath, "auto_drawing_template_path");
        }

        private static string LoadPersistedProjectionTemplatePath(
            string dedicatedPropertyName,
            string mirroredPropertyName,
            string fallback,
            bool loadLegacyTemplateAsFallback)
        {
            string deployRoot = ResolveDeployRoot();
            string configDirectory = Path.Combine(deployRoot, "config");
            string autoDrawingSettingsPath = Path.Combine(configDirectory, AutoDrawingSettingsFileName);
            string dedicatedValue = ReadTemplatePathFromJson(autoDrawingSettingsPath, dedicatedPropertyName);
            if (!string.IsNullOrWhiteSpace(dedicatedValue))
            {
                return dedicatedValue;
            }

            string toolboxSettingsPath = Path.Combine(configDirectory, ToolboxSettingsFileName);
            string mirroredValue = ReadTemplatePathFromJson(toolboxSettingsPath, mirroredPropertyName);
            if (!string.IsNullOrWhiteSpace(mirroredValue))
            {
                return mirroredValue;
            }

            if (loadLegacyTemplateAsFallback)
            {
                string legacyValue = LoadLegacyAutoDrawingTemplatePath();
                if (!string.IsNullOrWhiteSpace(legacyValue))
                {
                    return legacyValue;
                }
            }

            return fallback;
        }

        private static string ResolveDefaultAutoDrawingTemplatePath(string projectionStandard)
        {
            string relativePath = string.Equals(projectionStandard, "ThirdAngle", StringComparison.OrdinalIgnoreCase)
                ? DefaultThirdAngleTemplateRelativePath
                : DefaultFirstAngleTemplateRelativePath;
            string path = Path.Combine(ResolveDeployRoot(), relativePath);
            return File.Exists(path) ? path : string.Empty;
        }

        private static string ResolveLegacyAutoDrawingTemplatePath(
            string projectionStandard,
            string firstAngleTemplatePath,
            string thirdAngleTemplatePath)
        {
            string selected = string.Equals(projectionStandard, "ThirdAngle", StringComparison.OrdinalIgnoreCase)
                ? thirdAngleTemplatePath
                : firstAngleTemplatePath;
            if (!string.IsNullOrWhiteSpace(selected))
            {
                return selected;
            }

            return ResolveDefaultAutoDrawingTemplatePath(projectionStandard);
        }

        private static Dictionary<string, string> LoadKeyValueFile(string path)
        {
            Dictionary<string, string> map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
            {
                return map;
            }

            foreach (string rawLine in File.ReadAllLines(path))
            {
                if (string.IsNullOrWhiteSpace(rawLine))
                {
                    continue;
                }

                int index = rawLine.IndexOf('=');
                if (index <= 0)
                {
                    continue;
                }

                string key = rawLine.Substring(0, index).Trim();
                string value = rawLine.Substring(index + 1).Trim();
                map[key] = value;
            }

            return map;
        }

        private static JsonObject LoadJsonObject(string path)
        {
            if (!File.Exists(path))
            {
                return new JsonObject();
            }

            try
            {
                JsonNode? node = JsonNode.Parse(File.ReadAllText(path, Encoding.UTF8));
                return node as JsonObject ?? new JsonObject();
            }
            catch
            {
                return new JsonObject();
            }
        }

        private static string ReadTemplatePathFromJson(string path, string propertyName)
        {
            if (!File.Exists(path))
            {
                return string.Empty;
            }

            try
            {
                JsonNode? node = JsonNode.Parse(File.ReadAllText(path, Encoding.UTF8));
                JsonObject? obj = node as JsonObject;
                return obj?[propertyName]?.GetValue<string>() ?? string.Empty;
            }
            catch
            {
                return string.Empty;
            }
        }

        private static bool? ReadBooleanFromJson(string path, string propertyName)
        {
            if (!File.Exists(path))
            {
                return null;
            }

            try
            {
                JsonNode? node = JsonNode.Parse(File.ReadAllText(path, Encoding.UTF8));
                JsonObject? obj = node as JsonObject;
                JsonNode? valueNode = obj?[propertyName];
                if (valueNode == null)
                {
                    return null;
                }

                if (valueNode is JsonValue jsonValue)
                {
                    if (jsonValue.TryGetValue<bool>(out bool boolValue))
                    {
                        return boolValue;
                    }

                    if (jsonValue.TryGetValue<string>(out string? stringValue))
                    {
                        if (bool.TryParse(stringValue, out bool parsedBool))
                        {
                            return parsedBool;
                        }

                        if (int.TryParse(stringValue, out int parsedInt))
                        {
                            return parsedInt != 0;
                        }
                    }
                }
            }
            catch
            {
            }

            return null;
        }

        private static string GetValue(Dictionary<string, string> map, string key, string fallback)
        {
            return map.TryGetValue(key, out string? value) ? value : fallback;
        }

        private static string LoadRequestOrPersistedAutoDrawingSetting(
            Dictionary<string, string> map,
            string dedicatedPropertyName,
            string mirroredPropertyName,
            string fallback)
        {
            string requestValue = GetValue(map, dedicatedPropertyName, string.Empty);
            if (string.IsNullOrWhiteSpace(requestValue))
            {
                requestValue = GetValue(map, mirroredPropertyName, string.Empty);
            }

            return string.IsNullOrWhiteSpace(requestValue)
                ? LoadPersistedAutoDrawingSetting(dedicatedPropertyName, mirroredPropertyName, fallback)
                : requestValue;
        }

        private static string LoadPersistedAutoDrawingSetting(string dedicatedPropertyName, string mirroredPropertyName, string fallback)
        {
            string deployRoot = ResolveDeployRoot();
            string configDirectory = Path.Combine(deployRoot, "config");
            string autoDrawingSettingsPath = Path.Combine(configDirectory, AutoDrawingSettingsFileName);
            string dedicatedValue = ReadTemplatePathFromJson(autoDrawingSettingsPath, dedicatedPropertyName);
            if (!string.IsNullOrWhiteSpace(dedicatedValue))
            {
                return dedicatedValue;
            }

            string toolboxSettingsPath = Path.Combine(configDirectory, ToolboxSettingsFileName);
            string mirroredValue = ReadTemplatePathFromJson(toolboxSettingsPath, mirroredPropertyName);
            return string.IsNullOrWhiteSpace(mirroredValue) ? fallback : mirroredValue;
        }

        private static bool LoadPersistedAutoDrawingBoolean(string dedicatedPropertyName, string mirroredPropertyName, bool fallback)
        {
            bool? value = LoadPersistedAutoDrawingBooleanInternal(dedicatedPropertyName, mirroredPropertyName);
            return value ?? fallback;
        }

        private static string LoadPersistedOneClickColorString(string propertyName, string fallback)
        {
            string path = ResolveOneClickColorSettingsPath();
            string value = ReadTemplatePathFromJson(path, propertyName);
            return string.IsNullOrWhiteSpace(value) ? fallback : value;
        }

        private static string LoadPersistedOneClickColorString(string propertyName, string legacyPropertyName, string fallback)
        {
            string path = ResolveOneClickColorSettingsPath();
            string value = ReadTemplatePathFromJson(path, propertyName);
            if (string.IsNullOrWhiteSpace(value) && !string.IsNullOrWhiteSpace(legacyPropertyName))
            {
                value = ReadTemplatePathFromJson(path, legacyPropertyName);
            }

            return string.IsNullOrWhiteSpace(value) ? fallback : value;
        }

        private static bool LoadPersistedOneClickColorBoolean(string propertyName, bool fallback)
        {
            bool? value = ReadBooleanFromJson(ResolveOneClickColorSettingsPath(), propertyName);
            return value ?? fallback;
        }

        private static bool LoadPersistedOneClickColorBoolean(string propertyName, string legacyPropertyName, bool fallback)
        {
            string path = ResolveOneClickColorSettingsPath();
            bool? value = ReadBooleanFromJson(path, propertyName);
            if (!value.HasValue && !string.IsNullOrWhiteSpace(legacyPropertyName))
            {
                value = ReadBooleanFromJson(path, legacyPropertyName);
            }

            return value ?? fallback;
        }

        private static string ResolveOneClickColorSettingsPath()
        {
            string deployRoot = ResolveDeployRoot();
            string configDirectory = Path.Combine(deployRoot, "config");
            return Path.Combine(configDirectory, OneClickColorSettingsFileName);
        }

        private static string NormalizeOneClickColorPaletteKind(string value)
        {
            return string.Equals(value, "Dark", StringComparison.OrdinalIgnoreCase) ? "Dark" : "Light";
        }

        private static bool? LoadPersistedAutoDrawingBooleanInternal(string dedicatedPropertyName, string mirroredPropertyName)
        {
            string deployRoot = ResolveDeployRoot();
            string configDirectory = Path.Combine(deployRoot, "config");
            string autoDrawingSettingsPath = Path.Combine(configDirectory, AutoDrawingSettingsFileName);
            bool? dedicatedValue = ReadBooleanFromJson(autoDrawingSettingsPath, dedicatedPropertyName);
            if (dedicatedValue.HasValue)
            {
                return dedicatedValue;
            }

            string toolboxSettingsPath = Path.Combine(configDirectory, ToolboxSettingsFileName);
            return ReadBooleanFromJson(toolboxSettingsPath, mirroredPropertyName);
        }

        private static bool GetBool(Dictionary<string, string> map, string key, bool fallback)
        {
            if (!map.TryGetValue(key, out string? value))
            {
                return fallback;
            }

            if (bool.TryParse(value, out bool parsedBool))
            {
                return parsedBool;
            }

            if (int.TryParse(value, out int parsedInt))
            {
                return parsedInt != 0;
            }

            return fallback;
        }

        private static void Append(StringBuilder builder, string key, object? value)
        {
            builder.Append(key);
            builder.Append('=');
            builder.Append(value?.ToString() ?? string.Empty);
            builder.AppendLine();
        }

        private static string ResolveDeployRoot()
        {
            string? explicitRoot = Environment.GetEnvironmentVariable("ZHAOFU_DEPLOY_ROOT");
            if (!string.IsNullOrWhiteSpace(explicitRoot) && Directory.Exists(explicitRoot))
            {
                return explicitRoot;
            }

            if (Directory.Exists(WorkspaceDeployRoot))
            {
                return WorkspaceDeployRoot;
            }

            DirectoryInfo current = new DirectoryInfo(AppContext.BaseDirectory);
            DirectoryInfo? applicationDirectory = current.Parent;
            if (applicationDirectory != null &&
                string.Equals(applicationDirectory.Name, "application", StringComparison.OrdinalIgnoreCase))
            {
                DirectoryInfo? deployRoot = applicationDirectory.Parent;
                return deployRoot != null ? deployRoot.FullName : current.FullName;
            }

            return current.FullName;
        }
    }
}
