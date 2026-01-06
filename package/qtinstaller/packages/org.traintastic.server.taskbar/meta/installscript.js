function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var serverExe = targetDir + "/server/traintastic-server.exe";
    
    console.log("Creating taskbar shortcut for server...");
    
    // Get actual user profile path
    var userProfile = installer.value("UserProfile");
    if (!userProfile || userProfile === "") {
        userProfile = installer.value("HomeDir");
    }
    
    var taskbarDir = userProfile + "/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar";
    
    console.log("Taskbar directory: " + taskbarDir);
    
    // Ensure taskbar directory exists
    component.addOperation("Mkdir", taskbarDir);
    
    // Create the shortcut in taskbar folder
    component.addOperation("CreateShortcut",
        serverExe,
        taskbarDir + "/Traintastic Server.lnk",
        "workingDirectory=" + targetDir + "/server",
        "iconPath=" + serverExe,
        "iconId=0",
        "description=Traintastic Server");
}
