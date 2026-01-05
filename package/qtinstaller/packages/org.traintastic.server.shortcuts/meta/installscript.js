function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var serverExe = targetDir + "/server/traintastic-server.exe";
    
    console.log("Creating server desktop shortcut...");
    
    component.addOperation("CreateShortcut",
        serverExe,
        "@DesktopDir@/Traintastic Server.lnk",
        "workingDirectory=" + targetDir + "/server",
        "iconPath=" + serverExe,
        "iconId=0",
        "description=Start Traintastic Server");
}
