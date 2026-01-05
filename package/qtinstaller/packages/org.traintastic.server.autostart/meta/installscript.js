function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var serverExe = targetDir + "/server/traintastic-server.exe";
    
    console.log("Adding server to Windows startup...");
    
    component.addOperation("GlobalConfig",
        "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        "Traintastic Server",
        '"' + serverExe + '" --tray');
}
