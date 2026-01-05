function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.kernelType !== "winnt") return;
    
    var targetDir = installer.value("TargetDir");
    var clientExe = targetDir + "/client/traintastic-client.exe";
    
    console.log("Pinning client to taskbar...");
    
    var psCommand = '$shell = New-Object -ComObject Shell.Application; ' +
                   '$folder = $shell.Namespace(\\"' + targetDir.replace(/\//g, "\\\\") + '\\\\client\\"); ' +
                   '$item = $folder.ParseName(\\"traintastic-client.exe\\"); ' +
                   '$verb = $item.Verbs() | Where-Object {$_.Name -match \\"Pin to taskbar\\" -or $_.Name -match \\"An Taskleiste\\"}; ' +
                   'if($verb) { $verb.DoIt() }';
    
    component.addOperation("Execute",
        "{0,1}",
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-Command", psCommand);
}
