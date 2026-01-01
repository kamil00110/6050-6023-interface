function Controller() {
    installer.autoRejectMessageBoxes();
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, true);
    installer.setDefaultPageVisible(QInstaller.TargetDirectory, true);
    installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, true);
    installer.setDefaultPageVisible(QInstaller.Introduction, true);
    installer.setDefaultPageVisible(QInstaller.LicenseCheck, true);
}

function ComponentPage() {
    var page = gui.pageWidgetByObjectName("ComponentSelectionPage");
    if (page) {
        // pre-check Client+Server by default
        var client = page.findChild("Client");
        var server = page.findChild("Server");
        if (client && server) {
            client.checked = true;
            server.checked = true;
        }
    }
}

Controller.prototype.WelcomePageCallback = function() {
    // optional: show intro message
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    // enforce target dir or default
}

Controller.prototype.InstallationFinishedCallback = function() {
    // post-install: create JSON language file for server
    var serverSettingsFile = installer.value("TargetDir") + "/server/settings.json";
    if (!installer.fileExists(serverSettingsFile)) {
        var lang = installer.environmentVariable("LANG") || "en-us";
        installer.execute("cmd.exe", ["/c", 'echo {"language":"' + lang + '"} > "' + serverSettingsFile + '"']);
    }
}
