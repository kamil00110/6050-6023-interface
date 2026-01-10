# Traintastic Update Repository

This directory contains update metadata for the Traintastic Qt Installer Framework.

## Channels

- **Release** (`release/Updates.xml`): Stable releases from GitHub Releases
- **Developer** (`developer/Updates.xml`): Latest development builds from GitHub Actions

## Usage

Update your `config.xml` with:

```xml
<RemoteRepositories>
    <Repository>
        <Url>https://kamil00110.github.io/6050-6023-interface/repository/release</Url>
        <DisplayName>Release Builds (Stable)</DisplayName>
        <Enabled>1</Enabled>
    </Repository>
    <Repository>
        <Url>https://kamil00110.github.io/6050-6023-interface/repository/developer</Url>
        <DisplayName>Developer Builds (Latest)</DisplayName>
        <Enabled>0</Enabled>
    </Repository>
</RemoteRepositories>
```

## Note on Developer Builds

Developer build downloads require GitHub authentication as they come from Actions artifacts.
Users should download installers manually from the Actions tab or enable authentication in the maintenance tool.

---
Generated on: $(date -u +"%Y-%m-%d %H:%M:%S UTC")
Branch: qt-installer
Commit: ba2b1763e671dce3a214cf834712fcf136589cf6
