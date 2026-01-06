function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var serverExe = targetDir + "/server/traintastic-server.exe";
    
    console.log("Adding firewall rules for Traintastic client connections...");
    
    // TCP rule
    component.addElevatedOperation("Execute",
        "{0,1}",
        "netsh", "advfirewall", "firewall", "add", "rule",
        "name=Traintastic server (TCP)",
        "dir=in",
        "program=" + serverExe,
        "protocol=TCP",
        "localport=5740",
        "action=allow",
        "UNDOEXECUTE",
        "cmd", "/c",
        "netsh advfirewall firewall delete rule \"name=Traintastic server (TCP)\" >nul 2>&1 || exit 0");
    
    // UDP rule
    component.addElevatedOperation("Execute",
        "{0,1}",
        "netsh", "advfirewall", "firewall", "add", "rule",
        "name=Traintastic server (UDP)",
        "dir=in",
        "program=" + serverExe,
        "protocol=UDP",
        "localport=5740",
        "action=allow",
        "UNDOEXECUTE",
        "cmd", "/c",
        "netsh advfirewall firewall delete rule \"name=Traintastic server (UDP)\" >nul 2>&1 || exit 0");
}
