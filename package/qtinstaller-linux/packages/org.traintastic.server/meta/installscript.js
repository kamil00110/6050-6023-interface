function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    try {
        // Call default implementation
        component.createOperations();
        
        var targetDir = installer.value("TargetDir");
        var homeDir = installer.value("HomeDir");
        
        // Find the actual server binary path (extracted from deb)
        var serverBin = "";
        var possiblePaths = [
            targetDir + "/usr/bin/traintastic-server",
            targetDir + "/bin/traintastic-server"
        ];
        
        for (var i = 0; i < possiblePaths.length; i++) {
            if (installer.fileExists(possiblePaths[i])) {
                serverBin = possiblePaths[i];
                console.log("Found server at: " + serverBin);
                break;
            }
        }
        
        if (serverBin === "") {
            console.log("ERROR: Could not find server binary");
            return;
        }
        
        var maintenanceTool = targetDir + "/TraintasticMaintenanceTool";
        
        // Make server executable
        component.addOperation("Execute", "chmod", "+x", serverBin);
        
        // Create symlink in target/bin for easier access
        component.addOperation("Mkdir", targetDir + "/bin");
        component.addOperation("Execute", 
            "ln", "-sf", 
            serverBin, 
            targetDir + "/bin/traintastic-server");
        
        // Create desktop entry for server
        var desktopEntry = "[Desktop Entry]\n" +
            "Type=Application\n" +
            "Name=Traintastic Server\n" +
            "Comment=Model railway control system server\n" +
            "Exec=" + targetDir + "/bin/traintastic-server --tray\n" +
            "Icon=" + targetDir + "/usr/share/pixmaps/traintastic-server.png\n" +
            "Categories=Utility;Application;\n" +
            "Terminal=false\n";
        
        var desktopFile = homeDir + "/.local/share/applications/traintastic-server.desktop";
        
        component.addOperation("Mkdir", homeDir + "/.local/share/applications");
        component.addOperation("AppendFile", desktopFile, desktopEntry);
        component.addOperation("Execute", "chmod", "+x", desktopFile);
        
        // Create desktop entry for maintenance tool
        var maintenanceEntry = "[Desktop Entry]\n" +
            "Type=Application\n" +
            "Name=Modify, Repair or Uninstall Traintastic\n" +
            "Comment=Update, modify, repair or uninstall Traintastic\n" +
            "Exec=" + maintenanceTool + "\n" +
            "Icon=" + targetDir + "/usr/share/pixmaps/traintastic.png\n" +
            "Categories=System;Settings;\n" +
            "Terminal=false\n";
        
        var maintenanceDesktopFile = homeDir + "/.local/share/applications/traintastic-maintenance.desktop";
        component.addOperation("AppendFile", maintenanceDesktopFile, maintenanceEntry);
        component.addOperation("Execute", "chmod", "+x", maintenanceDesktopFile);
        
        // Create server settings file if it doesn't exist (only on initial install)
        if (installer.isInstaller()) {
            var configDir = homeDir + "/.config/traintastic/server";
            var settingsPath = configDir + "/settings.json";
            
            if (!installer.fileExists(settingsPath)) {
                var language = getTraintasticLanguage();
                var settingsContent = '{"language":"' + language + '"}';
                
                component.addOperation("Mkdir", configDir);
                component.addOperation("AppendFile", settingsPath, settingsContent);
            }
        }
        
        // Optional: Create systemd service file
        var page = component.userInterface("LinuxOptionsPage");
        if (page && page.createSystemdService && page.createSystemdService.checked) {
            var serviceFile = homeDir + "/.config/systemd/user/traintastic-server.service";
            var serviceContent = "[Unit]\n" +
                "Description=Traintastic Server\n" +
                "After=network.target\n" +
                "\n" +
                "[Service]\n" +
                "Type=simple\n" +
                "ExecStart=" + targetDir + "/bin/traintastic-server\n" +
                "Restart=on-failure\n" +
                "RestartSec=5\n" +
                "\n" +
                "[Install]\n" +
                "WantedBy=default.target\n";
            
            component.addOperation("Mkdir", homeDir + "/.config/systemd/user");
            component.addOperation("AppendFile", serviceFile, serviceContent);
            
            if (page.enableSystemdService && page.enableSystemdService.checked) {
                component.addOperation("Execute", "systemctl", "--user", "daemon-reload");
                component.addOperation("Execute", "systemctl", "--user", "enable", "traintastic-server.service");
                component.addOperation("Execute", "systemctl", "--user", "start", "traintastic-server.service");
            }
        }
        
    } catch (e) {
        console.log("Error in server createOperations: " + e);
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
