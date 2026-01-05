function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var serverExe = targetDir + "/server/traintastic-server.exe";
    
    console.log("Adding firewall rule for WLANmaus/Z21...");
    
    component.addElevatedOperation("Execute",
        "{0,1}",
        "netsh", "advfirewall", "firewall", "add", "rule",
        "name=Traintastic server (WLANmaus/Z21)",
        "dir=in",
        "program=" + serverExe,
        "protocol=UDP",
        "localport=21105",
        "action=allow",
        "UNDOEXECUTE",
        "netsh", "advfirewall", "firewall", "delete", "rule",
        "name=Traintastic server (WLANmaus/Z21)");
}
