function Component() {
    // Constructor - shared component is always installed
}

Component.prototype.createOperations = function() {
    try {
        // Call default implementation to extract files
        component.createOperations();
        
        var targetDir = installer.value("TargetDir");
        var homeDir = installer.value("HomeDir");
        var dataDir = homeDir + "/.local/share/traintastic";
        
        console.log("Moving shared data from: " + targetDir);
        console.log("Moving shared data to: " + dataDir);
        
        // Ensure traintastic data directory exists
        component.addOperation("Mkdir", dataDir);
        
        // Move translations from TargetDir to ~/.local/share/traintastic
        component.addOperation("CopyDirectory",
            targetDir + "/share/traintastic/translations",
            dataDir + "/translations");
        
        // Move manual
        component.addOperation("CopyDirectory",
            targetDir + "/share/traintastic/manual",
            dataDir + "/manual");
        
        // Move LNCV
        component.addOperation("CopyDirectory",
            targetDir + "/share/traintastic/lncv",
            dataDir + "/lncv");
        
        // Remove the copied directories from install directory
        component.addOperation("Rmdir",
            targetDir + "/share/traintastic/translations");
        
        component.addOperation("Rmdir",
            targetDir + "/share/traintastic/manual");
        
        component.addOperation("Rmdir",
            targetDir + "/share/traintastic/lncv");
        
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
        
    } catch (e) {
        console.log("Error in shared component createOperations: " + e);
        console.log("Stack: " + e.stack);
    }
}
