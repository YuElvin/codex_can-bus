import { createHash } from 'node:crypto';
import { createWriteStream, existsSync, mkdirSync, readFileSync, rmSync } from 'node:fs';
import { get } from 'node:https';
import { dirname, join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';

const root = resolve(new URL('..', import.meta.url).pathname);
const devTools = join(root, 'dev-tools');
const cacheDir = join(devTools, '.cache');
const packages = [
  '@xpack-dev-tools/arm-none-eabi-gcc',
  '@xpack-dev-tools/cmake',
  '@xpack-dev-tools/ninja-build',
  '@xpack-dev-tools/openocd',
];

function platformKey() {
  const os = process.platform;
  const arch = process.arch === 'arm64' ? 'arm64' : 'x64';
  return `${os}-${arch}`;
}

function download(url, dest) {
  return new Promise((resolveDownload, reject) => {
    const request = get(url, response => {
      if (response.statusCode >= 300 && response.statusCode < 400 && response.headers.location) {
        download(response.headers.location, dest).then(resolveDownload, reject);
        return;
      }
      if (response.statusCode !== 200) {
        reject(new Error(`HTTP ${response.statusCode} for ${url}`));
        return;
      }
      const out = createWriteStream(dest);
      response.pipe(out);
      out.on('finish', () => out.close(resolveDownload));
    });
    request.on('error', reject);
  });
}

function sha256(path) {
  return createHash('sha256').update(readFileSync(path)).digest('hex');
}

for (const packageName of packages) {
  const packageDir = join(devTools, 'node_modules', ...packageName.split('/'));
  const packageJson = JSON.parse(readFileSync(join(packageDir, 'package.json'), 'utf8'));
  const binarySpec = packageJson.xpack?.binaries;
  const platform = binarySpec?.platforms?.[platformKey()];
  if (!platform) {
    throw new Error(`${packageName} has no binary for ${platformKey()}`);
  }

  const archive = join(cacheDir, platform.fileName);
  const contentDir = join(packageDir, binarySpec.destination ?? '.content');
  const url = `${binarySpec.baseUrl}/${platform.fileName}`;

  mkdirSync(cacheDir, { recursive: true });
  mkdirSync(dirname(contentDir), { recursive: true });

  if (!existsSync(archive) || sha256(archive) !== platform.sha256) {
    console.log(`Downloading ${packageName} ${packageJson.version}`);
    await download(url, archive);
  }

  const actual = sha256(archive);
  if (actual !== platform.sha256) {
    throw new Error(`${platform.fileName} sha256 mismatch: ${actual}`);
  }

  rmSync(contentDir, { recursive: true, force: true });
  mkdirSync(contentDir, { recursive: true });

  const strip = String(binarySpec.skip ?? 0);
  const result = spawnSync('tar', ['-xzf', archive, `--strip-components=${strip}`, '-C', contentDir], {
    stdio: 'inherit',
  });
  if (result.status !== 0) {
    throw new Error(`Failed to extract ${platform.fileName}`);
  }
  console.log(`Installed ${packageName} -> ${contentDir}`);
}

