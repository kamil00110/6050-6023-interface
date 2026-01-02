function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    try {
        // Call default implementation
        component.createOperations();
        
        var targetDir = installer.value("TargetDir");
        var homeDir = installer.value("HomeDir");
        var clientBin = targetDir + "/bin/traintastic-client";
        
        // Make client executable
        component.addOperation("Execute", "chmod", "+x", clientBin);
        
        // Create desktop entry for client
        var desktopEntry = "[Desktop Entry]\n" +
            "Type=Application\n" +
            "n=Traintastic Client\n" +
            "Comment=Model railway control system client\n" +
            "Exec=" + clientBin + "\n" +
            "Icon=" + targetDir + "/share/pixmaps/traintastic-client.png\n" +
            "Categories=Utility;Application;\n" +
            "Terminal=false\n";
        
        var desktopFile = homeDir + "/.local/share/applications/traintastic-client.desktop";
        
        component.addOperation("Mkdir", homeDir + "/.local/share/applications");
        component.addOperation("AppendFile", desktopFile, desktopEntry);
        component.addOperation("Execute", "chmod", "+x", desktopFile);
        
        // Write client language configuration
        var configDir = homeDir + "/.config/traintastic";
        var iniPath = configDir + "/traintastic-client.ini";
        var language = getTraintasticLanguage();
        
        component.addOperation("Mkdir", configDir);
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
