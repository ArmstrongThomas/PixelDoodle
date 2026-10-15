/** Local operator command; this is deliberately not an HTTP upload endpoint. */
import {open} from 'node:fs/promises';
import {openRepository} from './repository.mjs';
import {MAX_PNG_BYTES} from './png.mjs';
const [database,path,artist,title,...tags]=process.argv.slice(2);
let repo,file;
try{
 if(!database||!path||!artist||!title)throw Error('Usage: node ingest.mjs <database> <PNG> <existing-artist-id> <title> [tags...]');
 file=await open(path,'r');const stat=await file.stat();if(!stat.isFile()||stat.size>MAX_PNG_BYTES)throw Error('Expected a regular PNG file no larger than 2 MiB');
 const bytes=Buffer.alloc(MAX_PNG_BYTES+1);let size=0;
 while(size<bytes.length){const part=await file.read(bytes,size,bytes.length-size,null);if(!part.bytesRead)break;size+=part.bytesRead;}
 if(size>MAX_PNG_BYTES)throw Error('PNG exceeds 2 MiB');
 repo=openRepository(database);console.log(JSON.stringify(repo.ingest(bytes.subarray(0,size),{artist,title,tags})));
}catch(e){console.error(e.message);process.exitCode=1;}finally{await file?.close();repo?.close();}
