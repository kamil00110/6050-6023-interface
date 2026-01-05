function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var manualPath = "C:/ProgramData/traintastic/manual/en/index.html";
    
    console.log("Creating manual shortcuts...");
    
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
