function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    try {
        // Call default implementation
        component.createOperations();
        
        if (systemInfo.kernelType !== "winnt") {
            return;
        }
        
        var targetDir = installer.value("TargetDir");
        var serverExe = targetDir + "/server/traintastic-server.exe";
        var maintenanceTool = targetDir + "/TraintasticMaintenanceTool.exe";
        
        // Check and install VC++ Redistributable (only on initial install)
        if (installer.isInstaller() && needsVCRedist()) {
            var vcRedistExe = targetDir + "/client/vc_redist.x64.exe";
            component.addOperation("Execute", 
                "{0,1}", 
                vcRedistExe, 
                "/quiet", 
                "/norestart",
                "UNDOEXECUTE",
                "echo", "VC++ Redist uninstall not needed");
        }
        
        // Create registry entries
        component.addOperation("GlobalConfig",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
            "InstallLocation",
            targetDir);
            
        component.addOperation("GlobalConfig",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
            "Version",
            "@ProductVersion@");
        
        // Create shortcuts
        component.addOperation("CreateShortcut",
            serverExe,
            "@StartMenuDir@/Traintastic Server.lnk",
            "workingDirectory=" + targetDir + "/server",
            "iconPath=" + serverExe,
            "iconId=0",
            "description=Start Traintastic Server");
        
        // Add maintenance tool shortcut to start menu
        component.addOperation("CreateShortcut",
            maintenanceTool,
            "@StartMenuDir@/Modify, Repair or Uninstall Traintastic.lnk",
            "workingDirectory=" + targetDir,
            "iconPath=" + maintenanceTool,
            "iconId=0",
            "description=Modify, update, repair or uninstall Traintastic");
        
        // Firewall rules - get checkbox states from UI
        var page = component.userInterface("FirewallPage");
        
        if (page && page.allowTraintastic && page.allowTraintastic.checked) {
            component.addElevatedOperation("Execute",
                "{0,1}",
                "netsh",
                "advfirewall",
                "firewall",
                "add",
                "rule",
                "name=Traintastic server (TCP)",
                "dir=in",
                "program=" + serverExe,
                "protocol=TCP",
                "localport=5740",
                "action=allow",
                "UNDOEXECUTE",
                "netsh",
                "advfirewall",
                "firewall",
                "delete",
                "rule",
                "name=Traintastic server (TCP)");
                
            component.addElevatedOperation("Execute",
                "{0,1}",
                "netsh",
                "advfirewall",
                "firewall",
                "add",
                "rule",
                "name=Traintastic server (UDP)",
                "dir=in",
                "program=" + serverExe,
                "protocol=UDP",
                "localport=5740",
                "action=allow",
                "UNDOEXECUTE",
                "netsh",
                "advfirewall",
                "firewall",
                "delete",
                "rule",
                "name=Traintastic server (UDP)");
        }
        
        if (page && page.allowWLANmaus && page.allowWLANmaus.checked) {
            component.addElevatedOperation("Execute",
                "{0,1}",
                "netsh",
                "advfirewall",
                "firewall",
                "add",
                "rule",
                "name=Traintastic server (WLANmaus/Z21)",
                "dir=in",
                "program=" + serverExe,
                "protocol=UDP",
                "localport=21105",
                "action=allow",
                "UNDOEXECUTE",
                "netsh",
                "advfirewall",
                "firewall",
                "delete",
                "rule",
                "name=Traintastic server (WLANmaus/Z21)");
        }
        
        // Desktop shortcuts
        if (page && page.createDesktopIcon && page.createDesktopIcon.checked) {
            component.addOperation("CreateShortcut",
                serverExe,
                "@DesktopDir@/Traintastic Server.lnk",
                "workingDirectory=" + targetDir + "/server",
                "iconPath=" + serverExe,
                "iconId=0",
                "description=Start Traintastic Server");
        }
        
        // Create server settings file if it doesn't exist (only on initial install)
        if (installer.isInstaller()) {
            var settingsDir = installer.value("HomeDir") + "/AppData/Local/traintastic/server";
            var settingsPath = settingsDir + "/settings.json";
            
            if (!installer.fileExists(settingsPath)) {
                var language = getTraintasticLanguage();
                var settingsContent = '{"language":"' + language + '"}';
                
                component.addOperation("Mkdir", settingsDir);
                component.addOperation("AppendFile", settingsPath, settingsContent);
            }
        }
        
    } catch (e) {
        console.log("Error in server createOperations: " + e);
    }
}

function needsVCRedist() {
    if (systemInfo.kernelType !== "winnt")
        return false;
        
    try {
        var version = installer.execute("reg", ["query", 
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64",
            "/v", "Version"])[0];
        
        if (!version) {
            console.log("VC++ Redistributable not found");
            return true;
        }
        
        console.log("VC++ Redistributable found: " + version);
        
        // Check if version is at least v14.24.28127.04
        if (version.indexOf("v14.2") === -1 || version < "v14.24") {
            console.log("VC++ Redistributable version too old");
            return true;
        }
    } catch (e) {
        console.log("Error checking VC++ Redistributable: " + e);
        return true;
    }
    
    return false;
}

function getTraintasticLanguage() {
    var locale = installer.value("Locale");
    
    if (locale.indexOf("nl") === 0)
        return "nl-nl";
    else if (locale.indexOf("de") === 0)
        return "de-de";
    else if (locale.indexOf("it") === 0)
        return "it-it";
    else if (locale.indexOf("sv") === 0)
        return "sv-se";
    else if (locale.indexOf("fr") === 0)
        return "fr-fr";
    else if (locale.indexOf("pl") === 0)
        return "pl-pl";
    else
        return "en-us";
}
