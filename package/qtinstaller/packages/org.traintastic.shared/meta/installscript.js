function Component() {
    // Constructor - shared component is always installed
}

Component.prototype.createOperations = function() {
    try {
        // Call default implementation to copy files
        component.createOperations();
        
        var commonAppData = installer.value("CommonAppDataDir");
        var dataDir = commonAppData + "/traintastic";
        
        // Ensure traintastic data directory exists
        component.addOperation("Mkdir", dataDir);
        component.addOperation("Mkdir", dataDir + "/translations");
        component.addOperation("Mkdir", dataDir + "/manual");
        component.addOperation("Mkdir", dataDir + "/lncv");
        
        // Delete old translation files (migration from older versions)
        var oldTranslations = [
            "en-us.txt",
            "nl-nl.txt", 
            "de-de.txt",
            "it-it.txt"
        ];
        
        for (var i = 0; i < oldTranslations.length; i++) {
            var oldFile = dataDir + "/translations/" + oldTranslations[i];
            if (installer.fileExists(oldFile)) {
                component.addOperation("Delete", oldFile);
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
