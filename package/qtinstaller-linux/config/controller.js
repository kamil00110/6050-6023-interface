function Controller() {
    // Enable verbose installer output for debugging
    installer.setValue("verbose", "true");
    
    // Check if this is an update/maintenance run
    if (installer.isInstaller()) {
        installer.autoRejectMessageBoxes();
        installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes);
        installer.setMessageBoxAutomaticAnswer("stopProcessesForUpdates", QMessageBox.Ignore);
    }
}

Controller.prototype.IntroductionPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        if (installer.isInstaller()) {
            widget.title = "Welcome to Traintastic Installer";
            widget.MessageLabel.setText("This will install Traintastic on your computer.");
        } else if (installer.isUninstaller()) {
            widget.title = "Uninstall Traintastic";
            widget.MessageLabel.setText("This will remove Traintastic from your computer.");
        } else if (installer.isUpdater()) {
            widget.title = "Update Traintastic";
            widget.MessageLabel.setText("This will update Traintastic to the latest version.");
        }
    }
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        widget.TargetDirectoryLineEdit.setText(installer.value("TargetDir"));
    }
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        widget.selectAll();
    }
}

Controller.prototype.FinishedPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        if (installer.isInstaller() || installer.isUpdater()) {
            widget.RunItCheckBox.visible = true;
            widget.RunItCheckBox.checked = false;
        } else if (installer.isUninstaller()) {
            widget.RunItCheckBox.visible = false;
        }
    }
}
