import http from 'node:http';
import test from 'node:test';import assert from 'node:assert/strict';import {DatabaseSync} from 'node:sqlite';import {randomBytes} from 'node:crypto';
import {createWebAuth} from '../auth.mjs';import {pairingService} from '../pairing.mjs';import {createServer} from '../server.mjs';
async function listen(config){const server=createServer(undefined,config);await new Promise(r=>server.listen(0,'127.0.0.1',r));return {server,url:`http://127.0.0.1:${server.address().port}`};}
test('unconfigured preview exposes status but no authentication or device actions',async()=>{const {server,url}=await listen();try{assert.deepEqual(await(await fetch(url+'/api/account/status')).json(),{enabled:false,providers:[]});assert.equal((await fetch(url+'/api/auth/sign-out',{method:'POST'})).status,503);assert.equal((await fetch(url+'/api/account/devices')).status,503);}finally{await new Promise(r=>server.close(r));}});
test('HTTP bridge verifies session, origin, bounded JSON and ownership',async()=>{const db=new DatabaseSync(':memory:');const origin='https://studio.test';let code;
 const web=createWebAuth({database:db,baseURL:origin,secret:randomBytes(32).toString('hex'),sendEmailCode:async d=>{code=d.otp;}});await web.migrate();
 const pairing=pairingService(db,{isAccountActive:id=>db.prepare('SELECT emailVerified FROM user WHERE id=?').get(id)?.emailVerified===1});const {server,url}=await listen({auth:web.auth,origin,providers:['email'],pairing});
 const transport=(url,options)=>new Promise((resolve,reject)=>{const req=http.request(url,options,res=>{const chunks=[];res.on('data',b=>chunks.push(b));res.on('end',()=>{const headers=new Headers();for(let i=0;i<res.rawHeaders.length;i+=2)headers.append(res.rawHeaders[i],res.rawHeaders[i+1]);resolve(new Response(Buffer.concat(chunks),{status:res.statusCode,headers}));});});req.on('error',reject);req.end(options.body);});
 const call=(path,body,cookie,from=origin)=>transport(url+path,{method:body===undefined?'GET':'POST',headers:{host:'studio.test',origin:from,'content-type':'application/json',...(cookie?{cookie}:{})},...(body===undefined?{}:{body:JSON.stringify(body)})});
 try{assert.equal((await call('/api/account/devices')).status,401);assert.equal((await fetch(url+'/api/account/status')).status,403);
 await call('/api/auth/email-otp/send-verification-otp',{email:'one@example.test',type:'sign-in'});let r=await call('/api/auth/sign-in/email-otp',{email:'one@example.test',otp:code});assert.equal(r.status,200);const cookie=r.headers.getSetCookie().map(c=>c.split(';')[0]).join('; ');const user=(await r.json()).user;
 const p=pairing.begin({label:'Test handheld',subject:'test'});r=await call('/api/account/pair/inspect',{code:p.userCode,accountId:'attacker'},cookie);assert.equal(r.status,200);assert.equal((await r.json()).label,'Test handheld');
 assert.equal((await call('/api/account/pair/decide',{code:p.userCode,approve:true},cookie,'https://evil.test')).status,403);
 r=await call('/api/account/pair/decide',{code:p.userCode,approve:true,accountId:'attacker'},cookie);assert.equal(r.status,200);const g=pairing.poll(p.deviceCode,'test');assert.equal(pairing.authenticate(g.accessToken,'gallery:read').accountId,user.id);
 r=await call('/api/account/devices',undefined,cookie);assert.equal((await r.json()).devices.length,1);
 assert.equal((await call('/api/account/devices/revoke',{id:g.deviceId,padding:'x'.repeat(9000)},cookie)).status,413);
 r=await call('/api/account/devices/revoke',{id:g.deviceId},cookie);assert.equal((await r.json()).revoked,true);assert.equal(pairing.authenticate(g.accessToken,'gallery:read'),null);
 await call('/api/auth/sign-out',{},cookie);assert.equal((await call('/api/account/devices',undefined,cookie)).status,401);
 }finally{await new Promise(r=>server.close(r));db.close();}
});
