using System;
using System.Windows;

namespace GonCaBiaoZuUI;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        string sessionId = string.Empty;
        string ipcDir = string.Empty;
        long nxHwnd = 0;

        for (int i = 0; i < e.Args.Length; i++)
        {
            string arg = e.Args[i];
            if (arg.Equals("--session", StringComparison.OrdinalIgnoreCase) && i + 1 < e.Args.Length)
            {
                sessionId = e.Args[++i];
            }
            else if (arg.Equals("--dir", StringComparison.OrdinalIgnoreCase) && i + 1 < e.Args.Length)
            {
                ipcDir = e.Args[++i];
            }
            else if (arg.Equals("--nx-hwnd", StringComparison.OrdinalIgnoreCase) && i + 1 < e.Args.Length)
            {
                long.TryParse(e.Args[++i], out nxHwnd);
            }
        }

        var win = new MainWindow(sessionId, ipcDir, new IntPtr(nxHwnd));
        MainWindow = win;
        win.Show();
    }
}
