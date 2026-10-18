import os
import sys
import json
import urllib.request
import urllib.error
import subprocess

TOKEN = os.environ.get("GITHUB_TOKEN", "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")
OWNER = "KimSokun"
REPO = "K2DynamicIsland"
TAG = "v1.0.0"

HEADERS = {
    "Authorization": f"token {TOKEN}",
    "User-Agent": "K2-AutoPublisher",
    "Accept": "application/vnd.github.v3+json"
}

def create_private_repo():
    print(f"[*] 1/4 Checking/Creating private repository '{OWNER}/{REPO}'...")
    url = f"https://api.github.com/repos/{OWNER}/{REPO}"
    req = urllib.request.Request(url, headers=HEADERS)
    try:
        with urllib.request.urlopen(req) as resp:
            print("  [+] Repository already exists on GitHub.")
            return True
    except urllib.error.HTTPError as e:
        if e.code == 404:
            print("  [*] Creating new private repository...")
            create_url = "https://api.github.com/user/repos"
            payload = json.dumps({
                "name": REPO,
                "private": True,
                "description": "K2 Dynamic Island - Windows Desktop Capsule Application"
            }).encode("utf-8")
            create_req = urllib.request.Request(create_url, data=payload, headers=HEADERS, method="POST")
            try:
                with urllib.request.urlopen(create_req) as c_resp:
                    print("  [+] Successfully created private repository!")
                    return True
            except Exception as ce:
                print(f"  [-] Failed to create repository: {ce}")
                return False
        else:
            print(f"  [-] HTTP error: {e}")
            return False

def push_git():
    print("[*] 2/4 Pushing source code to GitHub repository...")
    repo_url = f"https://{OWNER}:{TOKEN}@github.com/{OWNER}/{REPO}.git"
    
    # Set remote origin
    subprocess.run(["git", "remote", "set-url", "origin", repo_url], check=False)
    
    res = subprocess.run(["git", "push", "-u", "origin", "main", "--force"], capture_output=True, text=True)
    if res.returncode == 0:
        print("  [+] Successfully pushed branch 'main' to GitHub!")
        return True
    else:
        print(f"  [-] Git push output: {res.stderr}")
        return False

def create_release():
    print(f"[*] 3/4 Creating GitHub Release '{TAG}'...")
    url = f"https://api.github.com/repos/{OWNER}/{REPO}/releases"
    
    # Check if release already exists
    tag_url = f"https://api.github.com/repos/{OWNER}/{REPO}/releases/tags/{TAG}"
    req_tag = urllib.request.Request(tag_url, headers=HEADERS)
    try:
        with urllib.request.urlopen(req_tag) as resp:
            data = json.loads(resp.read().decode())
            print(f"  [+] Release {TAG} already exists (ID: {data['id']})")
            return data
    except urllib.error.HTTPError:
        pass

    release_body = (
        "## K2 Dynamic Island v1.0.0 Official Release\n\n"
        "### Key Features:\n"
        "- 🏝️ **Dynamic Island Capsule**: Sleek, modern floating capsule for Windows.\n"
        "- 🔄 **Secure Automatic Updates**: Built-in background update check & verified Authenticode installer flow.\n"
        "- 🛡️ **Anti-Tamper & Security**: Hardened frontend assets, disabled DevTools, and anti-debug protection.\n"
        "- 🎵 **Audio Activity Mini-Equalizer**: Real-time WASAPI audio wave animation synced with Spotify, YouTube & system media.\n"
        "- 📂 **File Drop Zone**: Seamless file drag-and-drop queue.\n"
        "- 🎨 **Backdrop Themes**: Lively motion, Photo presets, and Glass blur.\n\n"
        "### Installation:\n"
        "Download and run `K2DynamicIsland-Setup.exe`."
    )

    payload = json.dumps({
        "tag_name": TAG,
        "name": f"K2 Dynamic Island {TAG}",
        "body": release_body,
        "draft": False,
        "prerelease": False
    }).encode("utf-8")

    req = urllib.request.Request(url, data=payload, headers=HEADERS, method="POST")
    try:
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode())
            print(f"  [+] Release created successfully (ID: {data['id']})!")
            return data
    except Exception as e:
        print(f"  [-] Failed to create release: {e}")
        return None

def upload_installer(release_data):
    if not release_data:
        return False
        
    upload_url_template = release_data.get("upload_url", "")
    # upload_url is in format: https://uploads.github.com/repos/KimSokun/K2DynamicIsland/releases/123/assets{?name,label}
    upload_base = upload_url_template.split("{")[0]
    
    installer_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "dist", "K2DynamicIsland-Setup.exe")
    if not os.path.exists(installer_path):
        print(f"  [-] Installer not found at: {installer_path}")
        return False

    file_size = os.path.getsize(installer_path)
    file_name = "K2DynamicIsland-Setup.exe"
    print(f"[*] 4/4 Uploading '{file_name}' ({file_size} bytes) to GitHub Release...")

    # Check if asset already exists in release
    for asset in release_data.get("assets", []):
        if asset.get("name") == file_name:
            print(f"  [!] Asset '{file_name}' already attached to this release.")
            return True

    upload_url = f"{upload_base}?name={file_name}"
    headers = {
        "Authorization": f"token {TOKEN}",
        "User-Agent": "K2-AutoPublisher",
        "Content-Type": "application/octet-stream",
        "Content-Length": str(file_size)
    }

    with open(installer_path, "rb") as f:
        file_data = f.read()

    req = urllib.request.Request(upload_url, data=file_data, headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req) as resp:
            asset_info = json.loads(resp.read().decode())
            print(f"  [+] Upload complete! Download URL: {asset_info.get('browser_download_url')}")
            return True
    except Exception as e:
        print(f"  [-] Failed to upload asset: {e}")
        return False

def main():
    if not create_private_repo():
        return 1
    if not push_git():
        return 1
    release = create_release()
    if not upload_installer(release):
        return 1
    print("\n[SUCCESS] All tasks completed automatically!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
