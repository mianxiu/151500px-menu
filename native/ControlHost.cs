using System;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

sealed class ControlHost : Form
{
    const string Title = "151500px-menu — 后台管理";
    readonly WebView2 view = new WebView2 { Dock = DockStyle.Fill };
    readonly Uri address;
    readonly string profile;
    int result = 0;

    [DllImport("user32.dll")]
    static extern bool SetProcessDpiAwarenessContext(IntPtr context);

    [DllImport("user32.dll")]
    static extern uint GetDpiForSystem();

    [STAThread]
    static int Main(string[] args)
    {
        Uri address;
        int parentId;
        if (args.Length != 3 || !Uri.TryCreate(args[0], UriKind.Absolute, out address)
            || address.Scheme != "http" || address.Host != "127.0.0.1"
            || !int.TryParse(args[2], out parentId))
            return 2;
        try
        {
            if (string.IsNullOrEmpty(CoreWebView2Environment.GetAvailableBrowserVersionString()))
                return 3;
        }
        catch (WebView2RuntimeNotFoundException)
        {
            return 3;
        }

        SetProcessDpiAwarenessContext(new IntPtr(-4));
        Application.EnableVisualStyles();
        using (var form = new ControlHost(address, args[1]))
        using (var parent = Process.GetProcessById(parentId))
        using (var timer = new Timer { Interval = 500 })
        {
            // Stop the host when its originating service exits, even during startup.
            timer.Tick += (sender, e) => {
                if (parent.HasExited)
                    form.Close();
            };
            timer.Start();
            Application.Run(form);
            return form.result;
        }
    }

    ControlHost(Uri address, string profile)
    {
        this.address = address;
        this.profile = profile;
        StartPosition = FormStartPosition.CenterScreen;
        var dpi = GetDpiForSystem();
        Size = new Size((int)(760 * dpi / 96), (int)(340 * dpi / 96));
        Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
        Controls.Add(view);
        Shown += async (sender, e) => await Initialize();
    }

    async Task Initialize()
    {
        try
        {
            var options = new CoreWebView2EnvironmentOptions { AreBrowserExtensionsEnabled = false };
            var environment = await CoreWebView2Environment.CreateAsync(null, profile, options);
            if (IsDisposed)
                return;
            await view.EnsureCoreWebView2Async(environment);
            var browser = view.CoreWebView2;
            browser.Settings.IsWebMessageEnabled = false;
            browser.NavigationStarting += (sender, e) => {
                Uri target;
                e.Cancel = !Uri.TryCreate(e.Uri, UriKind.Absolute, out target)
                    || target.GetLeftPart(UriPartial.Authority) != address.GetLeftPart(UriPartial.Authority);
            };
            browser.NewWindowRequested += (sender, e) => {
                e.Handled = true;
                Uri target;
                if (e.IsUserInitiated && Uri.TryCreate(e.Uri, UriKind.Absolute, out target)
                    && target.Scheme == "https")
                {
                    try { Process.Start(target.AbsoluteUri); }
                    catch (System.ComponentModel.Win32Exception) { }
                }
            };
            browser.Navigate(address.AbsoluteUri);
            Text = Title;
        }
        catch (Exception)
        {
            result = 4;
            Close();
        }
    }
}
