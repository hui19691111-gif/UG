using System.IO;
using System.Windows;

namespace MinXiBiaoUI;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        try
        {
            base.OnStartup(e);
        }
        catch (Exception ex)
        {
            File.WriteAllText(Path.Combine(AppContext.BaseDirectory, "MinXiBiaoUI.crash.log"), ex.ToString());
            throw;
        }
    }
}
