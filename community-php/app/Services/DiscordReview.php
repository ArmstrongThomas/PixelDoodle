<?php
namespace App\Services;
use App\Models\User;
use Illuminate\Support\Facades\{Cache,Http};
final class DiscordReview {
 public function __construct(private GalleryStore $store){}
 public function moderation():Moderation{return new Moderation($this->store,fn($id)=>User::whereKey($id)->where('is_moderator',true)->where('disabled',false)->whereNotNull('email_verified_at')->exists(),fn()=>(int)floor(microtime(true)*1000));}
 public function row(string $id):?array{return $this->store->run('SELECT a.*,p.name AS artist_name,d.description,d.submitted_at,q.message_id,q.revision,q.due_at,q.synced_revision FROM artworks a JOIN artists p ON p.id=a.artist LEFT JOIN artwork_details d ON d.artwork=a.id JOIN moderation_queue q ON q.artwork=a.id WHERE a.id=?',[$id])->fetch()?:null;}
 public function payload(array $a):array {
  $pending=in_array($a['status'],['pending','held'],true);$base=rtrim(config('app.url'),'/');
  $deadline=$a['due_at']?'Autoapproval <t:'.intdiv($a['due_at'],1000).':R>':($a['status']==='pending'?'Awaiting confirmed delivery; timer not started.':'No autoapproval timer.');
  $audit=$this->store->run("SELECT actor,action,reason,at FROM moderation_audit WHERE artwork=? AND action IN ('approve','deny','hold') ORDER BY id DESC LIMIT 1",[$a['id']])->fetch();
  $fields=[['name'=>'Artist','value'=>mb_substr($a['artist_name'],0,80).' · [Profile]('.$base.'/#artist/'.$a['artist'].')'],['name'=>'Canvas / status','value'=>$a['width'].' × '.$a['height'].' · '.$a['status']],['name'=>'Review','value'=>$deadline]];
  if($audit){$name=str_starts_with($audit['actor'],'system:')?'Automatic approval':(User::find($audit['actor'])?->name??'Moderator');$fields[]=['name'=>'Last decision','value'=>mb_substr($name,0,80).' · '.$audit['action'].' · <t:'.intdiv($audit['at'],1000).':f>'.($audit['reason']?"\n".mb_substr($audit['reason'],0,500):'')];}
  $embed=['title'=>mb_substr($a['title'],0,120),'description'=>mb_substr($a['description']??'',0,2000),'url'=>$base.'/#review','color'=>0xcbff45,'fields'=>$fields,'image'=>['url'=>'attachment://artwork.png']];
  if($a['submitted_at'])$embed['timestamp']=gmdate('c',intdiv($a['submitted_at'],1000));
  $buttons=[];foreach(['approve'=>['Approve',3],'deny'=>['Reject',4],'hold'=>['Hold',2]] as $action=>[$label,$style])$buttons[]=['type'=>2,'style'=>$style,'label'=>$label,'custom_id'=>'pd:'.$action.':'.$a['id'].':'.$a['revision'],'disabled'=>!$pending];
  return ['allowed_mentions'=>['parse'=>[]],'embeds'=>[$embed],'components'=>[['type'=>1,'components'=>$buttons]]];
 }
 private function http(){return Http::withToken(config('discord.token'),'Bot')->acceptJson()->connectTimeout(5)->timeout(10)->withOptions(['allow_redirects'=>false]);}
 private function endpoint():string{return 'https://discord.com/api/v10/channels/'.config('discord.channel_id').'/messages';}
 public function tick():array {
  if(!config('discord.enabled')||!config('discord.token')||!preg_match('/^\d{1,32}$/D',(string)config('discord.channel_id')))throw new \RuntimeException('Configure the Discord bot before starting the worker.');
  $lock=Cache::lock('discord-review-tick',600);if(!$lock->get())return ['busy'=>true];
  try{
   $m=$this->moderation();$m->recoverInterrupted();$published=$m->autoapprove();$sent=0;$uncertain=0;$synced=0;
   for($i=0;$i<10;$i++){
    $claim=$m->claim();if(!$claim)break;$a=$this->row($claim['artwork']);
    // Predict the delivery revision; a concurrent decision will make this stale,
    // and the sync pass below replaces it with the authoritative state.
    $a['revision']++;$payload=$this->payload($a);
    try{$r=$this->http()->attach('files[0]',$a['image'],'artwork.png',['Content-Type'=>'image/png'])->post($this->endpoint(),['payload_json'=>json_encode($payload,JSON_THROW_ON_ERROR)]);
     $receipt=$r->json('id');if($r->successful()&&is_string($receipt)&&preg_match('/^\d{1,32}$/D',$receipt)){$m->delivered($a['id'],$claim['attempt'],$receipt);$sent++;}
     elseif($r->status()===429){$delay=min(3600,max(5,(int)ceil((float)$r->json('retry_after',60))));$this->store->run("UPDATE moderation_queue SET notification='queued',retry_at=? WHERE artwork=? AND attempt=? AND notification='sending'",[(int)floor(microtime(true)*1000)+$delay*1000,$a['id'],$claim['attempt']]);break;}
     else{$m->uncertain($a['id'],$claim['attempt']);$uncertain++;}
    }catch(\Throwable){$m->uncertain($a['id'],$claim['attempt']);$uncertain++;}
   }
   $rows=$this->store->run('SELECT artwork FROM moderation_queue WHERE message_id IS NOT NULL AND synced_revision<>revision ORDER BY rowid LIMIT 20')->fetchAll();
   foreach($rows as $row){$a=$this->row($row['artwork']);try{$r=$this->http()->patch($this->endpoint().'/'.$a['message_id'],$this->payload($a));if($r->successful()){$this->store->run('UPDATE moderation_queue SET synced_revision=? WHERE artwork=? AND revision=?',[$a['revision'],$a['id'],$a['revision']]);$synced++;}else break;}catch(\Throwable){break;}}
   return compact('sent','uncertain','published','synced');
  }finally{$lock->release();}
 }
}
