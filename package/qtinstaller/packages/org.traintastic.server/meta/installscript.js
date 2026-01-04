function Component() {
    console.log("Server component constructor called");
}

// CRITICAL: This makes the ServerPage appear in the installer wizard
Component.prototype.loaded = function() {
    if (installer.isInstaller()) {
        console.log("Adding ServerPage to installer wizard");
        // Add the page before the "Ready for Installation" page
        installer.addWizardPage(component, "ServerPage", QInstaller.ReadyForInstallation);
    }
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();
        
        if (systemInfo.kernelType !== "winnt") {
            return;
        }
        
        var targetDir = installer.value("TargetDir");
        var serverExe = targetDir + "/server/traintastic-server.exe";
        var maintenanceTool = targetDir + "/TraintasticMaintenanceTool.exe";
        
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
            } else {
                console.log("WARNING: vc_redist.x64.exe not found");
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
        
        // Start menu shortcuts
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
        
        // Process user selections from ServerPage (only on initial install)
        if (installer.isInstaller()) {
            console.log("Processing ServerPage user selections...");
            var page = component.userInterface("ServerPage");
            
            if (page) {
                console.log("ServerPage UI accessed successfully");
                
                // Log checkbox states
                var allowTraintastic = page.allowTraintastic && page.allowTraintastic.checked;
                var allowWLANmaus = page.allowWLANmaus && page.allowWLANmaus.checked;
                var createDesktopIcon = page.createDesktopIcon && page.createDesktopIcon.checked;
                var startServerOnStartup = page.startServerOnStartup && page.startServerOnStartup.checked;
                
                console.log("  allowTraintastic: " + allowTraintastic);
                console.log("  allowWLANmaus: " + allowWLANmaus);
                console.log("  createDesktopIcon: " + createDesktopIcon);
                console.log("  startServerOnStartup: " + startServerOnStartup);
                
                // Firewall rules for Traintastic client (TCP/UDP port 5740)
                if (allowTraintastic) {
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
                
                // Firewall rules for WLANmaus/Z21 (UDP port 21105)
                if (allowWLANmaus) {
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
                
                // Desktop shortcuts
                if (createDesktopIcon) {
                    console.log("Creating desktop shortcut for server...");
                    
                    component.addOperation("CreateShortcut",
                        serverExe,
                        "@DesktopDir@/Traintastic Server.lnk",
                        "workingDirectory=" + targetDir + "/server",
                        "iconPath=" + serverExe,
                        "iconId=0",
                        "description=Start Traintastic Server");
                }
                
                // Auto-startup with Windows
                if (startServerOnStartup) {
                    console.log("Adding server to Windows startup...");
                    
                    component.addOperation("GlobalConfig",
                        "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                        "Traintastic Server",
                        '"' + serverExe + '" --tray');
                }
                
            } else {
                console.log("WARNING: Could not access ServerPage UI");
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
            console.log("VC++ Redistributable not found");
            return true;
        }
        
        var version = result[0];
        console.log("VC++ Redistributable version: " + version);
        
        if (version.indexOf("v14.2") === -1 || version < "v14.24") {
            console.log("VC++ Redistributable version too old (need v14.24+)");
            return true;
        }
        
        console.log("VC++ Redistributable is up to date");
        return false;
        
    } catch (e) {
        console.log("Error checking VC++ Redistributable: " + e);
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
