const child_process = require('node:child_process');

const tasks = {
  build: () => {
    const cmds = [];
    let version;
    if (process.env.GITHUB_REF_TYPE === 'tag') {
      version = process.env.GITHUB_REF_NAME;
    } else if (process.env.GITHUB_REF_NAME) {
      // workflow github
      version = `0.${new Date().getTime()}.0-dev`;
    }

    if (version) {
      cmds.push(`npm version "${version}"`);
    }

    cmds.push('npm build');

    return cmds.join(' && ');
  },
  /** Publish */
  publish: () => {
    const registry = process.env.NPM_PUSH_REGISTRY || 'https://npm.pkg.github.com/';
    const cmds = [
      // Remove devDependencies in npm package
      `node ./node_modules/json/lib/json -I -f ./package.json -e "this.devDependencies={};this.scripts={};this.publishConfig['@aegenet:registry']='${registry}';"`,
      `npm publish --@aegenet:registry=${registry}${process.env.NPM_PUBLISH_PUBLIC === '1' ? ' --access public' : '' }`
    ];
    return cmds.join(' && ');
  },
}

const taskMode = process.argv[2];
if (taskMode && taskMode in tasks) {
  console.log(`[BUILD-FLOW] ${taskMode} starting...`);
  const task = tasks[taskMode];

  const cmd = task();
  if (cmd) {
    child_process.execSync(cmd, {
      env: process.env,
      cwd: process.cwd(),
      stdio: 'inherit',
    });
  }
  console.log(`[BUILD-FLOW] ${taskMode} finished.`)
} else {
  console.log(`[BUILD-FLOW] invalid task ${taskMode} provided.`);
  process.exit(1);
}
