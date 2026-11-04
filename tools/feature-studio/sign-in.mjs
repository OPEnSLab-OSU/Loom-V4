import {spawn} from 'node:child_process';
import {findCodex} from './codex.mjs';
const executable=await findCodex();
if(!executable){console.error('Install Codex CLI first: npm install -g @openai/codex\nThen run this sign-in launcher again.');process.exitCode=1;}
else {
  const child=spawn(executable.command,[...executable.prefix,'login',...process.argv.slice(2)],{stdio:'inherit',shell:false,windowsHide:true});
  child.on('error',error=>{console.error(error.message);process.exitCode=1;});
  child.on('close',code=>{process.exitCode=code??1;});
}
