function Controller() {
    installer.autoRejectMessageBoxes();
    installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes);
    installer.setMessageBoxAutomaticAnswer("stopProcessesForUpdates", QMessageBox.Ignore);
    
    // Add custom component selection page
    installer.addWizardPage(component, "ComponentSelectionPage", QInstaller.TargetDirectory);
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    var page = gui.pageWidgetByObjectName("DynamicComponentSelectionPage");
    page.entered.connect(this, Controller.prototype.ComponentSelectionPageEntered);
}

Controller.prototype.ComponentSelectionPageEntered = function() {
    var page = gui.currentPageWidget();
    if (page != null) {
        page.title = qsTr("Select Components");
        page.subTitle = qsTr("Select which components to install");
    }
}

Controller.prototype.IntroductionPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget != null) {
        widget.title = qsTr("Welcome to Traintastic Installer");
        widget.MessageLabel.setText(qsTr("This will install Traintastic on your computer."));
    }
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    gui.currentPageWidget().TargetDirectoryLineEdit.setText(installer.value("TargetDir"));
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    var widget = gui.currentPageWidget();
    widget.selectAll();
}

Controller.prototype.ReadyForInstallationPageCallback = function() {
    gui.currentPageWidget().showAll();
}

Controller.prototype.FinishedPageCallback = function() {
    var checkBoxForm = gui.currentPageWidget().LaunchQtCreatorCheckBoxForm;
    if (checkBoxForm && installer.isInstaller()) {
        checkBoxForm.launchQtCreatorCheckBox.text = qsTr("Launch Traintastic Client");
        checkBoxForm.launchQtCreatorCheckBox.checked = false;
    }
}
