#!/usr/bin/env python3
import json
import sys
from xml.etree.ElementTree import Element, SubElement, tostring
from xml.dom import minidom
import requests
import argparse

def get_github_releases(repo):
    """Fetch releases from GitHub API"""
    url = f"https://api.github.com/repos/{repo}/releases"
    response = requests.get(url)
    return response.json()

def get_artifact_info(run_id, repo, token):
    """Fetch artifact info from GitHub Actions"""
    url = f"https://api.github.com/repos/{repo}/actions/runs/{run_id}/artifacts"
    headers = {"Authorization": f"token {token}"}
    response = requests.get(url, headers=headers)
    return response.json()

def generate_updates_xml(builds, output_file):
    """Generate Updates.xml from build information"""
    
    root = Element('Updates')
    
    app_name = SubElement(root, 'ApplicationName')
    app_name.text = 'Traintastic'
    
    checksum = SubElement(root, 'Checksum')
    checksum.text = 'false'
    
    for build in builds:
        # Create PackageUpdate for server
        pkg = SubElement(root, 'PackageUpdate')
        SubElement(pkg, 'Name').text = 'org.traintastic.server'
        SubElement(pkg, 'DisplayName').text = 'Traintastic Server'
        SubElement(pkg, 'Version').text = build['version']
        SubElement(pkg, 'ReleaseDate').text = build['date']
        
        update_file = SubElement(pkg, 'UpdateFile', {
            'CompressedSize': str(build['server_size']),
            'UncompressedSize': str(build['server_size_uncompressed']),
            'OS': 'win'
        })
        update_file.text = build['server_url']
        
        SubElement(pkg, 'DownloadableArchives').text = build['server_filename']
    
    # Pretty print XML
    rough_string = tostring(root, 'utf-8')
    reparsed = minidom.parseString(rough_string)
    
    with open(output_file, 'w') as f:
        f.write(reparsed.toprettyxml(indent="  "))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--type', choices=['release', 'dev'], required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--repo', default='reinder/traintastic')
    args = parser.parse_args()
    
    if args.type == 'release':
        releases = get_github_releases(args.repo)
        builds = process_releases(releases)
    else:
        # Process dev builds
        builds = process_dev_builds(args.repo)
    
    generate_updates_xml(builds, args.output)
