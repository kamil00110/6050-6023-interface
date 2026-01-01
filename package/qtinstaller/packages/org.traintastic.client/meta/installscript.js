function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    try {
        // Call default implementation
        component.createOperations();
        
        var targetDir = installer.value("TargetDir");
        var clientExe = targetDir + "/client/traintastic-client.exe";
        var commonAppData = installer.value("CommonAppDataDir");
        
        // Create shortcuts
        component.addOperation("CreateShortcut",
            clientExe,
            "@StartMenuDir@/Traintastic Client.lnk",
            "workingDirectory=" + targetDir + "/client",
            "iconPath=" + clientExe,
            "iconId=0",
            "description=Start Traintastic Client");
        
        // Check if server component is also installed
        var serverInstalled = installer.componentByName("org.traintastic.server").installationRequested();
        
        // Only create desktop icon if it was requested (from server's UI page)
        if (serverInstalled) {
            var serverPage = installer.componentByName("org.traintastic.server").userInterface("FirewallPage");
            if (serverPage && serverPage.createDesktopIcon.checked) {
                component.addOperation("CreateShortcut",
                    clientExe,
                    "@DesktopDir@/Traintastic Client.lnk",
                    "workingDirectory=" + targetDir + "/client",
                    "iconPath=" + clientExe,
                    "iconId=0",
                    "description=Start Traintastic Client");
            }
        }
        
        // Write client language configuration
        var iniPath = commonAppData + "/traintastic/traintastic-client.ini";
        var language = getTraintasticLanguage();
        
        component.addOperation("Settings",
            "path=" + iniPath,
            "method=set",
            "key=general_/language",
            "value=" + language);
            
    } catch (e) {
        console.log("Error in createOperations: " + e);
    }
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
