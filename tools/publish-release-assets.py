"""Publish prepared release assets after the source commit and tag are on GitHub."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from urllib.error import URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from backend.application_updates import ApplicationUpdates, select_release
from backend.version import APP_VERSION


REPOSITORY = 'mianxiu/151500px-menu'
API = 'https://api.github.com/repos/' + REPOSITORY
UPLOAD = 'https://uploads.github.com/repos/' + REPOSITORY
TAG = 'v' + APP_VERSION


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def github_token():
    credentials = subprocess.check_output(
        ['git', 'credential', 'fill'],
        input=f'protocol=https\nhost=github.com\npath={REPOSITORY}.git\n\n'.encode(),
        env={**os.environ, 'GIT_TERMINAL_PROMPT': '0', 'GCM_INTERACTIVE': 'Never'},
        timeout=10,
    )
    fields = dict(line.split(b'=', 1) for line in credentials.splitlines() if b'=' in line)
    return fields[b'password'].decode('ascii')


def github(token, endpoint, *, method=None, payload=None, binary=None):
    if not endpoint.startswith((API + '/', UPLOAD + '/')):
        raise ValueError('Unexpected GitHub API destination')
    headers = {'User-Agent': '151500px-menu-release', 'Accept': 'application/vnd.github+json',
               'X-GitHub-Api-Version': '2022-11-28'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    if payload is not None:
        binary = json.dumps(payload).encode()
        headers['Content-Type'] = 'application/json'
    elif binary is not None:
        headers['Content-Type'] = 'application/octet-stream'
    with urlopen(Request(endpoint, data=binary, headers=headers, method=method), timeout=30) as response:
        raw = response.read()
    return json.loads(raw) if raw else None


def check_asset(asset, name, contents):
    digest = hashlib.sha256(contents).hexdigest()
    if (asset['name'], asset['state'], asset['size'], asset.get('digest')) != (name, 'uploaded', len(contents), 'sha256:' + digest):
        raise RuntimeError(f'GitHub asset identity or hash mismatch: {name}')
    return asset


def attach_files(token, release_id, files):
    list_url = f'{API}/releases/{release_id}/assets?per_page=100'
    upload_url = f'{UPLOAD}/releases/{release_id}/assets'
    existing = {asset['name']: asset for asset in github(token, list_url)}

    def attach(item):
        name, contents = item
        started = time.monotonic()
        asset = existing.get(name)
        if asset is None:
            print(f'Uploading {name} ({len(contents):,} bytes)', flush=True)
            try:
                asset = github(token, upload_url + '?' + urlencode({'name': name}), binary=contents)
            except (URLError, TimeoutError):
                # The upload may have succeeded before the response was lost.
                asset = next((candidate for candidate in github(token, list_url) if candidate['name'] == name), None)
                if asset is None or asset.get('state') != 'uploaded':
                    raise
        verified = check_asset(asset, name, contents)
        print(f'Verified {name} in {time.monotonic() - started:.1f}s', flush=True)
        return verified

    with ThreadPoolExecutor(max_workers=min(3, len(files))) as pool:
        return list(pool.map(attach, files.items()))


def publish(directory, notes):
    backend = f'151500px-menu-Backend-{APP_VERSION}.zip'
    checksum = backend + '.sha256'
    plugin = f'151500px-menu-UXP-{APP_VERSION}.zip'
    manifest = '151500px-menu-update.json'
    local = {name: (directory / name).read_bytes() for name in (backend, checksum, plugin)}
    expected_checksum = f'{hashlib.sha256(local[backend]).hexdigest()}  {backend}'
    if local[checksum].decode('ascii').strip() != expected_checksum:
        raise ValueError('Prepared backend archive differs from its checksum')
    token = github_token()
    commit = git('rev-parse', f'{TAG}^{{commit}}').decode().strip()
    for ref in ('heads/master', f'tags/{TAG}'):
        if github(token, f'{API}/git/ref/{ref}')['object']['sha'] != commit:
            raise RuntimeError('Push the matching source commit and lightweight tag before attaching assets')
    releases = github(token, API + '/releases?per_page=100')
    release = next((item for item in releases if item['tag_name'] == TAG), None)
    if release is None:
        release = github(token, API + '/releases', payload={
            'tag_name': TAG, 'name': APP_VERSION, 'target_commitish': commit, 'body': notes, 'draft': True,
        })
    release_id = release['id']
    if release['draft']:
        assets = attach_files(token, release_id, local)
        download_base = f'https://github.com/{REPOSITORY}/releases/download/{TAG}/'
        assets = [{**asset, 'browser_download_url': download_base + asset['name']} for asset in assets]
        candidate = select_release({**release, 'draft': False, 'assets': assets}, APP_VERSION)
        if candidate['version'] != APP_VERSION:
            raise ValueError('Update manifest version differs from the prepared release')
        document = ApplicationUpdates._release_document(candidate)
        local[manifest] = (json.dumps(document, ensure_ascii=False, indent=2) + '\n').encode()
        (directory / manifest).write_bytes(local[manifest])
        attach_files(token, release_id, {manifest: local[manifest]})
        github(token, f'{API}/releases/{release_id}', method='PATCH',
               payload={'draft': False, 'make_latest': 'true'})
    else:
        local[manifest] = (directory / manifest).read_bytes()
    latest = github(None, API + '/releases/latest')
    if latest['id'] != release_id or {asset['name'] for asset in latest['assets']} != local.keys():
        raise RuntimeError('Published release or public asset list does not match the prepared version')
    for asset in latest['assets']:
        check_asset(asset, asset['name'], local[asset['name']])
    (directory / 'public-release.json').write_text(json.dumps(latest, ensure_ascii=False, indent=2), 'utf-8')
    print(latest['html_url'], flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='Folder with the prepared backend ZIP, checksum and UXP ZIP')
    parser.add_argument('--notes-file', type=Path, help='Optional release notes written before publishing')
    args = parser.parse_args()
    publish(args.directory, args.notes_file.read_text('utf-8') if args.notes_file else '')
