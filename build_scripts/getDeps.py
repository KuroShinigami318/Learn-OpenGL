import argparse
import json
import concurrent.futures
import requests
import io
import zipfile
import os
import shutil

class ReposManager:

    def __init__(self, api_key, owner):
        self.api_key = api_key
        self.owner = owner
        self.error = None

    def getRepo(self, repo_name, folder_name, tag):
        headers = {'Accept': 'application/vnd.github+json', 'Authorization': f'Bearer {self.api_key}', 'X-GitHub-Api-Version': '2022-11-28'}
        url = f'https://api.github.com/repos/{self.owner}/{repo_name}/zipball/{tag}'
        check = requests.get('https://api.github.com/user/repos', headers = headers)
        lib_dir = f'{os.curdir}/libs/{folder_name}'
        if check.ok:
            r = requests.get(url, headers=headers)
            if not r.ok:
                self.error = f'\n{r.status_code}: {r.reason}. \nURL={url}'
            r.raise_for_status()
            if os.path.exists(lib_dir):
                shutil.rmtree(lib_dir)
            z = zipfile.ZipFile(io.BytesIO(r.content))
            temp, = zipfile.Path(z).iterdir()
            z.extractall(path=f'{os.curdir}/libs/')
            z.close()
            os.rename(f'{os.curdir}/libs/{temp.name}', lib_dir)
        else:
            self.error = "Invalid token"
        check.raise_for_status()

    def traverseAllDep(self, libs_dep_relative_path):
        f = open(libs_dep_relative_path)
        data = json.load(f)
        f.close()
        pool = concurrent.futures.ThreadPoolExecutor(max_workers=5)
        clone_tasks = {}
        for repo in data['libs']:
            clone_tasks[pool.submit(self.getRepo, repo['prj_name'], repo['name'], repo['tag'])] = repo['prj_name']

        for future in concurrent.futures.as_completed(clone_tasks):
            repo = clone_tasks[future]
            try:
                data = future.result()
            except Exception as exc:
                if self.error is None:
                    print(exc)
                else:
                    print(f'failed to clone repo: {repo}. {self.error}')
            else:
                print(f'Successfully clone repo: {repo}')

def main():
    parser = argparse.ArgumentParser(description='')
    parser.add_argument('api_key')
    parser.add_argument('owner')
    parser.add_argument('libs_dep_relative_path')
    args = parser.parse_args()
    reposManager = ReposManager(args.api_key, args.owner)
    reposManager.traverseAllDep(args.libs_dep_relative_path)

if __name__ == "__main__":
    main()