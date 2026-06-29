import { existsSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';

const root = resolve(new URL('..', import.meta.url).pathname);
const bin = {
  gcc: join(root, 'dev-tools/node_modules/@xpack-dev-tools/arm-none-eabi-gcc/.content/bin/arm-none-eabi-gcc'),
  gdb: join(root, 'dev-tools/node_modules/@xpack-dev-tools/arm-none-eabi-gcc/.content/bin/arm-none-eabi-gdb'),
  cmake: join(root, 'dev-tools/node_modules/@xpack-dev-tools/cmake/.content/bin/cmake'),
  ninja: join(root, 'dev-tools/node_modules/@xpack-dev-tools/ninja-build/.content/bin/ninja'),
  openocd: join(root, 'dev-tools/node_modules/@xpack-dev-tools/openocd/.content/bin/openocd'),
};

let failed = false;

function run(name, args) {
  const exe = bin[name];
  if (!existsSync(exe)) {
    console.log(`FAIL ${name}: missing ${exe}`);
    failed = true;
    return;
  }
  const result = spawnSync(exe, args, { encoding: 'utf8' });
  const firstLine = `${result.stdout}${result.stderr}`.split('\n').find(Boolean) ?? '';
  if (result.status !== 0 && name !== 'openocd') {
    console.log(`FAIL ${name}: ${firstLine}`);
    failed = true;
    return;
  }
  console.log(`OK   ${name}: ${firstLine}`);
}

run('gcc', ['--version']);
run('gdb', ['--version']);
run('cmake', ['--version']);
run('ninja', ['--version']);
run('openocd', ['--version']);

const stlinkCfg = join(root, 'dev-tools/node_modules/@xpack-dev-tools/openocd/.content/openocd/scripts/interface/stlink.cfg');
const h7Cfg = join(root, 'dev-tools/node_modules/@xpack-dev-tools/openocd/.content/openocd/scripts/target/stm32h7x.cfg');
console.log(`${existsSync(stlinkCfg) ? 'OK' : 'FAIL'}   openocd stlink.cfg`);
console.log(`${existsSync(h7Cfg) ? 'OK' : 'FAIL'}   openocd stm32h7x.cfg`);
if (!existsSync(stlinkCfg) || !existsSync(h7Cfg)) {
  failed = true;
}

process.exit(failed ? 1 : 0);

