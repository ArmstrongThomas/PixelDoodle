/** Explicit HTTPS-behind-local-proxy staging launcher. No secrets have defaults. */
import {DatabaseSync} from 'node:sqlite';import {resolve,dirname} from 'node:path';import {mkdirSync} from 'node:fs';
import {createWebAuth} from './auth.mjs';import {pairingService} from './pairing.mjs';import {openRepository} from './repository.mjs';import {createServer} from './server.mjs';
const required=name=>{const value=process.env[name];if(!value)throw Error(`Missing ${name}`);return value;};
const origin=required('AUTH_ORIGIN'),secret=required('AUTH_SECRET'),authPath=resolve(required('AUTH_DATABASE')),galleryPath=resolve(required('GALLERY_DATABASE'));
if(authPath===galleryPath)throw Error('Auth and gallery databases must be separate');
const discord={clientId:required('DISCORD_CLIENT_ID'),clientSecret:required('DISCORD_CLIENT_SECRET')};
mkdirSync(dirname(authPath),{recursive:true});const db=new DatabaseSync(authPath);db.exec('PRAGMA journal_mode=WAL; PRAGMA busy_timeout=5000;');
const web=createWebAuth({database:db,baseURL:origin,secret,discord});
if(process.argv.includes('--migrate')){await web.migrate();db.close();console.log('Auth schema migrated.');}
else{
 // Inspect schema before listening; normal startup never silently migrates auth.
 db.prepare('SELECT id FROM user LIMIT 1').get();
 const pairing=pairingService(db,{isAccountActive:id=>db.prepare('SELECT emailVerified FROM user WHERE id=?').get(id)?.emailVerified===1});
 const repository=openRepository(galleryPath),port=Number(process.env.PORT||4179);
 if(!Number.isInteger(port)||port<1||port>65535)throw Error('Invalid PORT');
 const server=createServer(repository,{auth:web.auth,origin:new URL(origin).origin,providers:['discord'],pairing,enableDevicePairing:process.env.ENABLE_DEVICE_PAIRING==='true'});
 server.on('close',()=>{repository.close();db.close();});server.listen(port,'127.0.0.1',()=>console.log(`Configured staging origin listening on loopback port ${port}`));
 server.on('error',()=>{console.error('Could not start auth staging server');process.exitCode=1;});
}
