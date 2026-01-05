function Component() {
    console.log("Server component constructor called");
    
    // Store checkbox states as component variables
    component.desktopShortcut = false;
    component.taskbarShortcut = false;
    component.manualShortcut = false;
    component.startOnStartup = false;
    component.firewallTraintastic = false;
    component.firewallWLANmaus = false;
}

// Add custom wizard page
Component.prototype.loaded = function() {
    if (installer.isInstaller()) {
        console.log("Server component loaded, will create custom page");
    }
}

// Create the custom page dynamically
Component.prototype.createOperationsForPath = function(path) {
    component.createOperationsForPath(path);
}

// This gets called when the component selection changes
Component.prototype.componentSelectionPageEntered = function() {
    if (installer.isInstaller()) {
        var serverSelected = component.installationRequested();
        console.log("Server component selected: " + serverSelected);
        
        if (serverSelected) {
            // Add our custom page after component selection
            try {
                if (!installer.value("ServerOptionsPageAdded")) {
                    console.log("Adding ServerOptionsPage to wizard");
                    installer.addWizardPage(component, "ServerOptionsPage", QInstaller.ReadyForInstallation);
                    installer.setValue("ServerOptionsPageAdded", "true");
                }
            } catch(e) {
                console.log("Error adding page: " + e);
            }
        }
    }
}

// Custom page callback - creates the UI dynamically
Component.prototype.ServerOptionsPageCallback = function() {
    console.log("ServerOptionsPage callback triggered");
    
    var widget = gui.currentPageWidget();
    if (!widget) {
        console.log("ERROR: No current page widget");
        return;
    }
    
    console.log("Creating ServerOptions UI...");
    
    // Set page title
    widget.title = "Server Installation Options";
    
    // Create main layout
    var layout = new QVBoxLayout(widget);
    widget.setLayout(layout);
    
    // Add description
    var descLabel = new QLabel(widget);
    descLabel.text = "Select additional options for Traintastic Server installation:";
    descLabel.wordWrap = true;
    var font = descLabel.font;
    font.pointSize = 10;
    descLabel.font = font;
    layout.addWidget(descLabel);
    
    // Add spacer
    layout.addSpacing(20);
    
    // Shortcuts group
    var shortcutsGroup = new QGroupBox(widget);
    shortcutsGroup.title = "Shortcuts";
    var shortcutsLayout = new QVBoxLayout(shortcutsGroup);
    shortcutsGroup.setLayout(shortcutsLayout);
    
    var cb1 = new QCheckBox(shortcutsGroup);
    cb1.objectName = "desktopShortcut";
    cb1.text = "Create desktop shortcuts for Server and Client";
    cb1.checked = false;
    cb1.toggled.connect(function(checked) {
        component.desktopShortcut = checked;
        console.log("Desktop shortcut: " + checked);
    });
    shortcutsLayout.addWidget(cb1);
    
    var cb2 = new QCheckBox(shortcutsGroup);
    cb2.objectName = "taskbarShortcut";
    cb2.text = "Pin Traintastic Server to taskbar";
    cb2.checked = false;
    cb2.toggled.connect(function(checked) {
        component.taskbarShortcut = checked;
        console.log("Taskbar shortcut: " + checked);
    });
    shortcutsLayout.addWidget(cb2);
    
    var cb3 = new QCheckBox(shortcutsGroup);
    cb3.objectName = "manualShortcut";
    cb3.text = "Create shortcut to Traintastic Manual";
    cb3.checked = false;
    cb3.toggled.connect(function(checked) {
        component.manualShortcut = checked;
        console.log("Manual shortcut: " + checked);
    });
    shortcutsLayout.addWidget(cb3);
    
    layout.addWidget(shortcutsGroup);
    layout.addSpacing(15);
    
    // Startup group
    var startupGroup = new QGroupBox(widget);
    startupGroup.title = "Startup";
    var startupLayout = new QVBoxLayout(startupGroup);
    startupGroup.setLayout(startupLayout);
    
    var cb4 = new QCheckBox(startupGroup);
    cb4.objectName = "startOnStartup";
    cb4.text = "Start Traintastic Server automatically when Windows starts";
    cb4.checked = false;
    cb4.toggled.connect(function(checked) {
        component.startOnStartup = checked;
        console.log("Start on startup: " + checked);
    });
    startupLayout.addWidget(cb4);
    
    layout.addWidget(startupGroup);
    layout.addSpacing(15);
    
    // Windows Firewall group
    var firewallGroup = new QGroupBox(widget);
    firewallGroup.title = "Windows Firewall";
    var firewallLayout = new QVBoxLayout(firewallGroup);
    firewallGroup.setLayout(firewallLayout);
    
    var cb5 = new QCheckBox(firewallGroup);
    cb5.objectName = "firewallTraintastic";
    cb5.text = "Allow Traintastic client connections (TCP/UDP port 5740)";
    cb5.checked = false;
    cb5.toggled.connect(function(checked) {
        component.firewallTraintastic = checked;
        console.log("Firewall Traintastic: " + checked);
    });
    firewallLayout.addWidget(cb5);
    
    var cb6 = new QCheckBox(firewallGroup);
    cb6.objectName = "firewallWLANmaus";
    cb6.text = "Allow WLANmaus/Z21 protocol (UDP port 21105)";
    cb6.checked = false;
    cb6.toggled.connect(function(checked) {
        component.firewallWLANmaus = checked;
        console.log("Firewall WLANmaus: " + checked);
    });
    firewallLayout.addWidget(cb6);
    
    layout.addWidget(firewallGroup);
    
    // Add stretch at the end
    layout.addStretch();
    
    console.log("ServerOptions UI created successfully");
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();
        
        if (systemInfo.kernelType !== "winnt") {
            return;
        }
        
        var targetDir = installer.value("TargetDir");
        var serverExe = targetDir + "/server/traintastic-server.exe";
        var clientExe = targetDir + "/client/traintastic-client.exe";
        var maintenanceTool = targetDir + "/TraintasticMaintenanceTool.exe";
        var manualPath = "C:/ProgramData/traintastic/manual/en/index.html";
        
        // VC++ Redistributable (only on initial install)
        if (installer.isInstaller() && needsVCRedist()) {
            console.log("Installing VC++ Redistributable...");
            var vcRedistExe = targetDir + "/client/vc_redist.x64.exe";
            if (installer.fileExists(vcRedistExe)) {
                component.addOperation("Execute", 
                    "{0,1,3010}", 
                    vcRedistExe, 
                    "/quiet", 
                    "/norestart");
            }
        }
        
        // Save component selection to registry
        var componentSelection = installer.value("ComponentSelection");
        if (componentSelection) {
            console.log("Saving component selection: " + componentSelection);
            component.addOperation("GlobalConfig",
                "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
                "Components",
                componentSelection);
        }
        
        // Registry: Traintastic location and version
        component.addOperation("GlobalConfig",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
            "InstallLocation",
            targetDir);
            
        component.addOperation("GlobalConfig",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
            "Version",
            "@ProductVersion@");
        
        // Registry: Windows Uninstall entry
        var uninstallKey = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Traintastic";
        
        component.addOperation("GlobalConfig", uninstallKey, "DisplayName", "Traintastic");
        component.addOperation("GlobalConfig", uninstallKey, "DisplayVersion", "@ProductVersion@");
        component.addOperation("GlobalConfig", uninstallKey, "Publisher", "Reinder Feenstra");
        component.addOperation("GlobalConfig", uninstallKey, "DisplayIcon", serverExe + ",0");
        component.addOperation("GlobalConfig", uninstallKey, "InstallLocation", targetDir);
        component.addOperation("GlobalConfig", uninstallKey, "UninstallString", '"' + maintenanceTool + '"');
        component.addOperation("GlobalConfig", uninstallKey, "ModifyPath", '"' + maintenanceTool + '" --manage-packages');
        component.addOperation("GlobalConfig", uninstallKey, "NoModify", "0");
        component.addOperation("GlobalConfig", uninstallKey, "NoRepair", "1");
        
        // Start menu shortcuts (always created)
        component.addOperation("CreateShortcut",
            serverExe,
            "@StartMenuDir@/Traintastic Server.lnk",
            "workingDirectory=" + targetDir + "/server",
            "iconPath=" + serverExe,
            "iconId=0",
            "description=Start Traintastic Server");
        
        component.addOperation("CreateShortcut",
            maintenanceTool,
            "@StartMenuDir@/Modify, Repair or Uninstall Traintastic.lnk",
            "workingDirectory=" + targetDir,
            "iconPath=" + maintenanceTool,
            "iconId=0",
            "description=Modify, update, repair or uninstall Traintastic");
        
        // Process user selections (only on initial install)
        if (installer.isInstaller()) {
            console.log("Processing user selections...");
            console.log("  Desktop shortcuts: " + component.desktopShortcut);
            console.log("  Taskbar pin: " + component.taskbarShortcut);
            console.log("  Manual shortcut: " + component.manualShortcut);
            console.log("  Start on startup: " + component.startOnStartup);
            console.log("  Firewall (Traintastic): " + component.firewallTraintastic);
            console.log("  Firewall (WLANmaus): " + component.firewallWLANmaus);
            
            // Desktop shortcuts
            if (component.desktopShortcut) {
                console.log("Creating desktop shortcuts...");
                
                component.addOperation("CreateShortcut",
                    serverExe,
                    "@DesktopDir@/Traintastic Server.lnk",
                    "workingDirectory=" + targetDir + "/server",
                    "iconPath=" + serverExe,
                    "iconId=0",
                    "description=Start Traintastic Server");
                
                // Check if client component is also being installed
                var clientComponent = installer.componentByName("org.traintastic.client");
                if (clientComponent && clientComponent.installationRequested()) {
                    component.addOperation("CreateShortcut",
                        clientExe,
                        "@DesktopDir@/Traintastic Client.lnk",
                        "workingDirectory=" + targetDir + "/client",
                        "iconPath=" + clientExe,
                        "iconId=0",
                        "description=Start Traintastic Client");
                }
            }
            
            // Taskbar shortcut
            if (component.taskbarShortcut) {
                console.log("Pinning server to taskbar...");
                
                // Use PowerShell to pin to taskbar (works better than VBS)
                var psCommand = '$shell = New-Object -ComObject Shell.Application; ' +
                               '$folder = $shell.Namespace(\\"' + targetDir.replace(/\//g, "\\\\") + '\\\\server\\"); ' +
                               '$item = $folder.ParseName(\\"traintastic-server.exe\\"); ' +
                               '$verb = $item.Verbs() | Where-Object {$_.Name -match \\"Pin to taskbar\\" -or $_.Name -match \\"An Taskleiste\\"}; ' +
                               'if($verb) { $verb.DoIt() }';
                
                component.addOperation("Execute",
                    "{0,1}",
                    "powershell",
                    "-NoProfile",
                    "-ExecutionPolicy", "Bypass",
                    "-Command", psCommand);
            }
            
            // Manual shortcut
            if (component.manualShortcut) {
                console.log("Creating manual shortcuts...");
                
                component.addOperation("CreateShortcut",
                    manualPath,
                    "@StartMenuDir@/Traintastic Manual.lnk",
                    "iconPath=%SystemRoot%\\System32\\shell32.dll",
                    "iconId=23",
                    "description=Open Traintastic Manual");
                
                if (component.desktopShortcut) {
                    component.addOperation("CreateShortcut",
                        manualPath,
                        "@DesktopDir@/Traintastic Manual.lnk",
                        "iconPath=%SystemRoot%\\System32\\shell32.dll",
                        "iconId=23",
                        "description=Open Traintastic Manual");
                }
            }
            
            // Auto-startup
            if (component.startOnStartup) {
                console.log("Adding server to Windows startup...");
                
                component.addOperation("GlobalConfig",
                    "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                    "Traintastic Server",
                    '"' + serverExe + '" --tray');
            }
            
            // Firewall - Traintastic client
            if (component.firewallTraintastic) {
                console.log("Adding firewall rules for Traintastic client...");
                
                component.addElevatedOperation("Execute",
                    "{0,1}",
                    "netsh", "advfirewall", "firewall", "add", "rule",
                    "name=Traintastic server (TCP)",
                    "dir=in",
                    "program=" + serverExe,
                    "protocol=TCP",
                    "localport=5740",
                    "action=allow",
                    "UNDOEXECUTE",
                    "netsh", "advfirewall", "firewall", "delete", "rule",
                    "name=Traintastic server (TCP)");
                    
                component.addElevatedOperation("Execute",
                    "{0,1}",
                    "netsh", "advfirewall", "firewall", "add", "rule",
                    "name=Traintastic server (UDP)",
                    "dir=in",
                    "program=" + serverExe,
                    "protocol=UDP",
                    "localport=5740",
                    "action=allow",
                    "UNDOEXECUTE",
                    "netsh", "advfirewall", "firewall", "delete", "rule",
                    "name=Traintastic server (UDP)");
            }
            
            // Firewall - WLANmaus/Z21
            if (component.firewallWLANmaus) {
                console.log("Adding firewall rule for WLANmaus/Z21...");
                
                component.addElevatedOperation("Execute",
                    "{0,1}",
                    "netsh", "advfirewall", "firewall", "add", "rule",
                    "name=Traintastic server (WLANmaus/Z21)",
                    "dir=in",
                    "program=" + serverExe,
                    "protocol=UDP",
                    "localport=21105",
                    "action=allow",
                    "UNDOEXECUTE",
                    "netsh", "advfirewall", "firewall", "delete", "rule",
                    "name=Traintastic server (WLANmaus/Z21)");
            }
            
            // Create initial server settings file with language
            var settingsDir = installer.value("HomeDir") + "/AppData/Local/traintastic/server";
            var settingsPath = settingsDir + "/settings.json";
            
            if (!installer.fileExists(settingsPath)) {
                var language = getTraintasticLanguage();
                console.log("Creating server settings file with language: " + language);
                component.addOperation("Mkdir", settingsDir);
                component.addOperation("AppendFile", settingsPath, '{"language":"' + language + '"}');
            }
        }
        
        // Complete registry cleanup on uninstall
        if (installer.isUninstaller()) {
            console.log("Cleaning up registry entries...");
            
            component.addElevatedOperation("Execute",
                "{0,1}",
                "cmd", "/c",
                "reg delete \"HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\" /f 2>nul");
                
            component.addElevatedOperation("Execute",
                "{0,1}",
                "cmd", "/c",
                "reg delete \"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Traintastic\" /f 2>nul");
                
            component.addElevatedOperation("Execute",
                "{0,1}",
                "cmd", "/c",
                "reg delete \"HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run\" /v \"Traintastic Server\" /f 2>nul");
        }
        
    } catch (e) {
        console.log("ERROR in server createOperations: " + e);
        if (e.stack) {
            console.log("Stack trace: " + e.stack);
        }
    }
}

function needsVCRedist() {
    if (systemInfo.kernelType !== "winnt")
        return false;
        
    try {
        var result = installer.execute("reg", ["query", 
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64",
            "/v", "Version"]);
        
        if (!result || result.length === 0) {
            return true;
        }
        
        var version = result[0];
        if (version.indexOf("v14.2") === -1 || version < "v14.24") {
            return true;
        }
        
        return false;
        
    } catch (e) {
        return true;
    }
}

function getTraintasticLanguage() {
    var locale = installer.value("Locale");
    
    if (locale.indexOf("nl") === 0) return "nl-nl";
    if (locale.indexOf("de") === 0) return "de-de";
    if (locale.indexOf("it") === 0) return "it-it";
    if (locale.indexOf("sv") === 0) return "sv-se";
    if (locale.indexOf("fr") === 0) return "fr-fr";
    if (locale.indexOf("pl") === 0) return "pl-pl";
    
    return "en-us";
}
