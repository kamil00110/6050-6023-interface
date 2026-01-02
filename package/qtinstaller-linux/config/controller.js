function Controller() {
    // Enable verbose installer output for debugging
    installer.setValue("verbose", "true");
    
    // Check if Traintastic is already installed
    var existingInstall = installer.value("HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic\\InstallLocation");
    
    if (installer.isInstaller() && existingInstall && existingInstall !== "") {
        console.log("Existing installation detected at: " + existingInstall);
        
        // Check if maintenance tool exists
        var maintenanceTool = existingInstall + "/TraintasticMaintenanceTool.exe";
        
        if (installer.fileExists(maintenanceTool)) {
            // Launch maintenance tool and exit installer
            console.log("Launching maintenance tool: " + maintenanceTool);
            
            // Show message to user
            var result = QMessageBox.information(
                "existingInstallation",
                "Traintastic Already Installed",
                "Traintastic is already installed on this computer.\n\n" +
                "Click OK to open the Maintenance Tool where you can:\n" +
                "• Update to a newer version\n" +
                "• Modify installed components\n" +
                "• Repair the installation\n" +
                "• Uninstall Traintastic\n\n" +
                "Click Cancel to continue with a new installation (not recommended).",
                QMessageBox.Ok | QMessageBox.Cancel
            );
            
            if (result === QMessageBox.Ok) {
                installer.execute(maintenanceTool, []);
                installer.setDefaultPageVisible(QInstaller.Introduction, false);
                installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
                installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
                installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);
                installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, false);
                installer.setDefaultPageVisible(QInstaller.PerformInstallation, false);
                installer.setDefaultPageVisible(QInstaller.InstallationFinished, false);
                
                // Exit gracefully
                gui.clickButton(buttons.CancelButton);
                return;
            }
        }
    }
    
    // Check if this is an update/maintenance run
    if (installer.isInstaller()) {
        installer.autoRejectMessageBoxes();
        installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes);
        installer.setMessageBoxAutomaticAnswer("stopProcessesForUpdates", QMessageBox.Ignore);
    }
}

Controller.prototype.IntroductionPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        if (installer.isInstaller()) {
            widget.title = "Welcome to Traintastic Installer";
            if (widget.MessageLabel) {
                widget.MessageLabel.setText("This will install Traintastic on your computer.");
            }
        } else if (installer.isUninstaller()) {
            widget.title = "Uninstall Traintastic";
            if (widget.MessageLabel) {
                widget.MessageLabel.setText("This will remove Traintastic from your computer.");
            }
        } else if (installer.isUpdater()) {
            widget.title = "Update Traintastic";
            if (widget.MessageLabel) {
                widget.MessageLabel.setText("This will update Traintastic to the latest version.");
            }
        }
    }
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        widget.TargetDirectoryLineEdit.setText(installer.value("TargetDir"));
    }
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        widget.selectAll();
    }
}

Controller.prototype.FinishedPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        if (installer.isInstaller() || installer.isUpdater()) {
            if (widget.RunItCheckBox) {
                widget.RunItCheckBox.visible = true;
                widget.RunItCheckBox.checked = false;
            }
        } else if (installer.isUninstaller()) {
            if (widget.RunItCheckBox) {
                widget.RunItCheckBox.visible = false;
            }
        }
    }
}
