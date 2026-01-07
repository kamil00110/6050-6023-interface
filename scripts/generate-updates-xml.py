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
    """Fetch recent successful workflow runs from GitHub Actions"""
    url = f"https://api.github.com/repos/{repo}/actions/runs?per_page=50&status=success"
    headers = {}
    if token:
        headers["Authorization"] = f"token {token}"
    
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req) as response:
            data = json.loads(response.read().decode())
            # Filter for successful 'Build' workflow runs with artifacts
            successful_runs = []
            for run in data.get('workflow_runs', []):
                if (run['conclusion'] == 'success' and 
                    run['name'] == 'Build' and 
                    len(successful_runs) < limit):
                    successful_runs.append(run)
            return successful_runs
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
        print(f"Error fetching artifacts for run {run_id}: {e}", file=sys.stderr)
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
        installer_size = 0
        for asset in release.get('assets', []):
            name_lower = asset['name'].lower()
            if (name_lower.endswith('.exe') and 
                ('setup' in name_lower or 'installer' in name_lower or 'traintastic' in name_lower)):
                installer_url = asset['browser_download_url']
                installer_size = asset.get('size', 0)
                break
        
        if not installer_url:
            print(f"Warning: No installer found for release {version}", file=sys.stderr)
            continue
            
        builds.append({
            'version': version,
            'date': release['published_at'][:10],
            'installer_url': installer_url,
            'installer_size': installer_size,
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
            name_lower = artifact['name'].lower()
            if 'qtinstaller' in name_lower or 'package-qtinstaller' in name_lower:
                installer_artifact = artifact
                break
        
        if not installer_artifact:
            print(f"Warning: No installer artifact found for run {run_id}", file=sys.stderr)
            continue
        
        # GitHub artifact download URL (requires authentication)
        # NOTE: These URLs require a GitHub token to download
        # The maintenance tool would need to handle this authentication
        artifact_download_url = f"https://github.com/{repo}/actions/runs/{run_id}/artifacts/{installer_artifact['id']}"
        
        builds.append({
            'version': version,
            'date': run['created_at'][:10],
            'installer_url': artifact_download_url,
            'installer_size': installer_artifact.get('size_in_bytes', 0),
            'run_id': run_id,
            'artifact_id': installer_artifact['id'],
            'commit': commit_sha,
            'type': 'dev'
        })
    
    return builds

def generate_updates_xml(builds, output_file, app_name="Traintastic"):
    """Generate Updates.xml from build information"""
    
    if not builds:
        print("No builds to process", file=sys.stderr)
        # Create empty but valid Updates.xml
        builds = []
    
    root = Element('Updates')
    
    app_name_elem = SubElement(root, 'ApplicationName')
    app_name_elem.text = app_name
    
    if builds:
        app_version = SubElement(root, 'ApplicationVersion')
        app_version.text = builds[0]['version']
    
    checksum = SubElement(root, 'Checksum')
    checksum.text = 'false'
    
    for build in builds:
        pkg = SubElement(root, 'PackageUpdate')
        
        name = SubElement(pkg, 'Name')
        name.text = 'org.traintastic.installer'
        
        display_name = SubElement(pkg, 'DisplayName')
        if build['type'] == 'dev':
            display_name.text = f"{app_name} {build['version']}"
        else:
            display_name.text = f"{app_name} {build['version']}"
        
        desc = SubElement(pkg, 'Description')
        if build['type'] == 'dev':
            desc.text = f"Development build from commit {build.get('commit', 'unknown')} - Run ID: {build.get('run_id', 'N/A')}"
        else:
            desc.text = f"Release version {build['version']}"
        
        version = SubElement(pkg, 'Version')
        version.text = build['version']
        
        release_date = SubElement(pkg, 'ReleaseDate')
        release_date.text = build['date']
        
        update_file = SubElement(pkg, 'UpdateFile', {
            'UncompressedSize': str(build.get('installer_size', 0)),
            'OS': 'win'
        })
        update_file.text = build['installer_url']
        
        # Add download instructions for dev builds
        if build['type'] == 'dev':
            instructions = SubElement(pkg, 'DownloadInstructions')
            instructions.text = f"Download from GitHub Actions: https://github.com/{build.get('repo', '')}/actions/runs/{build.get('run_id', '')}"
    
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
        # Add repo to builds for download instructions
        for build in builds:
            build['repo'] = args.repo
        app_name = "Traintastic (Developer)"
    
    if not builds:
        print("WARNING: No builds found!", file=sys.stderr)
    
    generate_updates_xml(builds, args.output, app_name)
    print("Done!")

if __name__ == "__main__":
    main()
