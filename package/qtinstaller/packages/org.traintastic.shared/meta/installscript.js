function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();
        
        var commonAppData = installer.value("CommonAppDataDir");
        if (!commonAppData || commonAppData === "") {
            commonAppData = "C:/ProgramData";
        }
        
        var dataDir = commonAppData + "/traintastic";
        var targetDir = installer.value("TargetDir");
        
        console.log("Target directory: " + targetDir);
        console.log("ProgramData directory: " + dataDir);
        
        // Create main data directory
        component.addOperation("Mkdir", dataDir);
        
        // Move files to ProgramData during install/update
        if (installer.isInstaller() || installer.isUpdater()) {
            
            // Translations directory
            var translationsSource = targetDir + "/translations";
            var translationsDest = dataDir + "/translations";
            
            console.log("Moving translations from: " + translationsSource);
            console.log("Moving translations to: " + translationsDest);
            
            if (systemInfo.kernelType === "winnt") {
                // Use robocopy on Windows (exit codes 0-7 are success)
                component.addElevatedOperation("Execute",
                    "{0,1,2,3,4,5,6,7}",
                    "robocopy",
                    translationsSource.replace(/\//g, "\\"),
                    translationsDest.replace(/\//g, "\\"),
                    "/E", "/IS", "/IT", "/NFL", "/NDL", "/NJH", "/NJS",
                    "ERRORMESSAGE", "Failed to copy translations directory");
                
                // Delete source after successful copy
                component.addOperation("Execute",
                    "cmd", "/c",
                    "rmdir", "/s", "/q", translationsSource.replace(/\//g, "\\"));
                    
                // Manual directory
                var manualSource = targetDir + "/manual";
                var manualDest = dataDir + "/manual";
                
                console.log("Moving manual from: " + manualSource);
                console.log("Moving manual to: " + manualDest);
                
                component.addElevatedOperation("Execute",
                    "{0,1,2,3,4,5,6,7}",
                    "robocopy",
                    manualSource.replace(/\//g, "\\"),
                    manualDest.replace(/\//g, "\\"),
                    "/E", "/IS", "/IT", "/NFL", "/NDL", "/NJH", "/NJS",
                    "ERRORMESSAGE", "Failed to copy manual directory");
                
                component.addOperation("Execute",
                    "cmd", "/c",
                    "rmdir", "/s", "/q", manualSource.replace(/\//g, "\\"));
                
                // LNCV directory
                var lncvSource = targetDir + "/lncv";
                var lncvDest = dataDir + "/lncv";
                
                console.log("Moving lncv from: " + lncvSource);
                console.log("Moving lncv to: " + lncvDest);
                
                component.addElevatedOperation("Execute",
                    "{0,1,2,3,4,5,6,7}",
                    "robocopy",
                    lncvSource.replace(/\//g, "\\"),
                    lncvDest.replace(/\//g, "\\"),
                    "/E", "/IS", "/IT", "/NFL", "/NDL", "/NJH", "/NJS",
                    "ERRORMESSAGE", "Failed to copy lncv directory");
                
                component.addOperation("Execute",
                    "cmd", "/c",
                    "rmdir", "/s", "/q", lncvSource.replace(/\//g, "\\"));
            }
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
                console.log("Deleting old translation: " + oldTranslations[i]);
                component.addOperation("Delete", oldTranslations[i]);
            }
        }
        
        // Migration: delete old DLLs from server directory
        var oldDlls = [
            targetDir + "/server/lua53.dll",
            targetDir + "/server/lua54.dll",
            targetDir + "/server/archive.dll",
            targetDir + "/server/zlib1.dll"
        ];
        
        for (var i = 0; i < oldDlls.length; i++) {
            if (installer.fileExists(oldDlls[i])) {
                console.log("Deleting old DLL: " + oldDlls[i]);
                component.addOperation("Delete", oldDlls[i]);
            }
        }
        
    } catch (e) {
        console.log("Error in shared component: " + e);
    }
}
