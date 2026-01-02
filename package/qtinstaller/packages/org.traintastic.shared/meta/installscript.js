function Component() {
    // Constructor - shared component is always installed
}

Component.prototype.createOperations = function() {
    try {
        // Call default implementation to copy files
        component.createOperations();
        
        var commonAppData = installer.value("CommonAppDataDir");
        if (!commonAppData) {
            commonAppData = installer.value("AllUsersAppDataDir");
        }
        
        var dataDir = commonAppData + "/traintastic";
        
        // Ensure traintastic data directory exists
        component.addOperation("Mkdir", dataDir);
        component.addOperation("Mkdir", dataDir + "/translations");
        component.addOperation("Mkdir", dataDir + "/manual");
        component.addOperation("Mkdir", dataDir + "/lncv");
        
        // Delete old translation files (migration from older versions)
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
        
        // Delete old DLL files that are now statically linked (migration)
        var targetDir = installer.value("TargetDir");
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
        console.log("Error in shared component createOperations: " + e);
    }
}
