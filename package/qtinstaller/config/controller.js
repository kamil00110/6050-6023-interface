function Controller() {
    installer.setValue("verbose", "true");
    
    // Clean up stale lock files
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
        
        // Add custom ServerOptions page dynamically
        console.log("Adding ServerOptionsPage to installer");
        installer.addWizardPageItem(null, "ServerOptionsPage", QInstaller.ComponentSelection);
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
    // Read saved component selection from registry
    var savedSelection = "";
    try {
        var result = installer.execute("reg", ["query",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
            "/v", "Components"]);
        
        if (result && result.length > 0) {
            var match = result[0].match(/Components\s+REG_SZ\s+(.+)/);
            if (match && match[1]) {
                savedSelection = match[1].trim();
                console.log("Saved component selection: " + savedSelection);
            }
        }
    } catch(e) {
        console.log("No saved component selection found");
    }
    
    var widget = gui.currentPageWidget();
    
    if (widget != null) {
        // Set custom title and description
        if (widget.CategoryGroupBox) {
            widget.CategoryGroupBox.title = "Select Installation Type";
        }
        
        var serverComponent = installer.componentByName("org.traintastic.server");
        var clientComponent = installer.componentByName("org.traintastic.client");
        
        // Apply saved selection or default
        if (savedSelection === "ClientOnly") {
            if (serverComponent) serverComponent.setValue("Default", "false");
            if (clientComponent) clientComponent.setValue("Default", "true");
        } else {
            // Default to both (ClientAndServer)
            if (serverComponent) serverComponent.setValue("Default", "true");
            if (clientComponent) clientComponent.setValue("Default", "true");
        }
        
        // Select defaults
        widget.selectDefault();
    }
}

// NEW: Server Options Page - appears after component selection
Controller.prototype.ServerOptionsPageCallback = function() {
    console.log("ServerOptionsPage callback triggered");
    
    var widget = gui.currentPageWidget();
    if (!widget) {
        console.log("ERROR: No widget for ServerOptionsPage");
        return;
    }
    
    // Check if server component is selected
    var serverComponent = installer.componentByName("org.traintastic.server");
    var serverSelected = serverComponent && serverComponent.installationRequested();
    
    console.log("Server component selected: " + serverSelected);
    
    if (!serverSelected) {
        // Skip this page if server is not selected
        console.log("Server not selected, skipping options page");
        gui.clickButton(buttons.NextButton);
        return;
    }
    
    console.log("Creating ServerOptions UI...");
    
    // Set page title using the widget's properties
    widget.setTitle("Server Installation Options");
    widget.setSubTitle("Select additional options for Traintastic Server installation");
    
    // Create main layout
    var layout = new QVBoxLayout();
    
    // Add description
    var descLabel = new QLabel(widget);
    descLabel.text = "Choose which additional features you want to install:";
    descLabel.wordWrap = true;
    layout.addWidget(descLabel);
    
    layout.addSpacing(20);
    
    // Shortcuts group
    var shortcutsGroup = new QGroupBox(widget);
    shortcutsGroup.title = "Shortcuts";
    var shortcutsLayout = new QVBoxLayout();
    
    var cbDesktop = new QCheckBox(shortcutsGroup);
    cbDesktop.objectName = "desktopShortcut";
    cbDesktop.text = "Create desktop shortcuts for Server and Client";
    cbDesktop.checked = false;
    shortcutsLayout.addWidget(cbDesktop);
    
    var cbTaskbar = new QCheckBox(shortcutsGroup);
    cbTaskbar.objectName = "taskbarShortcut";
    cbTaskbar.text = "Pin Traintastic Server to taskbar";
    cbTaskbar.checked = false;
    shortcutsLayout.addWidget(cbTaskbar);
    
    var cbManual = new QCheckBox(shortcutsGroup);
    cbManual.objectName = "manualShortcut";
    cbManual.text = "Create shortcut to Traintastic Manual";
    cbManual.checked = false;
    shortcutsLayout.addWidget(cbManual);
    
    shortcutsGroup.setLayout(shortcutsLayout);
    layout.addWidget(shortcutsGroup);
    layout.addSpacing(15);
    
    // Startup group
    var startupGroup = new QGroupBox(widget);
    startupGroup.title = "Startup";
    var startupLayout = new QVBoxLayout();
    
    var cbStartup = new QCheckBox(startupGroup);
    cbStartup.objectName = "startOnStartup";
    cbStartup.text = "Start Traintastic Server automatically when Windows starts";
    cbStartup.checked = false;
    startupLayout.addWidget(cbStartup);
    
    startupGroup.setLayout(startupLayout);
    layout.addWidget(startupGroup);
    layout.addSpacing(15);
    
    // Windows Firewall group
    var firewallGroup = new QGroupBox(widget);
    firewallGroup.title = "Windows Firewall";
    var firewallLayout = new QVBoxLayout();
    
    var cbFirewall = new QCheckBox(firewallGroup);
    cbFirewall.objectName = "firewallTraintastic";
    cbFirewall.text = "Allow Traintastic client connections (TCP/UDP port 5740)";
    cbFirewall.checked = false;
    firewallLayout.addWidget(cbFirewall);
    
    var cbWLAN = new QCheckBox(firewallGroup);
    cbWLAN.objectName = "firewallWLANmaus";
    cbWLAN.text = "Allow WLANmaus/Z21 protocol (UDP port 21105)";
    cbWLAN.checked = false;
    firewallLayout.addWidget(cbWLAN);
    
    firewallGroup.setLayout(firewallLayout);
    layout.addWidget(firewallGroup);
    
    // Add stretch
    layout.addStretch();
    
    widget.setLayout(layout);
    
    console.log("ServerOptions UI created successfully");
}

Controller.prototype.ReadyForInstallationPageCallback = function() {
    // Save component selection to registry
    var serverComponent = installer.componentByName("org.traintastic.server");
    var clientComponent = installer.componentByName("org.traintastic.client");
    
    var selection = "";
    var serverSelected = serverComponent && serverComponent.installationRequested();
    var clientSelected = clientComponent && clientComponent.installationRequested();
    
    if (serverSelected && clientSelected) {
        selection = "ClientAndServer";
    } else if (clientSelected) {
        selection = "ClientOnly";
    }
    
    if (selection) {
        console.log("Saving component selection: " + selection);
        installer.setValue("ComponentSelection", selection);
    }
    
    // Read and save server options checkbox states
    if (serverSelected && installer.isInstaller()) {
        console.log("Reading ServerOptions checkbox states...");
        
        var serverOptionsPage = gui.pageWidgetByObjectName("ServerOptionsPage");
        if (serverOptionsPage) {
            var cbDesktop = serverOptionsPage.findChild("desktopShortcut");
            var cbTaskbar = serverOptionsPage.findChild("taskbarShortcut");
            var cbManual = serverOptionsPage.findChild("manualShortcut");
            var cbStartup = serverOptionsPage.findChild("startOnStartup");
            var cbFirewall = serverOptionsPage.findChild("firewallTraintastic");
            var cbWLAN = serverOptionsPage.findChild("firewallWLANmaus");
            
            // Save checkbox states to installer values
            if (cbDesktop) {
                installer.setValue("ServerOptions_desktopShortcut_checked", cbDesktop.checked ? "true" : "false");
                console.log("  Desktop shortcut: " + cbDesktop.checked);
            }
            if (cbTaskbar) {
                installer.setValue("ServerOptions_taskbarShortcut_checked", cbTaskbar.checked ? "true" : "false");
                console.log("  Taskbar shortcut: " + cbTaskbar.checked);
            }
            if (cbManual) {
                installer.setValue("ServerOptions_manualShortcut_checked", cbManual.checked ? "true" : "false");
                console.log("  Manual shortcut: " + cbManual.checked);
            }
            if (cbStartup) {
                installer.setValue("ServerOptions_startOnStartup_checked", cbStartup.checked ? "true" : "false");
                console.log("  Start on startup: " + cbStartup.checked);
            }
            if (cbFirewall) {
                installer.setValue("ServerOptions_firewallTraintastic_checked", cbFirewall.checked ? "true" : "false");
                console.log("  Firewall Traintastic: " + cbFirewall.checked);
            }
            if (cbWLAN) {
                installer.setValue("ServerOptions_firewallWLANmaus_checked", cbWLAN.checked ? "true" : "false");
                console.log("  Firewall WLANmaus: " + cbWLAN.checked);
            }
        } else {
            console.log("WARNING: Could not find ServerOptionsPage");
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
