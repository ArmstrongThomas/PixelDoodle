/** Internal pairing state machine, not an HTTP/OAuth provider. RFC 8628-inspired.
 * Callers must supply authenticated account IDs and trusted rate-limit subjects.
 */
import {randomBytes,randomUUID,createHash} from 'node:crypto';
const hash=s=>createHash('sha256').update(s).digest('hex');
const scopes=new Set(['gallery:read','favorites:write','artworks:submit']);
const token=()=>randomBytes(32).toString('base64url');
const code=()=>{const alphabet='ABCDEFGHJKLMNPQRSTUVWXYZ23456789';return [...randomBytes(8)].map(b=>alphabet[b&31]).join('');};
const normalize=s=>typeof s==='string'?s.toUpperCase().replace(/[- ]/g,''):'';
const error=code=>Object.assign(new Error(code),{code});
export function pairingService(db,{isAccountActive,now=()=>Date.now()}={}){
 if(typeof isAccountActive!=='function')throw Error('Trusted account validation is required');
 db.exec(`CREATE TABLE IF NOT EXISTS pairing_requests(
 id TEXT PRIMARY KEY, device_hash TEXT UNIQUE NOT NULL,user_hash TEXT UNIQUE NOT NULL,label TEXT NOT NULL,scopes TEXT NOT NULL,
 expires INTEGER NOT NULL,next_poll INTEGER NOT NULL,interval_ms INTEGER NOT NULL,state TEXT NOT NULL CHECK(state IN ('pending','approved','denied','consumed')),account TEXT);
 CREATE TABLE IF NOT EXISTS device_grants(id TEXT PRIMARY KEY,account TEXT NOT NULL,label TEXT NOT NULL,token_hash TEXT UNIQUE NOT NULL,scopes TEXT NOT NULL,created INTEGER NOT NULL,expires INTEGER NOT NULL,revoked INTEGER NOT NULL DEFAULT 0);
 CREATE TABLE IF NOT EXISTS pairing_limits(subject TEXT PRIMARY KEY,until_ms INTEGER NOT NULL,count INTEGER NOT NULL);
 CREATE INDEX IF NOT EXISTS pairing_expiry ON pairing_requests(expires);
 CREATE INDEX IF NOT EXISTS grant_owner ON device_grants(account);`);
 function transaction(fn){db.exec('BEGIN IMMEDIATE');try{const result=fn();db.exec('COMMIT');return result;}catch(e){db.exec('ROLLBACK');throw e;}}
 function account(id){if(typeof id!=='string'||id.length>128||isAccountActive(id)!==true)throw error('unauthorized');}
 function rate(kind,key,max,period){if(typeof key!=='string'||!key||key.length>256)throw error('invalid_rate_subject');const at=now();const allowed=transaction(()=>{
  db.prepare('DELETE FROM pairing_limits WHERE until_ms<=?').run(at);
  const subject=hash(kind+':'+key),row=db.prepare('SELECT * FROM pairing_limits WHERE subject=?').get(subject);
  if(!row){if(db.prepare('SELECT count(*) AS n FROM pairing_limits').get().n>=10000)return false;db.prepare('INSERT INTO pairing_limits VALUES (?,?,1)').run(subject,at+period);return true;}
  if(row.count>=max)return false;db.prepare('UPDATE pairing_limits SET count=count+1 WHERE subject=?').run(subject);return true;
 });if(!allowed)throw error('rate_limited');}
 function userRequest(value){const c=normalize(value);if(!/^[A-HJ-NP-Z2-9]{8}$/.test(c))return null;return db.prepare("SELECT * FROM pairing_requests WHERE user_hash=? AND expires>? AND state='pending'").get(hash(c),now());}
 return {
  begin({label,requestedScopes=['gallery:read'],subject}={}){
   rate('begin',subject,5,60000);
   if(typeof label!=='string'||!label.trim()||label.length>48||/[\x00-\x1f\x7f]/.test(label)||!Array.isArray(requestedScopes)||!requestedScopes.length||requestedScopes.length>3||requestedScopes.some(s=>!scopes.has(s)))throw error('invalid_request');
   return transaction(()=>{const at=now();db.prepare('DELETE FROM pairing_requests WHERE expires<=?').run(at);if(db.prepare('SELECT count(*) AS n FROM pairing_requests').get().n>=1000)throw error('temporarily_unavailable');
    const device=token(),user=code(),expires=at+600000;db.prepare('INSERT INTO pairing_requests VALUES (?,?,?,?,?,?,?,?,?,NULL)').run(randomUUID(),hash(device),hash(user),label.trim(),JSON.stringify([...new Set(requestedScopes)]),expires,at,5000,'pending');
    return {deviceCode:device,userCode:user.slice(0,4)+'-'+user.slice(4),expiresIn:600,interval:5};
   });
  },
  inspect(accountId,userCode){account(accountId);rate('approve',accountId,20,3600000);const row=userRequest(userCode);if(!row)throw error('invalid_or_expired_code');return {label:row.label,scopes:JSON.parse(row.scopes),expiresAt:row.expires};},
  decide(accountId,userCode,approve){account(accountId);rate('approve',accountId,20,3600000);if(typeof approve!=='boolean')throw error('explicit_decision_required');return transaction(()=>{const row=userRequest(userCode);if(!row)throw error('invalid_or_expired_code');db.prepare('UPDATE pairing_requests SET state=?,account=? WHERE id=?').run(approve?'approved':'denied',accountId,row.id);return {state:approve?'approved':'denied'};});},
  poll(deviceCode,subject){rate('poll',subject,120,60000);if(typeof deviceCode!=='string'||!/^[A-Za-z0-9_-]{43}$/.test(deviceCode))throw error('invalid_device_code');return transaction(()=>{
   const at=now(),row=db.prepare('SELECT * FROM pairing_requests WHERE device_hash=?').get(hash(deviceCode));if(!row||row.expires<=at||row.state==='consumed')return {error:'expired_token'};
   if(at<row.next_poll){const interval=Math.min(60000,row.interval_ms+5000);db.prepare('UPDATE pairing_requests SET next_poll=?,interval_ms=? WHERE id=?').run(at+interval,interval,row.id);return {error:'slow_down',interval:interval/1000};}
   db.prepare('UPDATE pairing_requests SET next_poll=? WHERE id=?').run(at+row.interval_ms,row.id);
   if(row.state==='pending')return {error:'authorization_pending'};
   if(row.state==='denied'||isAccountActive(row.account)!==true)return {error:'access_denied'};
   if(db.prepare('SELECT count(*) AS n FROM device_grants WHERE account=? AND revoked=0 AND expires>?').get(row.account,at).n>=20)return {error:'device_limit'};
   const access=token(),id=randomUUID(),expires=at+30*86400000;
   db.prepare('INSERT INTO device_grants VALUES (?,?,?,?,?,?,?,0)').run(id,row.account,row.label,hash(access),row.scopes,at,expires);
   db.prepare("UPDATE pairing_requests SET state='consumed' WHERE id=?").run(row.id);
   return {accessToken:access,tokenType:'Bearer',expiresIn:30*86400,scopes:JSON.parse(row.scopes),deviceId:id};
  });},
  authenticate(access,scope){if(typeof access!=='string'||!/^[A-Za-z0-9_-]{43}$/.test(access)||!scopes.has(scope))return null;const row=db.prepare('SELECT * FROM device_grants WHERE token_hash=? AND revoked=0 AND expires>?').get(hash(access),now());return row&&isAccountActive(row.account)===true&&JSON.parse(row.scopes).includes(scope)?{accountId:row.account,deviceId:row.id}:null;},
  devices(accountId){account(accountId);return db.prepare('SELECT id,label,scopes,created,expires,revoked FROM device_grants WHERE account=? ORDER BY created DESC').all(accountId).map(r=>({...r,scopes:JSON.parse(r.scopes)}));},
  revoke(accountId,id){account(accountId);return db.prepare('UPDATE device_grants SET revoked=1 WHERE id=? AND account=?').run(id,accountId).changes===1;}
 };
}

