function Controller() {
    installer.autoRejectMessageBoxes();
    installer.installationFinished.connect(function() {
        // Add firewall rules if server is installed
        if (installer.packageByName("server").isInstalled) {
            var serverExe = installer.value("TargetDir") + "/server/traintastic-server.exe";
            var cmdTCP = 'netsh advfirewall firewall add rule name="Traintastic server (TCP)" dir=in program="' + serverExe + '" protocol=TCP localport=5740 action=allow';
            var cmdUDP = 'netsh advfirewall firewall add rule name="Traintastic server (UDP)" dir=in program="' + serverExe + '" protocol=UDP localport=5740 action=allow';
            var cmdWLAN = 'netsh advfirewall firewall add rule name="Traintastic server (WLANmaus/Z21)" dir=in program="' + serverExe + '" protocol=UDP localport=21105 action=allow';
            console.log("Adding firewall rules...");
            console.log(cmdTCP);
            console.log(cmdUDP);
            console.log(cmdWLAN);
            installer.executeDetached("cmd.exe", ["/c", cmdTCP]);
            installer.executeDetached("cmd.exe", ["/c", cmdUDP]);
            installer.executeDetached("cmd.exe", ["/c", cmdWLAN]);
        }

        // VC++ Redistributable check could be added here
    });
}
