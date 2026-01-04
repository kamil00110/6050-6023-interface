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
                    console.log("Creating desktop shortcut for client");
                    component.addOperation("CreateShortcut",
                        clientExe,
                        "@DesktopDir@/Traintastic Client.lnk",
                        "workingDirectory=" + targetDir + "/client",
                        "iconPath=" + clientExe,
                        "iconId=0",
                        "description=Start Traintastic Client");
                }
            } else {
                // If server component is not installed, don't check the page
                console.log("Server component not selected, skipping desktop shortcut check");
            }
        }
        
        // Write client language configuration to ProgramData
        var commonAppData = installer.value("CommonAppDataDir");
        if (!commonAppData || commonAppData === "") {
            commonAppData = "C:/ProgramData";
        }
        
        var iniDir = commonAppData + "/traintastic";
        var iniPath = iniDir + "/traintastic-client.ini";
        var language = getTraintasticLanguage();
        
        console.log("Writing client config to: " + iniPath);
        
        // Create directory if it doesn't exist
        component.addOperation("Mkdir", iniDir);
        
        // Write language setting
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
