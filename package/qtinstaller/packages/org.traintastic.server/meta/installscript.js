function Component() {
    // Constructor
    component.loaded.connect(this, Component.prototype.installerLoaded);
    
    // Add custom wizard page for firewall options
    installer.addWizardPage(component, "FirewallPage", QInstaller.ComponentSelection);
}

Component.prototype.installerLoaded = function() {
    if (installer.isInstaller()) {
        component.setValue("FirewallTraintastic", "false");
        component.setValue("FirewallWLANmaus", "false");
    }
}

Component.prototype.createOperations = function() {
    try {
        // Call default implementation
        component.createOperations();
        
        var targetDir = installer.value("TargetDir");
        var serverExe = targetDir + "/server/traintastic-server.exe";
        
        // Check and install VC++ Redistributable
        var vcRedistExe = targetDir + "/client/vc_redist.x64.exe";
        if (systemInfo.kernelType === "winnt") {
            if (needsVCRedist()) {
                component.addOperation("Execute", 
                    "{0,1}", 
                    vcRedistExe, 
                    "/quiet", 
                    "/norestart",
                    "UNDOEXECUTE",
                    "echo", "VC++ Redist uninstall not needed");
            }
        }
        
        // Create registry entries
        component.addOperation("GlobalConfig",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
            "InstallLocation",
            targetDir);
            
        component.addOperation("GlobalConfig",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
            "Version",
            installer.value("Version"));
            
        component.addOperation("GlobalConfig",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\traintastic.org\\Traintastic",
            "Components",
            getInstalledComponents());
        
        // Create shortcuts
        var startMenuDir = installer.value("StartMenuDir");
        
        component.addOperation("CreateShortcut",
            serverExe,
            "@StartMenuDir@/Traintastic Server.lnk",
            "workingDirectory=" + targetDir + "/server",
            "iconPath=" + serverExe,
            "iconId=0",
            "description=Start Traintastic Server",
            "arguments=--tray");
            
        if (component.userInterface("FirewallPage").createDesktopIcon.checked) {
            component.addOperation("CreateShortcut",
                serverExe,
                "@DesktopDir@/Traintastic Server.lnk",
                "workingDirectory=" + targetDir + "/server",
                "iconPath=" + serverExe,
                "iconId=0",
                "description=Start Traintastic Server",
                "arguments=--tray");
        }
        
        // Firewall rules
        if (component.userInterface("FirewallPage").allowTraintastic.checked) {
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
        
        if (component.userInterface("FirewallPage").allowWLANmaus.checked) {
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
        
        // Create server settings file if it doesn't exist
        var settingsPath = QDesktopServices.storageLocation(QDesktopServices.DataLocation) 
            + "/traintastic/server/settings.json";
        var settingsDir = QDir(settingsPath).path;
        
        if (!installer.fileExists(settingsPath)) {
            var language = getTraintasticLanguage();
            var settingsContent = '{"language":"' + language + '"}';
            
            component.addOperation("Mkdir", settingsDir);
            component.addOperation("AppendFile", settingsPath, settingsContent);
        }
        
    } catch (e) {
        console.log("Error in createOperations: " + e);
    }
}

function needsVCRedist() {
    if (systemInfo.kernelType !== "winnt")
        return false;
        
    var regPath = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64";
    var version = installer.value(regPath + "\\Version");
    
    if (version === "") {
        console.log("VC++ Redistributable not found");
        return true;
    }
    
    console.log("VC++ Redistributable version: " + version);
    
    // Check if version is at least v14.24.28127.04
    if (version < "v14.24.28127.04") {
        console.log("VC++ Redistributable version too old");
        return true;
    }
    
    return false;
}

function getTraintasticLanguage() {
    var locale = QLocale().name();
    
    if (locale.startsWith("nl"))
        return "nl-nl";
    else if (locale.startsWith("de"))
        return "de-de";
    else if (locale.startsWith("it"))
        return "it-it";
    else if (locale.startsWith("sv"))
        return "sv-se";
    else if (locale.startsWith("fr"))
        return "fr-fr";
    else if (locale.startsWith("pl"))
        return "pl-pl";
    else
        return "en-us";
}

function getInstalledComponents() {
    var components = [];
    
    if (installer.componentByName("org.traintastic.server").installationRequested())
        components.push("Server");
        
    if (installer.componentByName("org.traintastic.client").installationRequested())
        components.push("Client");
        
    return components.join(",");
}

Component.prototype.FirewallPageCallback = function() {
    var page = component.userInterface("FirewallPage");
    page.complete = true;
}
