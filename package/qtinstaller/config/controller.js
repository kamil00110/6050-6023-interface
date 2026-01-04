function Controller() {
    // Enable verbose installer output for debugging
    installer.setValue("verbose", "true");
    
    // Clean up stale lock files that cause "cannot initialize cache" errors
    if (installer.isUpdater() || installer.isPackageManager()) {
        try {
            var homeDir = installer.value("HomeDir");
            var lockFile = homeDir + "/AppData/Local/cache/qt-installer-framework/135517b1-2668-3c27-9010-5dab869c084f/cache.lock";
            
            if (installer.fileExists(lockFile)) {
                console.log("Removing stale lock file: " + lockFile);
                installer.execute("cmd", ["/c", "del", "/f", "/q", lockFile.replace(/\//g, "\\")]);
            }
        } catch(e) {
            console.log("Could not remove lock file: " + e);
        }
    }
    
    // Check if Traintastic is already installed
    if (installer.isInstaller()) {
        try {
            var result = installer.execute("reg", ["query", 
                "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
                "/v", "InstallLocation"]);
            
            if (result && result.length > 0) {
                var output = result[0];
                var match = output.match(/InstallLocation\s+REG_SZ\s+(.+)/);
                
                if (match && match[1]) {
                    var existingInstall = match[1].trim();
                    var maintenanceTool = existingInstall + "\\TraintasticMaintenanceTool.exe";
                    
                    if (installer.fileExists(maintenanceTool)) {
                        console.log("Existing installation found at: " + existingInstall);
                        
                        var button = QMessageBox.question(
                            "existingInstallation",
                            "Traintastic Already Installed",
                            "Traintastic is already installed.\n\n" +
                            "Click 'Yes' to open the Maintenance Tool to:\n" +
                            "• Update to a newer version\n" +
                            "• Modify components\n" +
                            "• Repair installation\n" +
                            "• Uninstall\n\n" +
                            "Click 'No' for new installation (not recommended).",
                            QMessageBox.Yes | QMessageBox.No
                        );
                        
                        if (button === QMessageBox.Yes) {
                            installer.execute(maintenanceTool, []);
                            gui.clickButton(buttons.CancelButton);
                            return;
                        }
                    }
                }
            }
        } catch(e) {
            console.log("No existing installation detected");
        }
    }
    
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
    if (widget != null && widget.TargetDirectoryLineEdit) {
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
