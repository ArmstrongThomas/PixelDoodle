/** Internal moderation/notification ledger. No HTTP or outbound network calls. */
import {randomUUID} from 'node:crypto';
export const moderationSchema=`
 CREATE TABLE moderation_queue(
  artwork TEXT PRIMARY KEY REFERENCES artworks(id),
  notification TEXT NOT NULL DEFAULT 'queued' CHECK(notification IN ('queued','sending','uncertain','delivered')),
  attempt TEXT, message_id TEXT, delivered_at INTEGER, due_at INTEGER,
  revision INTEGER NOT NULL DEFAULT 0);
 CREATE TABLE moderation_audit(
  id INTEGER PRIMARY KEY, artwork TEXT NOT NULL REFERENCES artworks(id),
  actor TEXT NOT NULL, action TEXT NOT NULL, reason TEXT NOT NULL, at INTEGER NOT NULL);
 CREATE INDEX moderation_due ON moderation_queue(due_at);
 INSERT INTO moderation_queue(artwork) SELECT id FROM artworks WHERE status='pending' AND synthetic=0;
`;
const fail=code=>{throw Object.assign(new Error(code),{code});};
export function moderationService(db,{isModerator,now=()=>Date.now()}={}){
 if(typeof isModerator!=='function')throw Error('Trusted moderator validator required');
 function transaction(fn){db.exec('BEGIN IMMEDIATE');try{const value=fn();db.exec('COMMIT');return value;}catch(e){db.exec('ROLLBACK');throw e;}}
 function staff(actor){if(typeof actor!=='string'||!actor||actor.length>128||isModerator(actor)!==true)fail('unauthorized');}
 function row(id){return db.prepare('SELECT q.*,a.status FROM moderation_queue q JOIN artworks a ON a.id=q.artwork WHERE q.artwork=?').get(id);}
 function audit(id,actor,action,reason=''){db.prepare('INSERT INTO moderation_audit(artwork,actor,action,reason,at) VALUES (?,?,?,?,?)').run(id,actor,action,reason,now());}
 function publish(id){db.prepare("UPDATE artworks SET status='published',visibility='public',publishedAt=? WHERE id=?").run(new Date(now()).toISOString(),id);}
 return {
  list(actor){staff(actor);return db.prepare("SELECT q.artwork,q.notification,q.delivered_at,q.due_at,q.revision,a.title,a.status,a.width,a.height FROM moderation_queue q JOIN artworks a ON a.id=q.artwork WHERE a.status IN ('pending','held') ORDER BY a.rowid LIMIT 100").all();},
  // Called only by a future trusted outbox worker, never a user-supplied receipt.
  claim(){return transaction(()=>{const item=db.prepare("SELECT q.artwork FROM moderation_queue q JOIN artworks a ON a.id=q.artwork WHERE q.notification='queued' AND a.status='pending' ORDER BY a.rowid LIMIT 1").get();if(!item)return null;const attempt=randomUUID();db.prepare("UPDATE moderation_queue SET notification='sending',attempt=? WHERE artwork=?").run(attempt,item.artwork);return {...item,attempt};});},
  delivered(id,attempt,messageId){if(typeof messageId!=='string'||!/^\d{1,32}$/.test(messageId))fail('invalid_receipt');return transaction(()=>{const r=row(id);if(!r||r.attempt!==attempt||r.notification!=='sending')return false;const at=now();db.prepare("UPDATE moderation_queue SET notification='delivered',message_id=?,delivered_at=?,due_at=?,revision=revision+1 WHERE artwork=?").run(messageId,at,r.status==='pending'?at+8*3600000:null,id);audit(id,'system:notification','delivered');return true;});},
  uncertain(id,attempt){return transaction(()=>{const r=row(id);if(!r||r.attempt!==attempt||r.notification!=='sending')return false;db.prepare("UPDATE moderation_queue SET notification='uncertain',due_at=NULL,revision=revision+1 WHERE artwork=?").run(id);audit(id,'system:notification','uncertain');return true;});},
  // A restarted worker must not replay an indeterminate outbound send.
  recoverInterrupted(){return transaction(()=>{const rows=db.prepare("SELECT artwork FROM moderation_queue WHERE notification='sending'").all();for(const r of rows){db.prepare("UPDATE moderation_queue SET notification='uncertain',due_at=NULL,revision=revision+1 WHERE artwork=?").run(r.artwork);audit(r.artwork,'system:notification','interrupted');}return rows.length;});},
  decide(actor,id,action,expectedRevision,reason=''){
   staff(actor);if(!['approve','deny','hold'].includes(action)||!Number.isSafeInteger(expectedRevision)||expectedRevision<0||typeof reason!=='string'||reason.length>500||/[\x00-\x1f\x7f]/.test(reason))fail('invalid_decision');
   return transaction(()=>{const r=row(id);if(!r||r.revision!==expectedRevision||!['pending','held'].includes(r.status))fail('stale_review');
    if(action==='approve')publish(id);else db.prepare('UPDATE artworks SET status=?,visibility=\'private\' WHERE id=?').run(action==='deny'?'denied':'held',id);
    db.prepare('UPDATE moderation_queue SET due_at=NULL,revision=revision+1 WHERE artwork=?').run(id);audit(id,actor,action,reason.trim());return {status:action==='approve'?'published':action==='deny'?'denied':'held'};
   });
  },
  autoapprove(){return transaction(()=>{const rows=db.prepare("SELECT q.artwork FROM moderation_queue q JOIN artworks a ON a.id=q.artwork WHERE q.notification='delivered' AND q.due_at<=? AND a.status='pending' LIMIT 100").all(now());for(const r of rows){publish(r.artwork);db.prepare('UPDATE moderation_queue SET due_at=NULL,revision=revision+1 WHERE artwork=?').run(r.artwork);audit(r.artwork,'system:autoapproval','approve');}return rows.length;});}
 };
}
