using System;
using System.Windows;

namespace ZhsmToolboxSettingsUI
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            try
            {
                string requestPath = string.Empty;
                string responsePath = string.Empty;
                bool openBendFactorOnly = false;
                string bendFactorConfigRelativePath = BendFactorSettingsWindow.DefaultConfigRelativePath;

                string[] args = e.Args ?? Array.Empty<string>();
                for (int i = 0; i < args.Length; i++)
                {
                    if (string.Equals(args[i], "--request", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                    {
                        requestPath = args[i + 1];
                    }
                    else if (string.Equals(args[i], "--response", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                    {
                        responsePath = args[i + 1];
                    }
                    else if (string.Equals(args[i], "--bend-factor", StringComparison.OrdinalIgnoreCase))
                    {
                        openBendFactorOnly = true;
                    }
                    else if (string.Equals(args[i], "--pilian-zhuanbanjin-bend-factor", StringComparison.OrdinalIgnoreCase))
                    {
                        openBendFactorOnly = true;
                        bendFactorConfigRelativePath = @"config\PiLianZuanBanJin_bend_factor_config.json";
                    }
                }

                if (openBendFactorOnly)
                {
                    BendFactorSettingsWindow bendFactorWindow = new BendFactorSettingsWindow(bendFactorConfigRelativePath);
                    MainWindow = bendFactorWindow;
                    bendFactorWindow.Show();
                    return;
                }

                ToolboxSettingsRequest request = ToolboxSettingsExchange.LoadRequest(requestPath);
                MainWindow window = new MainWindow(request, responsePath);
                MainWindow = window;
                window.Show();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "昭福工具设置", MessageBoxButton.OK, MessageBoxImage.Error);
                Shutdown(-1);
            }
        }
    }
}
