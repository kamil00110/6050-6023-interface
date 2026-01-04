function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();
        
        var targetDir = installer.value("TargetDir");
        var clientExe = targetDir + "/client/traintastic-client.exe";
        
        // Create start menu shortcut
        component.addOperation("CreateShortcut",
            clientExe,
            "@StartMenuDir@/Traintastic Client.lnk",
            "workingDirectory=" + targetDir + "/client",
            "iconPath=" + clientExe,
            "iconId=0",
            "description=Start Traintastic Client");
        
        // Desktop shortcut - check if requested in server component options
        if (installer.isInstaller()) {
            var serverComponent = installer.componentByName("org.traintastic.server");
            if (serverComponent) {
                var serverPage = serverComponent.userInterface("ServerPage");
                if (serverPage && serverPage.createDesktopIcon && serverPage.createDesktopIcon.checked) {
                    component.addOperation("CreateShortcut",
                        clientExe,
                        "@DesktopDir@/Traintastic Client.lnk",
                        "workingDirectory=" + targetDir + "/client",
                        "iconPath=" + clientExe,
                        "iconId=0",
                        "description=Start Traintastic Client");
                }
            }
        }
        
        // Write client language configuration
        var iniPath = installer.value("CommonAppDataDir") + "/traintastic/traintastic-client.ini";
        var language = getTraintasticLanguage();
        
        component.addOperation("Settings",
            "path=" + iniPath,
            "method=set",
            "key=general_/language",
            "value=" + language);
            
    } catch (e) {
        console.log("Error in client createOperations: " + e);
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
