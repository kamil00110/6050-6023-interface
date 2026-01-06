function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var serverExe = targetDir + "/server/traintastic-server.exe";
    
    console.log("Pinning server to taskbar...");
    
    // Windows 10/11: Copy shortcut to taskbar folder
    var appData = installer.value("UserProfile") + "/AppData/Roaming";
    var taskbarDir = appData + "/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar";
    
    // Create the shortcut in taskbar folder
    component.addOperation("CreateShortcut",
        serverExe,
        taskbarDir + "/Traintastic Server.lnk",
        "workingDirectory=" + targetDir + "/server",
        "iconPath=" + serverExe,
        "iconId=0",
        "description=Traintastic Server");
}
