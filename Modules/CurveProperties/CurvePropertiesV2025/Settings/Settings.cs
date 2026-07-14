using BaseFunction;
using System;
using System.IO;

namespace CurveProperties
{
    /// <summary>
    /// Менеджер настроек и точка доступа к ним
    /// </summary>
    public static class Settings
    {
        private static readonly string ProjectNamespace = typeof(SettingsClass).Namespace;

        private static readonly string SettingsPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "AlzAcadProgramSettings",
            $"{ProjectNamespace}.xml"
        );

        public static SettingsClass Default { get; private set; } = new SettingsClass();

        static Settings()
        {
            Load();
        }

        public static void Load()
        {
            if (BaseXMLClass.GetSerialisationResult(SettingsPath, typeof(SettingsClass), false) is SettingsClass model)
                Default = model;
            else
                Default = new SettingsClass();
        }

        public static void Save()
        {
            BaseXMLClass.SetSerialisationResult(SettingsPath, Default, false);
        }
    }
}



