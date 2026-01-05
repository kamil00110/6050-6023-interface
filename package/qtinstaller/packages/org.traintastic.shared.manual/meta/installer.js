function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    // Get ProgramData directory dynamically
    var commonAppData = installer.value("CommonAppDataDir");
    if (!commonAppData || commonAppData === "") {
        commonAppData = "C:/ProgramData";
    }
    
    var manualPath = commonAppData + "/traintastic/manual/en/index.html";
    
    console.log("Creating manual shortcuts...");
    console.log("Manual path: " + manualPath);
    
    // Start menu shortcut
    component.addOperation("CreateShortcut",
        manualPath,
        "@StartMenuDir@/Traintastic Manual.lnk",
        "iconPath=%SystemRoot%\\System32\\shell32.dll",
        "iconId=23",
        "description=Open Traintastic Manual");
    
    // Desktop shortcut
    component.addOperation("CreateShortcut",
        manualPath,
        "@DesktopDir@/Traintastic Manual.lnk",
        "iconPath=%SystemRoot%\\System32\\shell32.dll",
        "iconId=23",
        "description=Open Traintastic Manual");
}
