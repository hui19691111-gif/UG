using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;

namespace Zhaofu.Nx.SheetMetal.BendFactorBatch.Shared
{
    [DataContract]
    public sealed class BendFactorBatchRequest
    {
        [DataMember(Order = 1)]
        public string WindowTitle { get; set; } = "昭福钣金 - 批量修改折弯系数";

        [DataMember(Order = 2)]
        public string Stage { get; set; } = "configure";

        [DataMember(Order = 3)]
        public string DisplayPartName { get; set; } = string.Empty;

        [DataMember(Order = 4)]
        public string CurrentPartPath { get; set; } = string.Empty;

        [DataMember(Order = 5)]
        public string ConfigPath { get; set; } = string.Empty;

        [DataMember(Order = 6)]
        public string ResponsePath { get; set; } = string.Empty;

        [DataMember(Order = 7)]
        public string ResultsDirectory { get; set; } = string.Empty;

        [DataMember(Order = 8)]
        public BendFactorBatchConfig Config { get; set; } = new BendFactorBatchConfig();

        [DataMember(Order = 9)]
        public List<BendPreviewRecord> PreviewRows { get; set; } = new List<BendPreviewRecord>();
    }

    [DataContract]
    public sealed class BendFactorBatchConfig
    {
        [DataMember(Order = 1)]
        public string Mode { get; set; } = "folder";

        [DataMember(Order = 2)]
        public string FolderPath { get; set; } = string.Empty;

        [DataMember(Order = 3)]
        public bool Recursive { get; set; } = true;

        [DataMember(Order = 4)]
        public bool IncludeAssemblyChildrenWhenCurrent { get; set; } = true;

        [DataMember(Order = 5)]
        public bool AutoConvertToSheetmetal { get; set; } = true;

        [DataMember(Order = 6)]
        public string BaseFaceDirectionPreference { get; set; } = "PreferUpBends";

        [DataMember(Order = 7)]
        public string FixedFaceColor { get; set; } = "#FFD400";

        [DataMember(Order = 8)]
        public double MaxDeductionDeviationMm { get; set; } = 0.02;

        [DataMember(Order = 9)]
        public string ImportExportCoefficientTablePath { get; set; } = string.Empty;

        [DataMember(Order = 10)]
        public LargeArcRuleSettingsRecord LargeArcRuleSettings { get; set; } = new LargeArcRuleSettingsRecord();

        [DataMember(Order = 11)]
        public List<ConditionTableRowRecord> ConditionRows { get; set; } = new List<ConditionTableRowRecord>();

        [DataMember(Order = 12)]
        public List<CoefficientTableRowRecord> CoefficientRows { get; set; } = new List<CoefficientTableRowRecord>();

        [DataMember(Order = 13)]
        public bool SkipSmallBodyByWidthHeight { get; set; } = false;

        [DataMember(Order = 14)]
        public double MinBodyLengthForProcessing { get; set; } = 0.0;

        [DataMember(Order = 15)]
        public double MinBodyWidthForProcessing { get; set; } = 0.0;

        [DataMember(Order = 16)]
        public double MaxBodyLengthForSmallBodyFiltering { get; set; } = 40.0;

        [DataMember(Order = 17)]
        public List<BendRuleRecord> Rules { get; set; } = new List<BendRuleRecord>();

        [DataMember(Order = 18)]
        public bool FilterBodiesByLayerRange { get; set; } = true;

        [DataMember(Order = 19)]
        public int StartBodyLayer { get; set; } = 1;

        [DataMember(Order = 20)]
        public int EndBodyLayer { get; set; } = 99;

        [DataMember(Order = 21)]
        public bool ProcessLargestBodyOnlyPerLayer { get; set; } = false;

        [DataMember(Order = 22)]
        public bool MarkerLineFaceUp { get; set; } = false;

        [DataMember(Order = 23)]
        public bool AutoSaveAfterRun { get; set; } = true;

        [DataMember(Order = 24)]
        public bool ApplyFixedFaceColor { get; set; } = true;
    }

    [DataContract]
    public sealed class ConditionTableRowRecord
    {
        [DataMember(Order = 1)]
        public string ConditionKey { get; set; } = string.Empty;

        [DataMember(Order = 2)]
        public string AngleLabel { get; set; } = string.Empty;

        [DataMember(Order = 3)]
        public string SmallArcCode { get; set; } = string.Empty;

        [DataMember(Order = 4)]
        public string LargeArcCode { get; set; } = string.Empty;
    }

    [DataContract]
    public sealed class CoefficientTableRowRecord
    {
        [DataMember(Order = 1)]
        public string Material { get; set; } = "材质 <未指定>";

        [DataMember(Order = 2)]
        public double Thickness { get; set; }

        [DataMember(Order = 3)]
        public double? Q1 { get; set; }

        [DataMember(Order = 4)]
        public double? Q2 { get; set; }

        [DataMember(Order = 5)]
        public double? Q3 { get; set; }

        [DataMember(Order = 6)]
        public double? K1 { get; set; }

        [DataMember(Order = 7)]
        public double? K2 { get; set; }

        [DataMember(Order = 8)]
        public double? K3 { get; set; }

        [DataMember(Order = 9)]
        public double? A1 { get; set; }

        [DataMember(Order = 10)]
        public double? A2 { get; set; }

        [DataMember(Order = 11)]
        public double? A3 { get; set; }

        [DataMember(Order = 12)]
        public double? R { get; set; }

        [DataMember(Order = 13)]
        public string F1 { get; set; } = "f1";

        [DataMember(Order = 14)]
        public string F2 { get; set; } = "f1";
    }

    [DataContract]
    public sealed class LargeArcRuleSettingsRecord
    {
        [DataMember(Order = 1)]
        public bool UseAbsoluteRadiusThreshold { get; set; } = true;

        [DataMember(Order = 2)]
        public double AbsoluteRadiusThreshold { get; set; } = 7.0;

        [DataMember(Order = 3)]
        public bool UseRadiusThicknessRatio { get; set; }

        [DataMember(Order = 4)]
        public double RadiusThicknessRatioThreshold { get; set; } = 5.0;

        [DataMember(Order = 5)]
        public double RightAngleRadiusThreshold { get; set; } = 5.0;

        [DataMember(Order = 6)]
        public double RightAngleFallbackKFactor { get; set; } = 0.5;

        [DataMember(Order = 7)]
        public double AcuteAngleRadiusThreshold { get; set; } = 5.0;

        [DataMember(Order = 8)]
        public double AcuteAngleFallbackKFactor { get; set; } = 0.5;
    }

    [DataContract]
    public sealed class BendRuleRecord
    {
        [DataMember(Order = 1)]
        public bool Enabled { get; set; } = true;

        [DataMember(Order = 2)]
        public string RuleName { get; set; } = string.Empty;

        [DataMember(Order = 3)]
        public double? AngleMinDeg { get; set; }

        [DataMember(Order = 4)]
        public double? AngleMaxDeg { get; set; }

        [DataMember(Order = 5)]
        public double? ThicknessMin { get; set; }

        [DataMember(Order = 6)]
        public double? ThicknessMax { get; set; }

        [DataMember(Order = 7)]
        public double? InnerRadiusMin { get; set; }

        [DataMember(Order = 8)]
        public double? InnerRadiusMax { get; set; }

        [DataMember(Order = 9)]
        public string Material { get; set; } = string.Empty;

        [DataMember(Order = 10)]
        public string Method { get; set; } = "KFactor";

        [DataMember(Order = 11)]
        public double Value { get; set; }

        [DataMember(Order = 12)]
        public double? RadiusOverride { get; set; }

        [DataMember(Order = 13)]
        public bool IsFallback { get; set; }

        [DataMember(Order = 14)]
        public string Note { get; set; } = string.Empty;
    }

    [DataContract]
    public sealed class BendPreviewRecord
    {
        [DataMember(Order = 1)]
        public bool Apply { get; set; } = true;

        [DataMember(Order = 2)]
        public string FilePath { get; set; } = string.Empty;

        [DataMember(Order = 3)]
        public string FileName { get; set; } = string.Empty;

        [DataMember(Order = 4)]
        public string BodyName { get; set; } = string.Empty;

        [DataMember(Order = 5)]
        public string BendId { get; set; } = string.Empty;

        [DataMember(Order = 6)]
        public int BodyTag { get; set; }

        [DataMember(Order = 7)]
        public int FaceTag { get; set; }

        [DataMember(Order = 8)]
        public double Thickness { get; set; }

        [DataMember(Order = 9)]
        public double InnerRadius { get; set; }

        [DataMember(Order = 10)]
        public double BendAngleDeg { get; set; }

        [DataMember(Order = 11)]
        public double CurrentKFactor { get; set; }

        [DataMember(Order = 12)]
        public string MatchedRuleName { get; set; } = string.Empty;

        [DataMember(Order = 13)]
        public string MatchedMethod { get; set; } = string.Empty;

        [DataMember(Order = 14)]
        public double TargetKFactor { get; set; }

        [DataMember(Order = 15)]
        public double EquivalentDeduction { get; set; }

        [DataMember(Order = 16)]
        public double DeductionDeviationMm { get; set; }

        [DataMember(Order = 17)]
        public string SourceType { get; set; } = string.Empty;

        [DataMember(Order = 18)]
        public bool AutoConverted { get; set; }

        [DataMember(Order = 19)]
        public string FixedFaceId { get; set; } = string.Empty;

        [DataMember(Order = 20)]
        public string FixedFaceDirection { get; set; } = string.Empty;

        [DataMember(Order = 21)]
        public string FixedFaceColor { get; set; } = string.Empty;

        [DataMember(Order = 22)]
        public string Warning { get; set; } = string.Empty;

        [DataMember(Order = 23)]
        public string Material { get; set; } = string.Empty;

        [DataMember(Order = 24)]
        public string BendType { get; set; } = string.Empty;

        [DataMember(Order = 25)]
        public string Result { get; set; } = string.Empty;

        [DataMember(Order = 26)]
        public string StableIdentity { get; set; } = string.Empty;
    }

    [DataContract]
    public sealed class BendFactorBatchResponse
    {
        [DataMember(Order = 1)]
        public string Action { get; set; } = "cancel";

        [DataMember(Order = 2)]
        public BendFactorBatchConfig Config { get; set; } = new BendFactorBatchConfig();

        [DataMember(Order = 3)]
        public List<BendPreviewRecord> PreviewRows { get; set; } = new List<BendPreviewRecord>();
    }

    public static class BendFactorBatchExchange
    {
        public static BendFactorBatchRequest LoadRequest(string path)
        {
            return Deserialize<BendFactorBatchRequest>(path);
        }

        public static void SaveRequest(BendFactorBatchRequest request, string path)
        {
            Serialize(request, path);
        }

        public static BendFactorBatchResponse LoadResponse(string path)
        {
            return Deserialize<BendFactorBatchResponse>(path);
        }

        public static void SaveResponse(BendFactorBatchResponse response, string path)
        {
            Serialize(response, path);
        }

        public static BendFactorBatchConfig LoadConfig(string path)
        {
            return Deserialize<BendFactorBatchConfig>(path);
        }

        public static void SaveConfig(BendFactorBatchConfig config, string path)
        {
            Serialize(config, path);
        }

        private static T Deserialize<T>(string path) where T : class, new()
        {
            if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
            {
                return new T();
            }

            DataContractJsonSerializer serializer = CreateSerializer(typeof(T));
            using (FileStream stream = File.OpenRead(path))
            {
                object value = serializer.ReadObject(stream) ?? new T();
                return value as T ?? new T();
            }
        }

        private static void Serialize<T>(T value, string path) where T : class
        {
            if (value == null)
            {
                throw new ArgumentNullException(nameof(value));
            }

            string directory = Path.GetDirectoryName(path) ?? string.Empty;
            if (!string.IsNullOrWhiteSpace(directory))
            {
                Directory.CreateDirectory(directory);
            }

            DataContractJsonSerializer serializer = CreateSerializer(typeof(T));
            using (MemoryStream stream = new MemoryStream())
            {
                serializer.WriteObject(stream, value);
                File.WriteAllBytes(path, stream.ToArray());
            }
        }

        private static DataContractJsonSerializer CreateSerializer(Type type)
        {
            return new DataContractJsonSerializer(type, new DataContractJsonSerializerSettings
            {
                UseSimpleDictionaryFormat = true
            });
        }
    }
}
