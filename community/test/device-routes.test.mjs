import http from 'node:http';
import test from 'node:test';import assert from 'node:assert/strict';import {DatabaseSync} from 'node:sqlite';
import {createServer} from '../server.mjs';import {pairingService} from '../pairing.mjs';
const origin='https://pixels.7db.pw';
async function setup(enabled=true){
 const db=new DatabaseSync(':memory:');let now=100000;
 const pairing=pairingService(db,{now:()=>now,isAccountActive:id=>id==='owner'});
 const server=createServer(undefined,{origin,pairing,enableDevicePairing:enabled});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const call=(path,body,headers={},method='POST')=>new Promise((resolve,reject)=>{
  const req=http.request({hostname:'127.0.0.1',port:server.address().port,path,method,agent:false,headers:{host:'pixels.7db.pw','content-type':'application/json',...headers}},res=>{const chunks=[];res.on('data',b=>chunks.push(b));res.on('end',()=>resolve({status:res.statusCode,headers:res.headers,body:JSON.parse(Buffer.concat(chunks))}));});req.on('error',reject);req.end(JSON.stringify(body));
 });
 return {db,pairing,call,advance:()=>{now+=60000;},close:async()=>{await new Promise(r=>server.close(r));db.close();}};
}
test('native HTTP pairing requires approval, exchanges once and respects revocation',async()=>{
 const t=await setup();try{
  const begin=await t.call('/api/device/pair/begin',{label:'DS Lite',requestedScopes:['gallery:read']});assert.equal(begin.status,200);assert.equal(begin.headers['cache-control'],'no-store');assert.equal(begin.body.verificationUri,origin+'/#account');
  const p=begin.body;assert.equal((await t.call('/api/device/pair/poll',{deviceCode:p.deviceCode})).body.error,'authorization_pending');
  assert.equal((await t.call('/api/device/pair/poll',{deviceCode:p.deviceCode})).body.error,'slow_down');
  t.pairing.decide('owner',p.userCode,true);t.advance();
  const grant=(await t.call('/api/device/pair/poll',{deviceCode:p.deviceCode})).body;
  assert.equal(t.pairing.authenticate(grant.accessToken,'gallery:read').accountId,'owner');assert.equal(t.pairing.authenticate(grant.accessToken,'artworks:submit'),null);
  assert.equal((await t.call('/api/device/pair/poll',{deviceCode:p.deviceCode})).body.error,'expired_token');
  t.pairing.revoke('owner',grant.deviceId);assert.equal(t.pairing.authenticate(grant.accessToken,'gallery:read'),null);
  const denied=(await t.call('/api/device/pair/begin',{label:'Denied'})).body;t.pairing.decide('owner',denied.userCode,false);assert.equal((await t.call('/api/device/pair/poll',{deviceCode:denied.deviceCode})).body.error,'access_denied');
 }finally{await t.close();}
});
test('native pairing HTTP rejects unsafe input and cannot evade peer rate limits',async()=>{
 const off=await setup(false);try{assert.equal((await off.call('/api/device/pair/begin',{label:'DS'})).status,503);}finally{await off.close();}
 const t=await setup();try{
  const path='/api/device/pair/begin';
  assert.equal((await t.call(path,{label:'DS'},{host:'evil.test'})).status,403);
  assert.equal((await t.call(path,{label:'DS'},{origin:'https://evil.test'})).status,403);
  assert.equal((await t.call(path,{}, {},'GET')).status,405);
  assert.equal((await t.call(path,{label:'DS'},{'content-type':'text/plain'})).status,415);
  assert.equal((await t.call(path,{label:'x'.repeat(1100)})).status,413);
  assert.equal((await t.call(path,{label:'DS',subject:'spoofed'})).status,400);
  assert.equal((await t.call(path+'?deviceCode=secret',{label:'DS'})).status,404);
  assert.equal((await t.call('/api/device/pair/poll',{deviceCode:'bad'})).status,400);
  assert.equal((await t.call(path,{label:'DS',requestedScopes:['admin']})).status,400);
  for(let i=0;i<4;i++)assert.equal((await t.call(path,{label:'DS'},{'x-forwarded-for':`192.0.2.${i}`,'cf-connecting-ip':`192.0.2.${i}`})).status,200);
  assert.equal((await t.call(path,{label:'DS'},{'x-real-ip':'192.0.2.99'})).status,429);
 }finally{await t.close();}
});

