function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var clientExe = targetDir + "/client/traintastic-client.exe";
    
    console.log("Creating taskbar shortcut for client...");
    
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
        clientExe,
        taskbarDir + "/Traintastic Client.lnk",
        "workingDirectory=" + targetDir + "/client",
        "iconPath=" + clientExe,
        "iconId=0",
        "description=Traintastic Client");
}
