#!/usr/bin/env python3
"""
Generate Updates.xml for Qt Installer Framework from GitHub Releases and Actions
"""

import json
import sys
import os
from xml.etree.ElementTree import Element, SubElement, tostring
from xml.dom import minidom
import argparse
from datetime import datetime
import urllib.request
import urllib.error

def get_github_releases(repo, token=None):
    """Fetch releases from GitHub API"""
    url = f"https://api.github.com/repos/{repo}/releases"
    headers = {}
    if token:
        headers["Authorization"] = f"token {token}"
    
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req) as response:
            return json.loads(response.read().decode())
    except urllib.error.URLError as e:
        print(f"Error fetching releases: {e}", file=sys.stderr)
        return []

def get_workflow_runs(repo, token=None, limit=10):
    """Fetch recent workflow runs from GitHub Actions"""
    url = f"https://api.github.com/repos/{repo}/actions/runs?per_page={limit}"
    headers = {}
    if token:
        headers["Authorization"] = f"token {token}"
    
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req) as response:
            data = json.loads(response.read().decode())
            # Filter for successful builds only
            return [run for run in data.get('workflow_runs', []) 
                   if run['conclusion'] == 'success' and run['name'] == 'Build']
    except urllib.error.URLError as e:
        print(f"Error fetching workflow runs: {e}", file=sys.stderr)
        return []

def get_run_artifacts(repo, run_id, token=None):
    """Fetch artifacts for a specific workflow run"""
    url = f"https://api.github.com/repos/{repo}/actions/runs/{run_id}/artifacts"
    headers = {}
    if token:
        headers["Authorization"] = f"token {token}"
    
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req) as response:
            return json.loads(response.read().decode()).get('artifacts', [])
    except urllib.error.URLError as e:
        print(f"Error fetching artifacts: {e}", file=sys.stderr)
        return []

def process_releases(releases, repo):
    """Process GitHub releases into build info"""
    builds = []
    
    for release in releases:
        if release.get('draft') or release.get('prerelease'):
            continue
            
        version = release['tag_name'].lstrip('v')
        
        # Find Windows installer in assets
        installer_url = None
        for asset in release.get('assets', []):
            if asset['name'].endswith('.exe') and 'setup' in asset['name'].lower():
                installer_url = asset['browser_download_url']
                break
        
        if not installer_url:
            continue
            
        builds.append({
            'version': version,
            'date': release['published_at'][:10],
            'installer_url': installer_url,
            'installer_size': 0,  # Would need to fetch asset info
            'type': 'release'
        })
    
    return builds

def process_dev_builds(repo, token=None):
    """Process GitHub Actions workflow runs into dev build info"""
    builds = []
    
    runs = get_workflow_runs(repo, token, limit=10)
    
    for run in runs:
        run_id = run['id']
        commit_sha = run['head_sha'][:8]
        version = f"0.3.0-dev-{run_id}-{commit_sha}"
        
        artifacts = get_run_artifacts(repo, run_id, token)
        
        # Find Windows installer artifact
        installer_artifact = None
        for artifact in artifacts:
            if 'qtinstaller' in artifact['name'].lower() or 'installer' in artifact['name'].lower():
                installer_artifact = artifact
                break
        
        if not installer_artifact:
            continue
        
        # Note: Artifacts require authentication to download
        # The installer will need to handle this or artifacts need to be published
        builds.append({
            'version': version,
            'date': run['created_at'][:10],
            'installer_url': f"https://github.com/{repo}/actions/runs/{run_id}",
            'installer_size': artifact.get('size_in_bytes', 0),
            'run_id': run_id,
            'commit': commit_sha,
            'type': 'dev'
        })
    
    return builds

def generate_updates_xml(builds, output_file, app_name="Traintastic"):
    """Generate Updates.xml from build information"""
    
    if not builds:
        print("No builds to process", file=sys.stderr)
        return
    
    root = Element('Updates')
    
    app_name_elem = SubElement(root, 'ApplicationName')
    app_name_elem.text = app_name
    
    if builds:
        app_version = SubElement(root, 'ApplicationVersion')
        app_version.text = builds[0]['version']
    
    checksum = SubElement(root, 'Checksum')
    checksum.text = 'false'
    
    for build in builds:
        # Create a simple package update pointing to the full installer
        pkg = SubElement(root, 'PackageUpdate')
        
        name = SubElement(pkg, 'Name')
        name.text = 'org.traintastic.installer'
        
        display_name = SubElement(pkg, 'DisplayName')
        display_name.text = f"{app_name} {build['version']}"
        
        desc = SubElement(pkg, 'Description')
        if build['type'] == 'dev':
            desc.text = f"Development build from commit {build.get('commit', 'unknown')}"
        else:
            desc.text = f"Release version {build['version']}"
        
        version = SubElement(pkg, 'Version')
        version.text = build['version']
        
        release_date = SubElement(pkg, 'ReleaseDate')
        release_date.text = build['date']
        
        # For now, we just provide download info
        # Full component updates would need individual component archives
        update_file = SubElement(pkg, 'UpdateFile', {
            'UncompressedSize': str(build.get('installer_size', 0)),
            'OS': 'win'
        })
        update_file.text = build['installer_url']
    
    # Pretty print XML
    rough_string = tostring(root, 'utf-8')
    reparsed = minidom.parseString(rough_string)
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(output_file) if os.path.dirname(output_file) else '.', exist_ok=True)
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(reparsed.toprettyxml(indent="  "))
    
    print(f"Generated {output_file} with {len(builds)} build(s)")

def main():
    parser = argparse.ArgumentParser(description='Generate Updates.xml for Qt Installer Framework')
    parser.add_argument('--type', choices=['release', 'dev'], required=True,
                       help='Type of builds to process')
    parser.add_argument('--output', required=True,
                       help='Output path for Updates.xml')
    parser.add_argument('--repo', default='reinder/traintastic',
                       help='GitHub repository (owner/name)')
    parser.add_argument('--token', default=os.environ.get('GITHUB_TOKEN'),
                       help='GitHub API token (or set GITHUB_TOKEN env var)')
    
    args = parser.parse_args()
    
    print(f"Processing {args.type} builds for {args.repo}...")
    
    if args.type == 'release':
        releases = get_github_releases(args.repo, args.token)
        builds = process_releases(releases, args.repo)
        app_name = "Traintastic"
    else:
        builds = process_dev_builds(args.repo, args.token)
        app_name = "Traintastic (Developer)"
    
    if not builds:
        print("WARNING: No builds found!", file=sys.stderr)
        # Create empty Updates.xml
        builds = []
    
    generate_updates_xml(builds, args.output, app_name)
    print("Done!")

if __name__ == "__main__":
    main()
