using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Threading;

namespace AutoCreateThreeViewsUI;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        AppDomain.CurrentDomain.UnhandledException += (_, args) =>
        {
            WriteCrashLog(args.ExceptionObject as Exception);
        };
        DispatcherUnhandledException += (_, args) =>
        {
            WriteCrashLog(args.Exception);
            args.Handled = false;
        };

        base.OnStartup(e);
    }

    private static void WriteCrashLog(Exception? exception)
    {
        try
        {
            string path = Path.Combine(AppContext.BaseDirectory, "AutoCreateThreeViewsUI.crash.log");
            StringBuilder builder = new();
            builder.AppendLine(DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
            builder.AppendLine(exception?.ToString() ?? "Unknown UI exception.");
            builder.AppendLine();
            File.AppendAllText(path, builder.ToString(), Encoding.UTF8);
        }
        catch
        {
        }
    }
}
