function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var clientExe = targetDir + "/client/traintastic-client.exe";
    
    console.log("Pinning client to taskbar...");
    
    // Windows 10/11: Copy shortcut to taskbar folder
    var appData = installer.value("UserProfile") + "/AppData/Roaming";
    var taskbarDir = appData + "/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar";
    
    // Create the shortcut in taskbar folder
    component.addOperation("CreateShortcut",
        clientExe,
        taskbarDir + "/Traintastic Client.lnk",
        "workingDirectory=" + targetDir + "/client",
        "iconPath=" + clientExe,
        "iconId=0",
        "description=Traintastic Client");
}
