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
        
        // Use Execute with robocopy to move directories (more reliable than Move operation)
        // robocopy /MOVE moves files and removes source directories
        // /E copies subdirectories including empty ones
        // /NFL /NDL /NJH /NJS /NC /NS makes output quieter
        
        // Move translations
        component.addElevatedOperation("Execute",
            "{0,1,8}",  // Allow error codes 0 (success), 1 (files copied), 8 (some files/dirs not copied)
            "robocopy",
            targetDir + "/translations",
            dataDir + "/translations",
            "/E",
            "/MOVE",
            "UNDOEXECUTE",
            "robocopy",
            dataDir + "/translations",
            targetDir + "/translations",
            "/E",
            "/MOVE");
        
        // Move manual
        component.addElevatedOperation("Execute",
            "{0,1,8}",
            "robocopy",
            targetDir + "/manual",
            dataDir + "/manual",
            "/E",
            "/MOVE",
            "UNDOEXECUTE",
            "robocopy",
            dataDir + "/manual",
            targetDir + "/manual",
            "/E",
            "/MOVE");
        
        // Move LNCV
        component.addElevatedOperation("Execute",
            "{0,1,8}",
            "robocopy",
            targetDir + "/lncv",
            dataDir + "/lncv",
            "/E",
            "/MOVE",
            "UNDOEXECUTE",
            "robocopy",
            dataDir + "/lncv",
            targetDir + "/lncv",
            "/E",
            "/MOVE");
        
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
