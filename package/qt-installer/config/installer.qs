function Controller() {
    installer.installationFinished.connect(function() {
        // Post-install VC++ redistributable
        var vcRedist = installer.value("TargetDir") + "/client/build/Release/vc_redist.x64.exe";
        installer.execute(vcRedist, ["/quiet", "/norestart"]);

        // Firewall rules
        var serverExe = installer.value("TargetDir") + "/server/traintastic-server.exe";
        installer.execute("netsh.exe", ["advfirewall", "firewall", "add", "rule", "name=Traintastic server (TCP)", "dir=in", "program=" + serverExe, "protocol=TCP", "localport=5740", "action=allow"]);
        installer.execute("netsh.exe", ["advfirewall", "firewall", "add", "rule", "name=Traintastic server (UDP)", "dir=in", "program=" + serverExe, "protocol=UDP", "localport=5740", "action=allow"]);
        installer.execute("netsh.exe", ["advfirewall", "firewall", "add", "rule", "name=Traintastic server (WLANmaus/Z21)", "dir=in", "program=" + serverExe, "protocol=UDP", "localport=21105", "action=allow"]);

        // Post-install JSON
        var fs = require("fs");
        var path = installer.value("TargetDir") + "/server/settings.json";
        if (!fs.existsSync(path)) {
            fs.writeFileSync(path, JSON.stringify({language: installer.value("TargetLanguage")}));
        }
    });

    // Custom component page
    installer.addWizardPage("Components", "Select Components", "Choose whether to install Client, Server, or Both", function(page) {
        var clientRadio = page.createRadioButton("Client only");
        var serverRadio = page.createRadioButton("Server only");
        var bothRadio = page.createRadioButton("Client and Server");
        bothRadio.setChecked(true);

        page.radioButtonsChanged.connect(function() {
            installer.setValue("InstallClient", clientRadio.isChecked() || bothRadio.isChecked());
            installer.setValue("InstallServer", serverRadio.isChecked() || bothRadio.isChecked());
        });
    });
}
