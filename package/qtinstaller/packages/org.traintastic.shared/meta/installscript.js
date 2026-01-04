function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();
        
        var commonAppData = installer.value("CommonAppDataDir");
        if (!commonAppData) {
            commonAppData = "C:/ProgramData";
        }
        
        var dataDir = commonAppData + "/traintastic";
        var targetDir = installer.value("TargetDir");
        
        console.log("Moving shared data from: " + targetDir);
        console.log("Moving shared data to: " + dataDir);
        
        // Only move files during install/update, not during uninstall
        if (installer.isInstaller() || installer.isUpdater()) {
            component.addOperation("Mkdir", dataDir);
            
            // Move directories using robocopy
            // Note: robocopy exit codes 0-7 are success, 8+ are errors
            component.addElevatedOperation("Execute",
                "robocopy",
                targetDir + "\\translations",
                dataDir + "\\translations",
                "/E", "/MOVE", "/NFL", "/NDL", "/NJH", "/NJS",
                "ERRORMESSAGE", "Failed to move translations directory");
            
            component.addElevatedOperation("Execute",
                "robocopy",
                targetDir + "\\manual",
                dataDir + "\\manual",
                "/E", "/MOVE", "/NFL", "/NDL", "/NJH", "/NJS",
                "ERRORMESSAGE", "Failed to move manual directory");
            
            component.addElevatedOperation("Execute",
                "robocopy",
                targetDir + "\\lncv",
                dataDir + "\\lncv",
                "/E", "/MOVE", "/NFL", "/NDL", "/NJH", "/NJS",
                "ERRORMESSAGE", "Failed to move lncv directory");
        }
        
        // Migration: delete old translation files
        var oldTranslations = [
            dataDir + "/translations/en-us.txt",
            dataDir + "/translations/nl-nl.txt", 
            dataDir + "/translations/de-de.txt",
            dataDir + "/translations/it-it.txt"
        ];
        
        for (var i = 0; i < oldTranslations.length; i++) {
            if (installer.fileExists(oldTranslations[i])) {
                component.addOperation("Delete", oldTranslations[i]);
            }
        }
        
        // Migration: delete old DLLs
        var oldDlls = [
            targetDir + "/server/lua53.dll",
            targetDir + "/server/lua54.dll",
            targetDir + "/server/archive.dll",
            targetDir + "/server/zlib1.dll"
        ];
        
        for (var i = 0; i < oldDlls.length; i++) {
            if (installer.fileExists(oldDlls[i])) {
                component.addOperation("Delete", oldDlls[i]);
            }
        }
        
    } catch (e) {
        console.log("Error in shared component: " + e);
    }
}
