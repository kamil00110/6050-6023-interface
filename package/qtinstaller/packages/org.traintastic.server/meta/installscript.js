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
            }
        }
        
        // Save component selection to registry
        var serverSelected = component.installationRequested();
        var clientComponent = installer.componentByName("org.traintastic.client");
        var clientSelected = clientComponent && clientComponent.installationRequested();
        
        var selection = "";
        if (serverSelected && clientSelected) {
            selection = "ClientAndServer";
        } else if (clientSelected) {
            selection = "ClientOnly";
        } else if (serverSelected) {
            selection = "ServerOnly";
        }
        
        if (selection) {
            console.log("Saving component selection: " + selection);
            component.addOperation("GlobalConfig",
                "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
                "Components",
                selection);
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
        
        // Create initial server settings file with language
        var settingsDir = installer.value("HomeDir") + "/AppData/Local/traintastic/server";
        var settingsPath = settingsDir + "/settings.json";
        
        if (!installer.fileExists(settingsPath)) {
            var language = getTraintasticLanguage();
            console.log("Creating server settings file with language: " + language);
            component.addOperation("Mkdir", settingsDir);
            component.addOperation("AppendFile", settingsPath, '{"language":"' + language + '"}');
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
