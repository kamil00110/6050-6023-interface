function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var clientExe = targetDir + "/client/traintastic-client.exe";
    
    console.log("Creating client desktop shortcut...");
    
    component.addOperation("CreateShortcut",
        clientExe,
        "@DesktopDir@/Traintastic Client.lnk",
        "workingDirectory=" + targetDir + "/client",
        "iconPath=" + clientExe,
        "iconId=0",
        "description=Start Traintastic Client");
}
