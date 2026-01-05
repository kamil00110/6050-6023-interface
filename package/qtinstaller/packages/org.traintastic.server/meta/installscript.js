function Component() {
    console.log("Server component constructor called");
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
        
        // Process user selections from ServerOptionsPage (only on initial install)
        if (installer.isInstaller()) {
            console.log("Processing ServerOptions user selections...");
            
            // Read checkbox states from installer values
            var desktopShortcut = installer.value("ServerOptions_desktopShortcut_checked") === "true";
            var taskbarShortcut = installer.value("ServerOptions_taskbarShortcut_checked") === "true";
            var manualShortcut = installer.value("ServerOptions_manualShortcut_checked") === "true";
            var startOnStartup = installer.value("ServerOptions_startOnStartup_checked") === "true";
            var firewallTraintastic = installer.value("ServerOptions_firewallTraintastic_checked") === "true";
            var firewallWLANmaus = installer.value("ServerOptions_firewallWLANmaus_checked") === "true";
            
            console.log("  Desktop shortcuts: " + desktopShortcut);
            console.log("  Taskbar pin: " + taskbarShortcut);
            console.log("  Manual shortcut: " + manualShortcut);
            console.log("  Start on startup: " + startOnStartup);
            console.log("  Firewall (Traintastic): " + firewallTraintastic);
            console.log("  Firewall (WLANmaus): " + firewallWLANmaus);
            
            // Desktop shortcuts
            if (desktopShortcut) {
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
            if (taskbarShortcut) {
                console.log("Pinning server to taskbar...");
                
                // Use PowerShell to pin to taskbar
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
            if (manualShortcut) {
                console.log("Creating manual shortcuts...");
                
                component.addOperation("CreateShortcut",
                    manualPath,
                    "@StartMenuDir@/Traintastic Manual.lnk",
                    "iconPath=%SystemRoot%\\System32\\shell32.dll",
                    "iconId=23",
                    "description=Open Traintastic Manual");
                
                if (desktopShortcut) {
                    component.addOperation("CreateShortcut",
                        manualPath,
                        "@DesktopDir@/Traintastic Manual.lnk",
                        "iconPath=%SystemRoot%\\System32\\shell32.dll",
                        "iconId=23",
                        "description=Open Traintastic Manual");
                }
            }
            
            // Auto-startup
            if (startOnStartup) {
                console.log("Adding server to Windows startup...");
                
                component.addOperation("GlobalConfig",
                    "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                    "Traintastic Server",
                    '"' + serverExe + '" --tray');
            }
            
            // Firewall - Traintastic client
            if (firewallTraintastic) {
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
            if (firewallWLANmaus) {
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
