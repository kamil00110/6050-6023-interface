function Controller() {
    installer.autoRejectMessageBoxes();
    installer.installationFinished.connect(function() {
        var serverPkg = installer.packageByName("server");
        if (serverPkg.isInstalled) {
            var serverExe = installer.value("TargetDir") + "/server/traintastic-server.exe";
            installer.executeDetached("cmd.exe", ["/c", 'netsh advfirewall firewall add rule name="Traintastic server (TCP)" dir=in program="' + serverExe + '" protocol=TCP localport=5740 action=allow']);
            installer.executeDetached("cmd.exe", ["/c", 'netsh advfirewall firewall add rule name="Traintastic server (UDP)" dir=in program="' + serverExe + '" protocol=UDP localport=5740 action=allow']);
            installer.executeDetached("cmd.exe", ["/c", 'netsh advfirewall firewall add rule name="Traintastic server (WLANmaus/Z21)" dir=in program="' + serverExe + '" protocol=UDP localport=21105 action=allow']);
        }
    });
}
