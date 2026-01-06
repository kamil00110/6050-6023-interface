function Controller() {
    installer.setValue("verbose", "true");
    
    // CRITICAL: Clean up lock files more aggressively
    if (installer.isUpdater() || installer.isPackageManager() || installer.isUninstaller()) {
        try {
            var homeDir = installer.value("HomeDir");
            var cacheBase = homeDir + "/AppData/Local/cache/qt-installer-framework";
            var lockFile = cacheBase + "/135517b1-2668-3c27-9010-5dab869c084f/cache.lock";
            
            console.log("Checking for lock file: " + lockFile);
            
            if (installer.fileExists(lockFile)) {
                console.log("Removing stale lock file: " + lockFile);
                
                // Try multiple methods to remove the lock file
                // Method 1: Direct delete
                installer.execute("cmd", ["/c", "del", "/f", "/q", lockFile.replace(/\//g, "\\")]);
                
                // Method 2: If still exists, try taskkill on any Qt IFW processes
                if (installer.fileExists(lockFile)) {
                    console.log("Lock file still exists, killing any running installer processes...");
                    installer.execute("cmd", ["/c", "taskkill", "/F", "/IM", "TraintasticMaintenanceTool.exe", "/T"]);
                    installer.execute("cmd", ["/c", "taskkill", "/F", "/IM", "traintastic-setup*.exe", "/T"]);
                    
                    // Wait a bit
                    installer.execute("cmd", ["/c", "timeout", "/t", "2", "/nobreak"]);
                    
                    // Try delete again
                    installer.execute("cmd", ["/c", "del", "/f", "/q", lockFile.replace(/\//g, "\\")]);
                }
            }
        } catch(e) {
            console.log("Could not remove lock file: " + e);
        }
    }
    
    // Check for existing installation
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
        // Set custom title
        widget.setTitle("Select Components");
        widget.setSubTitle("Choose which components to install");
        
        // Read saved component selection from registry for default selection
        try {
            var result = installer.execute("reg", ["query",
                "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
                "/v", "Components"]);
            
            if (result && result.length > 0) {
                var match = result[0].match(/Components\s+REG_SZ\s+(.+)/);
                if (match && match[1]) {
                    var savedSelection = match[1].trim();
                    console.log("Saved component selection: " + savedSelection);
                    
                    // Apply saved selection
                    var serverComponent = installer.componentByName("org.traintastic.server");
                    var clientComponent = installer.componentByName("org.traintastic.client");
                    
                    if (savedSelection === "ClientOnly") {
                        if (serverComponent) serverComponent.setValue("Default", "false");
                        if (clientComponent) clientComponent.setValue("Default", "true");
                    } else if (savedSelection === "ServerOnly") {
                        if (serverComponent) serverComponent.setValue("Default", "true");
                        if (clientComponent) clientComponent.setValue("Default", "false");
                    } else {
                        // ClientAndServer (default)
                        if (serverComponent) serverComponent.setValue("Default", "true");
                        if (clientComponent) clientComponent.setValue("Default", "true");
                    }
                }
            }
        } catch(e) {
            console.log("No saved component selection found, using defaults");
        }
        
        // Select defaults
        widget.selectDefault();
    }
}

Controller.prototype.ReadyForInstallationPageCallback = function() {
    console.log("Ready for installation");
}

// Allow cancellation on all pages
Controller.prototype.onCurrentPageChanged = function(newPageId) {
    var widget = gui.currentPageWidget();
    if (widget && gui.findChild(widget, "CancelButton")) {
        var cancelButton = gui.findChild(widget, "CancelButton");
        if (cancelButton) {
            cancelButton.setEnabled(true);
            cancelButton.setVisible(true);
        }
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
