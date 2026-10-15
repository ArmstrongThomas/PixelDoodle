import {openRepository} from './repository.mjs';
const [command,path,...extra]=process.argv.slice(2);
if(!['init','seed-demo'].includes(command)||!path||extra.length){console.error('Usage: node database.mjs init|seed-demo <database-path>');process.exitCode=1;}
else {let repo;try{repo=openRepository(path);if(command==='seed-demo')repo.seedDemo();console.log(command==='init'?'Gallery database initialized.':'Synthetic fixtures committed.');}catch(e){console.error(e.message);process.exitCode=1;}finally{repo?.close();}}
