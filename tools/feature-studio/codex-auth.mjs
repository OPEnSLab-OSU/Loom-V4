import {spawn,execFile} from 'node:child_process';

// Only report sign-in state. CLI output can contain an API key or OAuth URL.
// Never send that output to the web app, project files, or saved run history.
export class CodexAuth {
  constructor(executable,{spawnProcess=spawn,execute=execFile,timeoutMs=300000}={}) {
    this.executable=executable;this.spawnProcess=spawnProcess;this.execute=execute;this.timeoutMs=timeoutMs;
    this.child=null;this.state={available:!!executable,signedIn:false,status:'unknown',message:'Check your Codex sign-in.'};
  }
  snapshot(){return {...this.state};}
  async check() {
    if(this.child)return this.snapshot();
    if(!this.executable)return this.state={available:false,signedIn:false,status:'missing',message:'Install Codex CLI, then check again.'};
    return new Promise(resolve=>this.execute(this.executable.command,[...this.executable.prefix,'login','status'],{windowsHide:true,timeout:10000,maxBuffer:64000},(error,stdout,stderr)=>{
      const signedIn=!error&&/logged in/i.test(stdout+'\n'+stderr);
      this.state={available:true,signedIn,status:signedIn?'signed-in':error?.killed?'unknown':'signed-out',message:signedIn?'Signed in to Codex. Ready to assemble.':error?.killed?'Codex took too long to reply. Check again.':'Sign in to Codex to assemble applications.'};
      resolve(this.snapshot());
    }));
  }
  start() {
    if(this.child)return this.snapshot();
    if(!this.executable)throw new Error('Install Codex CLI, then choose Check sign-in.');
    const child=this.spawnProcess(this.executable.command,[...this.executable.prefix,'login'],{windowsHide:true,shell:false,stdio:['ignore','pipe','pipe']});
    this.child=child;this.state={available:true,signedIn:false,status:'signing-in',message:'Complete sign-in in the browser window opened by Codex.'};
    // Consume without retaining credentials or auth URLs.
    child.stdout?.resume();child.stderr?.resume();
    let failed=false;
    child.on('error',()=>{failed=true;this.state={available:true,signedIn:false,status:'failed',message:'Could not start sign-in. Use sign-in.cmd or sh sign-in.sh in the Studio folder.'};});
    const timer=setTimeout(()=>{this.cancel();this.state.message='Sign-in timed out. Try again, or use the sign-in launcher.';},this.timeoutMs);timer.unref?.();
    child.once('close',async()=>{clearTimeout(timer);if(this.child!==child)return;this.child=null;if(!failed)await this.check();});
    return this.snapshot();
  }
  cancel(){const child=this.child;this.child=null;child?.kill();this.state={available:!!this.executable,signedIn:false,status:'cancelled',message:'Sign-in stopped. You can try again.'};return this.snapshot();}
}
