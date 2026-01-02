function Component() {
    // Constructor - shared component is always installed
}

Component.prototype.createOperations = function() {
    try {
        // First, call default implementation to extract files to TargetDir
        component.createOperations();
        
        var commonAppData = installer.value("CommonAppDataDir");
        if (!commonAppData) {
            commonAppData = "C:/ProgramData";
        }
        
        var dataDir = commonAppData + "/traintastic";
        var targetDir = installer.value("TargetDir");
        
        console.log("Moving shared data from: " + targetDir);
        console.log("Moving shared data to: " + dataDir);
        
        // Ensure traintastic data directory exists
        component.addOperation("Mkdir", dataDir);
        
        // Move translations from TargetDir to ProgramData
        component.addOperation("CopyDirectory",
            targetDir + "/translations",
            dataDir + "/translations");
        
        // Move manual from TargetDir to ProgramData
        component.addOperation("CopyDirectory",
            targetDir + "/manual",
            dataDir + "/manual");
        
        // Move LNCV from TargetDir to ProgramData
        component.addOperation("CopyDirectory",
            targetDir + "/lncv",
            dataDir + "/lncv");
        
        // Remove the copied directories from Program Files
        component.addOperation("Rmdir",
            targetDir + "/translations");
        
        component.addOperation("Rmdir",
            targetDir + "/manual");
        
        component.addOperation("Rmdir",
            targetDir + "/lncv");
        
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
        console.log("Stack: " + e.stack);
    }
}
